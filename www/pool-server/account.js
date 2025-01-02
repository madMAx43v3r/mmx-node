
const mongoose = require('mongoose');
const axios = require("axios");
const dbs = require('./schema.js');
const utils = require('./utils.js');
const config = require('./config.js');

var db = null;
var vdf_height = null;
var sync_height = null;
var balance_lock = false;

const api_token_header = {
    headers: {'x-api-token': config.api_token}
};

async function query_height()
{
    try {
        vdf_height = await utils.get_vdf_height();
        sync_height = await utils.get_synced_height();
        if(!sync_height) {
            console.log('Waiting for node sync ...');
        }
    } catch(e) {
        console.log("Failed to query sync height:", e.message);
    }
}

async function update_account(address, reward, pool_share, points, count, height, opt)
{
    const account = await dbs.Account.findOne({address: address});
    if(account) {
        if(reward > 0) {
            account.balance += reward;
        }
        account.pool_share = pool_share;
        account.points_rate = points / config.share_window;
        account.partial_rate = count / config.share_window_hours;
        account.last_update = height;
        await account.save(opt);
    }
}

var update_lock = false;

async function update()
{
    if(!sync_height) {
        return;
    }
    if(update_lock) {
        return;
    }
    while(balance_lock) {
        await utils.sleep(1000);
    }
    update_lock = true;
    balance_lock = true;
    try {
        const conn = await db.startSession();
        try {
            await conn.startTransaction();
            const opt = {session: conn};

            const start_height = vdf_height - config.share_window;

            const result = await dbs.Partial.aggregate([
                {$match: {pending: false, valid: true, height: {$gte: start_height}}},
                {$group: {_id: "$account", points: {$sum: "$points"}, count: {$sum: 1}}}
            ]);

            const distribution = await dbs.Partial.aggregate([
                {$match: {pending: false, valid: true, height: {$gte: start_height}}},
                {$group: {_id: "$height", points: {$sum: "$points"}, count: {$sum: 1}}}
            ]);

            const errors = await dbs.Partial.aggregate([
                {$match: {pending: false, valid: false, height: {$gte: start_height}}},
                {$group: {_id: "$error_code", points: {$sum: "$points"}, count: {$sum: 1}}}
            ]);

            let uptime = 0;
            let reward_enable = false;
            {
                const blocks_per_hour = 3600 * 1000 / config.block_interval;
                
                const map = new Map();
                for(const entry of distribution) {
                    const hour = Math.floor((entry._id - start_height) / blocks_per_hour);
                    map.set(hour, true);
                }
                uptime = Math.min(map.size, config.share_window_hours);
                reward_enable = (uptime > config.share_window_hours / 2);
            }

            let total_points = 0;
            let total_partials = 0;
            for(const entry of result) {
                total_points += entry.points;
                total_partials += entry.count;
            }

            let total_points_failed = 0;
            for(const entry of errors) {
                total_points_failed += entry.points;
            }

            let pool = await dbs.Pool.findOne({id: "this"});
            if(!pool) {
                pool = new dbs.Pool({id: "this"});
            }
            pool.farmers = result.length;
            pool.points_rate = total_points / config.share_window;
            pool.partial_rate = total_partials / config.share_window_hours;

            const partial_errors = {};
            for(const entry of errors) {
                partial_errors[entry._id] = {
                    points: entry.points,
                    count: entry.count,
                    share: entry.points / (total_points + total_points_failed)
                };
            }
            pool.partial_errors = partial_errors;
            pool.last_update = vdf_height;
            await pool.save(opt);

            let total_rewards = 0;

            if(reward_enable) {
                // Find all blocks that are pending and have been confirmed enough
                const blocks = await dbs.Block.find({
                    pending: true,
                    height: {$lte: sync_height - config.account_delay}
                });

                for(const block of blocks) {
                    const res = await axios.get(config.node_url + '/wapi/header?height=' + block.height, api_token_header);
                    const valid = (res.data.hash === block.hash);
                    if(valid) {
                        total_rewards += block.reward_value;
                    }
                    block.valid = valid;
                    block.pending = false;
                    await block.save(opt);

                    if(valid) {
                        console.log("Farmed block", block.height, "reward", block.reward_value, "MMX", "account", block.account);
                    } else {
                        console.log("Invalid block", block.height);
                    }
                }
            }
            var fee_value = 0;
            
            // Take pool fee first
            if(total_rewards) {
                fee_value = total_rewards * config.pool_fee;
                total_rewards -= fee_value;

                let account = await dbs.Account.findOne({address: config.fee_account});
                if(!account) {
                    account = new dbs.Account({address: config.fee_account});
                }
                account.balance += fee_value;
                await account.save(opt);
            }

            // Distribute rewards and update account stats
            let res = [];
            for(const entry of result) {
                const pool_share = entry.points / total_points;
                const reward_share = total_rewards * pool_share;
                res.push(update_account(
                    entry._id, reward_share, pool_share, entry.points, entry.count, vdf_height, opt));
            }
            await Promise.all(res);

            await conn.commitTransaction();

            if(total_rewards) {
                console.log(
                    "Distributed", total_rewards, "MMX to", result.length, "accounts",
                    "fee", fee_value, "MMX", "total_points", total_points, "total_partials", total_partials);
            }
            console.log(
                "vdf_height", vdf_height, "points_rate", pool.points_rate, "partial_rate", pool.partial_rate, "farmers", pool.farmers,
                "reward_enable", reward_enable, "uptime", uptime, "/", config.share_window_hours, "hours"
            );
            if(total_points_failed) {
                console.log("partial_errors:", partial_errors);
            }
        } catch(e) {
            await conn.abortTransaction();
            throw e;
        } finally {
            conn.endSession();
        }
    } catch(e) {
		console.log("update() failed:", e.message);
	} finally {
        update_lock = false;
        balance_lock = false;
    }
}

