
const mongoose = require('mongoose');
const axios = require("axios");
const dbs = require('./schema.js');
const utils = require('./utils.js');
const config = require('./config.js');

var db = null;

async function update_account(address, reward, pool_share, points, count, height, opt)
{
    const account = await dbs.Account.findOne({address: address});
    if(account) {
        if(reward > 0) {
            account.balance = parseFloat(account.balance) + reward;
        }
        account.pool_share = pool_share;
        account.points_rate = points / config.share_window;
        account.partial_rate = count / 24;
        account.last_update = height;
        await account.save(opt);
    }
}

var update_lock = false;

async function update()
{
    let sync_height = null;
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
    const now = Date.now();
    
    if(update_lock) {
        return;
    }
    update_lock = true;
    try {
        const blocks = await dbs.Block.find({
            pending: true,
            height: {$lte: sync_height - config.account_delay}
        });

        let total_rewards = 0;
        for(const block of blocks) {
            const res = await axios.get(config.node_url + '/wapi/header?height=' + block.height);
            const valid = (res.data.hash === block.hash);
            if(valid) {
                total_rewards += block.reward_value;
            }
            block.valid = valid;
            block.pending = false;
        }

        const conn = await db.startSession();
        try {
            await conn.startTransaction();
            const opt = {session: conn};

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
            const start_height = sync_height - config.share_window;

            const result = await dbs.Partial.aggregate([
                {$match: {valid: true, height: {$gte: start_height}}},
                {$group: {_id: "$account", points: {$sum: "$points"}, count: {$sum: 1}}}
            ]);

            const errors = await dbs.Partial.aggregate([
                {$match: {valid: false, height: {$gte: start_height}}},
                {$group: {_id: "$error_code", points: {$sum: "$points"}, count: {$sum: 1}}}
            ]);

            let total_points = 0;
            let total_partials = 0;
            for(const entry of result) {
                total_points += entry.points;
                total_partials += entry.count;
            }

            let res = [];
            for(const entry of result) {
                const pool_share = entry.points / total_points;
                const reward_share = total_rewards * pool_share;
                res.push(update_account(
                    entry._id, reward_share, pool_share, entry.points, entry.count, sync_height, opt));
            }
            await Promise.all(res);

            for(const block of blocks) {
                await block.save(opt);
            }

            let pool = await dbs.Pool.findOne({id: "this"});
            if(!pool) {
                pool = new dbs.Pool({id: "this"});
            }
            pool.farmers = result.length;
            pool.points_rate = total_points / config.share_window;
            pool.partial_rate = total_partials / 24;

            const partial_errors = {};
            for(const entry of errors) {
                partial_errors[entry._id] = {
                    points: entry.points,
                    count: entry.count,
                    share: entry.points / total_points
                };
            }
            pool.partial_errors = partial_errors;
            pool.last_update = sync_height;

            await conn.commitTransaction();

            for(const block of blocks) {
                if(block.valid) {
                    console.log("Farmed block", block.height, "reward", block.reward_value / config.mmx_divider, "MMX", "account", block.account);
                } else {
                    console.log("Invalid block", block.height);
                }
            }
            console.log(
                "Distributed", total_rewards / config.mmx_divider, "MMX to", result.length, "accounts",
                "total_points", total_points, "total_partials", total_partials);
            console.log("Errors:", partial_errors);
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

async function main()
{
    db = await mongoose.connect(config.mongodb_uri);

    update();

    setInterval(update, config.share_interval * 1000);
}

main();
