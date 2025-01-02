
const mongoose = require('mongoose');
const axios = require("axios");
const dbs = require('./schema.js');
const utils = require('./utils.js');
const config = require('./config.js');

var db = null;
var first_sync = null;

async function verify(partial, vdf_height)
{
    try {
        if(vdf_height > partial.height + config.partial_expiry)
        {
            if(partial.height < first_sync) {
                partial.error_code = 'POOL_LOST_SYNC';
                partial.error_message = 'Pool server lost sync with blockchain or was offline';
            } else {
                partial.error_code = 'SERVER_ERROR';
                partial.error_message = 'Partial expired for some reason';
            }
            partial.data = null;
            partial.valid = false;
            partial.pending = false;
            await partial.save();

            console.log("Partial", partial.hash, "expired at height", vdf_height, 'due to', partial.error_code, '(' + partial.error_message + ')');
            return;
        }

        const res = await axios.post(config.node_url + '/api/node/verify_partial', {
            partial: partial.data,
            pool_target: config.pool_target
        }, {
            headers: {'x-api-token': config.api_token}
        });
        const [code, message] = res.data;

        // console.log("Partial", partial.hash, "verified with code", code, '(' + message + ')');

        if(code == 'NONE') {
            partial.valid = true;
        } else {
            partial.valid = false;
            partial.error_code = code;
            partial.error_message = message;

            console.log("Partial", partial.hash, "failed verification with code", code, '(' + message + ')');
        }
        partial.data = null;
        partial.pending = false;
        await partial.save();
    } catch(e) {
        console.log("Failed to verify partial", partial.hash, ":", e.message, '(' + e.response.data + ')');
    }

    try {
        if(partial.valid) {
            let account = await dbs.Account.findOne({address: partial.account});
            if(!account) {
                account = new dbs.Account({
                    address: partial.account,
                    difficulty: config.default_difficulty,
                });
                await account.save();

                console.log("Created new account for", partial.account);
            }
        }
    } catch(e) {
        console.log("Failed to create account:", e.message);
    }
}

var verify_lock = false;

async function verify_all()
{
    let vdf_height = null;
    try {
        vdf_height = await utils.get_synced_vdf_height();
        if(!vdf_height) {
            console.log('Waiting for node sync ...');
            return;
        }
    } catch(e) {
        console.log("Failed to query sync height:", e.message);
        return;
    }
    if(!first_sync) {
        first_sync = vdf_height;
    }

    if(verify_lock) {
        return;
    }
    verify_lock = true;
    try {
        const list = dbs.Partial.find({pending: true, height: {$lte: vdf_height}});

        let res = [];
        let total = 0;
        for await(const partial of list) {
            res.push(verify(partial, vdf_height));
            total++;
        }
        await Promise.all(res);

        if(total) {
            console.log("Verified", total, "partials at height", vdf_height);
        }
    } catch(e) {
		console.log("verify_all() failed:", e.message);
	} finally {
        verify_lock = false;
    }
}

async function main()
{
    db = await mongoose.connect(config.mongodb_uri);

    setInterval(verify_all, 1000);
}

main();