var find_blocks_lock = false;

async function find_blocks()
{
    if(find_blocks_lock) {
        return;
    }
    find_blocks_lock = true;
    try {
        let since = 0;
        {
            const latest = await dbs.Block.findOne().sort({height: -1});
            if(latest) {
                since = Math.max(latest.height - config.account_delay, 0);
            }
        }
        const res = await axios.get(config.node_url + '/wapi/address/history?id='
            + config.pool_target + "&since=" + since + "&limit=-1",
            api_token_header
        );

        for(const entry of res.data) {
            if(entry.type != 'REWARD') {
                continue;
            }
            if(!entry.is_native) {
                continue;                       // not MMX
            }
            const block_hash = entry.txid;      // txid = block hash in case of type == REWARD

            if(await dbs.Block.exists({hash: block_hash})) {
                continue;
            }
            try {
                const res = await axios.get(config.node_url + '/wapi/header?hash=' + block_hash, api_token_header);
                const header = res.data;
                const block = new dbs.Block({
                    hash: header.hash,
                    height: header.height,
                    vdf_height: header.vdf_height,
                    account: header.reward_account,
                    contract: header.reward_contract,
                    reward_addr: header.reward_addr,
                    farmer_key: header.farmer_key,
                    reward: entry.amount,
                    reward_value: entry.value,
                    time: entry.time_stamp,
                    pending: true,
                });
                await block.save();
                console.log("Added block", block.height, "hash", block.hash, "reward", block.reward_value, "MMX");
            } catch(e) {
                console.log("Failed to add block:", e.message);
            }
        }
    } catch(e) {
        console.log("check() failed:", e.message);
    } finally {
        find_blocks_lock = false;
    }
}

var check_payout_lock = false;

async function check_payout()
{
    if(!sync_height) {
        return;
    }
    const height = sync_height;

    if(check_payout_lock) {
        return;
    }
    while(balance_lock) {
        await utils.sleep(1000);
    }
    check_payout_lock = true;
    balance_lock = true;
    try {
        const list = await dbs.Payout.find({pending: true});
        for(const payout of list) {
            let failed = false;
            let expired = false;
            let confirmed = false;
            try {
                const res = await axios.get(config.node_url + '/wapi/transaction?id=' + payout.txid, api_token_header);
                const tx = res.data;
                if(tx.confirm && tx.confirm >= config.account_delay) {
                    if(tx.did_fail) {
                        failed = true;
                        console.log("Payout transaction failed with:", tx.message);
                    } else {
                        confirmed = true;
                    }
                    payout.tx_fee = tx.fee.value;
                }
            } catch(e) {
                if(height - payout.height > config.payout_tx_expire + 100) {
                    failed = true;
                    expired = true;
                    console.log("Payout transaction expired:", "height", payout.height, "txid", payout.txid);
                } else {
                    console.log("Failed to check payout transaction:", e.message,
                        "response", e.response ? e.response.data : null, "txid", payout.txid);
                }
            }
            if(!confirmed && !failed) {
                continue;
            }
            payout.valid = !failed;
            payout.pending = false;

            const conn = await db.startSession();
            try {
                await conn.startTransaction();
                const opt = {session: conn};

                if(failed) {
                    // revert balances
                    for(const entry of payout.amounts) {
                        const address = entry[0];
                        const account = await dbs.Account.findOne({address: address});
                        if(!account) {
                            throw new Error("Account not found: " + address);
                        }
                        account.balance += entry[1];
                        await account.save(opt);
                    }
                    payout.expired = expired;

                    console.log("Payout failed:", "height", payout.height, "txid", payout.txid);
                } else {
                    for(const entry of payout.amounts) {
                        await dbs.Account.updateOne({address: entry[0]}, {$inc: {total_paid: entry[1]}}, opt);
                    }
                    console.log("Payout confirmed:", "height", payout.height, "total_amount", payout.total_amount,
                        "count", payout.count, "tx_fee", payout.tx_fee, "txid", payout.txid);
                }
                await payout.save(opt);

                if(!expired) {
                    const account = await dbs.Account.findOne({address: config.fee_account});
                    if(!account) {
                        throw new Error("Fee account not found: " + config.fee_account);
                    }
                    account.balance -= payout.tx_fee;
                    await account.save(opt);
                }
                await conn.commitTransaction();
            } catch(e) {
                await conn.abortTransaction();
                throw e;
            } finally {
                conn.endSession();
            }
        }
    } catch(e) {
        console.log("check() failed:", e.message);
    } finally {
        check_payout_lock = false;
        balance_lock = false;
    }
}

