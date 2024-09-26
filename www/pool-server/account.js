
const mongoose = require('mongoose');
const axios = require("axios");
const dbs = require('./schema.js');
const utils = require('./utils.js');
const config = require('./config.js');

var db = null;
var sync_height = null;

async function update_account(address, reward, pool_share, points, count, height, opt)
{
    const account = await dbs.Account.findOne({address: address});
    if(account) {
        if(reward > 0) {
            account.balance = parseFloat(account.balance) + reward;
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
    update_lock = true;
    try {
        const conn = await db.startSession();
        try {
            await conn.startTransaction();
            const opt = {session: conn};

            const start_height = sync_height - config.share_window;

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
            pool.last_update = sync_height;
            await pool.save(opt);

            let total_rewards = 0;

            if(reward_enable) {
                // Find all blocks that are pending and have been confirmed enough
                const blocks = await dbs.Block.find({
                    pending: true,
                    height: {$lte: sync_height - config.account_delay}
                });

                for(const block of blocks) {
                    const res = await axios.get(config.node_url + '/wapi/header?height=' + block.height);
                    const valid = (res.data.hash === block.hash);
                    if(valid) {
                        total_rewards += block.reward_value;
                    }
                    block.valid = valid;
                    block.pending = false;
                    await block.save(opt);

                    if(valid) {
                        console.log("Farmed block", block.height, "reward", block.reward_value / config.mmx_divider, "MMX", "account", block.account);
                    } else {
                        console.log("Invalid block", block.height);
                    }
                }
            }
            
            // Take pool fee first
            if(total_rewards) {
                const fee = total_rewards * config.pool_fee;
                total_rewards -= fee;

                const account = await dbs.Account.findOne({address: config.fee_account});
                if(!account) {
                    account = new dbs.Account({address: config.fee_account});
                }
                account.balance += fee;
                await account.save(opt);
            }

            // Distribute rewards and update account stats
            let res = [];
            for(const entry of result) {
                const pool_share = entry.points / total_points;
                const reward_share = total_rewards * pool_share;
                res.push(update_account(
                    entry._id, reward_share, pool_share, entry.points, entry.count, sync_height, opt));
            }
            await Promise.all(res);

            await conn.commitTransaction();

            if(total_rewards) {
                console.log(
                    "Distributed", total_rewards / config.mmx_divider, "MMX to", result.length, "accounts",
                    "total_points", total_points, "total_partials", total_partials);
            }
            console.log(
                "height", sync_height, "points_rate", pool.points_rate, "partial_rate", pool.partial_rate, "farmers", pool.farmers,
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
    }
}

var check_lock = false;

async function check()
{
    try {
        sync_height = await utils.get_synced_height();
        if(!sync_height) {
            console.log('Waiting for node sync ...');
            return;
        }
    } catch(e) {
        console.log("Failed to query sync height:", e.message);
        return;
    }
    
    if(check_lock) {
        return;
    }
    check_lock = true;
    try {
        let since = 0;
        {
            const latest = await dbs.Block.findOne().sort({height: -1});
            if(latest) {
                since = Math.max(latest.height - config.account_delay, 0);
            }
        }
        const res = await axios.get(config.node_url + '/wapi/address/history?id='
            + config.fee_account + "&since=" + since + "&until=" + (sync_height - 1) + "&limit=-1",
            {headers: {'x-api-token': config.api_token}}
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
                const res = await axios.get(config.node_url + '/wapi/header?hash=' + block_hash);
                const header = res.data;
                const block = new dbs.Block({
                    hash: header.hash,
                    height: header.height,
                    account: header.reward_account,
                    contract: header.reward_contract,
                    reward_addr: header.reward_addr,
                    farmer_key: header.proof.farmer_key,
                    reward: entry.amount,
                    reward_value: entry.value,
                    time: entry.time,
                    pending: true,
                });
                await block.save();
            } catch(e) {
                console.log("Failed to add block:", e.message);
            }
        }
    } catch(e) {
        console.log("check() failed:", e.message);
    } finally {
        check_lock = false;
    }
}

async function main()
{
    db = await mongoose.connect(config.mongodb_uri);

    await check();
    await update();

    setInterval(check, config.block_interval / 5);
    setInterval(update, config.share_interval * 1000);
}

main();
