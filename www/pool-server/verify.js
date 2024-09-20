
const mongoose = require('mongoose');
const dbs = require('./schema.js');
const config = require('./config.js');
const axios = require("axios");

var db = null;

async function verify(partial)
{
    try {
        const res = await axios.post(config.node_url + '/api/node/verify_partial', {
            partial: partial.data,
            pool_target: config.pool_target
        }, {
            headers: {'x-api-token': config.api_token}
        });
        if(res.status != 200) {
            throw new Error("Failed to verify partial: HTTP " + res.status + " (" + res.data + ")");
        }
        const [code, message] = res.data;

        console.log("Partial", partial.hash, "verified with code", code, '(' + message + ')');

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
        console.log("Failed to verify partial", partial.hash, ":", e.message);
    }
}

var verify_lock = false;

async function verify_all()
{
    let sync_height = null;
    try {
        const res = await axios.get(config.node_url + '/api/node/get_synced_height');
        sync_height = res.data;
        if(res.status != 200 || !sync_height) {
            console.log('Waiting for node sync ...');
            return;
        }
    } catch(e) {
        console.log("Failed to query sync height:", e.message);
        return;
    }

    if(verify_lock) {
        return;
    }
    verify_lock = true;
    try {
        const list = dbs.Partial.find({pending: true, height: {$lte: sync_height}});

        let res = [];
        let total = 0;
        for await(const partial of list) {
            res.push(verify(partial));
            total++;
        }
        await Promise.all(res);

        if(total) {
            console.log("Verified", total, "partials at height", sync_height);
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