async function make_payout(height, amounts, opt)
{
    const options = {
        auto_send: false,
        mark_spent: true,
        expire_at: height + config.payout_tx_expire,
    };

    let tx = null;
    try {
        const res = await axios.post(config.node_url + '/wapi/wallet/send_many', {
            index: config.wallet_index,
            amounts: amounts,
            currency: "MMX",
            options: options,
        }, api_token_header);
        tx = res.data;
    } catch(e) {
        throw new Error("Failed to create payout transaction: "
            + e.message + " (" + (e.response ? e.response.data : "???") + ")");
    }

    if(tx.sender != config.pool_target) {
        throw new Error("Invalid payout transaction sender: " + tx.sender
            + " != " + config.pool_target + " (wrong config.wallet_index)");
    }

    let total_amount = 0;
    for(const entry of amounts) {
        const payout = new dbs.UserPayout({
            account: entry[0],
            height: height,
            amount: entry[1],
            txid: tx.id,
        });
        total_amount += payout.amount;
        await payout.save(opt);
    }
    const payout = new dbs.Payout({
        txid: tx.id,
        total_amount: total_amount,
        amounts: amounts,
        count: amounts.length,
        time: Date.now(),
        height: height,
        pending: true,
    });
    await payout.save(opt);

    console.log("Payout transaction created:", tx.id, "total_amount", total_amount, "count", amounts.length);
    return tx;
}

var payout_lock = false;

async function payout()
{
    if(!sync_height) {
        return;
    }
    const height = sync_height;

    if(payout_lock) {
        return;
    }
    while(balance_lock) {
        await utils.sleep(1000);
    }
    payout_lock = true;
    balance_lock = true;
    try {
        const pool = await dbs.Pool.findOne({id: "this"});
        if(!pool) {
            throw new Error("Pool state not found");
        }
        if(!pool.payout_enable) {
            console.log("Payouts currently disabled");
            return;
        }
        if(pool.last_payout && height - pool.last_payout < config.payout_interval) {
            return;
        }
        const conn = await db.startSession();
        try {
            await conn.startTransaction();
            const opt = {session: conn};

            const list = dbs.Account.find({balance: {$gt: config.payout_threshold}});

            // reserve for tx fees
            const reserve = (list.length * config.tx_output_cost + 1);

            let total_count = 0;
            let total_amount = 0;
            let outputs = [];
            let tx_list = [];

            for await(const account of list)
            {
                let amount = account.balance;

                if(account.address == config.fee_account) {
                    amount -= reserve;
                    if(amount < config.payout_threshold) {
                        continue;
                    }
                }
                account.balance -= amount;
                await account.save(opt);

                console.log("Payout triggered for", account.address, "amount", amount, "MMX");

                total_count++;
                total_amount += amount;
                outputs.push([account.address, amount]);

                if(outputs.length >= config.max_payouts) {
                    tx_list.push(await make_payout(height, outputs, opt));
                    outputs = [];
                }
            }
            if(outputs.length) {
                tx_list.push(await make_payout(height, outputs, opt));
            }
            pool.last_payout = height;
            await pool.save(opt);

            await conn.commitTransaction();

            for(const tx of tx_list) {
                try {
                    await axios.post(config.node_url + '/wapi/wallet/send_off', {
                        index: config.wallet_index,
                        tx: tx,
                    }, api_token_header);
                    console.log("Payout transaction sent:", tx.id);
                } catch(e) {
                    console.log("Failed to send payout transaction:",
                        e.message, "txid", tx.id, "response", e.response ? e.response.data : "???");
                }
            }
            if(tx_list.length) {
                console.log("Payout initiated:", "height", height, "total_amount",
                    total_amount, "total_count", total_count, "tx_count", tx_list.length);
            }
        } catch(e) {
            await conn.abortTransaction();
            throw e;
        } finally {
            conn.endSession();
        }
    } catch(e) {
        console.log("payout() failed:", e.message);
    } finally {
        payout_lock = false;
        balance_lock = false;
    }
}

async function main()
{
    db = await mongoose.connect(config.mongodb_uri);

    await query_height();
    await update();
    await payout();
    await find_blocks();
    await check_payout();

    setInterval(query_height, config.block_interval / 10);
    setInterval(update, config.share_interval * 1000);
    setInterval(payout, 15 * 60 * 1000);
    setInterval(find_blocks, config.block_interval / 2);
    setInterval(check_payout, 60 * 1000);
}

main();
