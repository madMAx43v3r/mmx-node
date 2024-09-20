
const mongoose = require('mongoose');
const express = require('express');
const http = require('http');
const axios = require("axios");
const dbs = require('./schema.js');
const config = require('./config.js');

var db = null;
var app = express();

var sync_height = false;
var sync_time = null;

app.use(express.json());

function no_cache(req, res, next) {
    res.set('Cache-Control', 'no-cache');
    next();
}

function max_age_cache(max_age = 60) {
    return (req, res, next) => {
        res.set('Cache-Control', 'public, max-age=' + max_age);
        next();
    }
}


app.get('/difficulty', max_age_cache(30), async (req, res) =>
{
    res.json({difficulty: 1});
});

app.post('/partial', no_cache, async (req, res, next) =>
{
    try {
        const now = Date.now();
        const partial = req.body;
        
        const out = {
            valid: false
        };
        let is_valid = true;
        let response_time = null;

        if(sync_height) {
            response_time = (config.challenge_delay - (partial.height - sync_height)) * config.block_interval + (now - sync_time);

            if(response_time > config.max_response_time) {
                is_valid = false;
                out.error_code = 'PARTIAL_TOO_LATE';
                out.error_message = 'Partial received too late: ' + response_time / 1e3 + ' sec';
            }
        } else {
            is_valid = false;
            out.error_code = 'POOL_LOST_SYNC';
            out.error_message = 'Pool lost sync with blockchain';
        }
        out.response_time = response_time;

        console.log('/partial', 'height', partial.height, 'diff', partial.difficulty,
            'response', response_time / 1e3, 'time', now, 'account', partial.account, 'hash', partial.hash);

        if(partial.height < 0 || partial.height > 4294967295
            || partial.lookup_time_ms < 0 || partial.lookup_time_ms > 4294967295)
        {
            out.error_code = 'INVALID_PARTIAL';
            out.error_message = 'Invalid numeric value for height or lookup_time_ms';
            res.json(out);
            return;
        }
        if(!partial.proof) {
            out.error_code = 'INVALID_PROOF';
            out.error_message = 'Missing proof';
            res.json(out);
            return;
        }
        if(partial.proof.__type == 'mmx.ProofOfSpaceNFT') {
            const ksize = partial.proof.ksize;
            const proof_xs = partial.proof.proof_xs;
            if(ksize < 0 || ksize > 255) {
                out.error_code = 'INVALID_PROOF';
                out.error_message = 'Invalid proof ksize';
                res.json(out);
                return;
            }
            if(!Array.isArray(proof_xs) || proof_xs.length > 1024) {
                out.error_code = 'INVALID_PROOF';
                out.error_message = 'Invalid proof proof_xs';
                res.json(out);
                return;
            }
            for(const x of proof_xs) {
                if(x < 0 || x > 4294967295) {
                    out.error_code = 'INVALID_PROOF';
                    out.error_message = 'Proof value out of range';
                    res.json(out);
                    return;
                }
            }
        }
        if(partial.difficulty < 1 || partial.difficulty > 4503599627370495) {
            out.error_code = 'INVALID_DIFFICULTY';
            out.error_message = 'Invalid numeric value for difficulty';
            res.json(out);
            return;
        }
        if(await dbs.Partial.exists({hash: partial.hash})) {
            out.error_code = 'DUPLICATE_PARTIAL';
            out.error_message = 'Duplicate partial';
            res.json(out);
            return;
        }

        const entry = new dbs.Partial({
            hash: partial.hash,
            height: partial.height,
            account: partial.account,
            contract: partial.contract,
            harvester: partial.harvester,
            difficulty: partial.difficulty,
            lookup_time: partial.lookup_time_ms,
            response_time: response_time,
            time: now,
        });

        if(is_valid) {
            entry.data = partial;
        } else {
            entry.valid = false;
            entry.pending = false;
            entry.error_code = out.error_code;
            entry.error_message = out.error_message;
        }
        await entry.save();

        out.valid = is_valid;
        res.json(out);
    } catch(e) {
        next(e);
    }
});

app.use((err, req, res, next) => {
    console.log('URL', req.url);
    console.error(err);
    res.status(500).send(err.message);
});


async function update_height()
{
    try {
        const now = Date.now();
        const res = await axios.get(config.node_url + '/api/node/get_synced_height');
        const value = res.data;
        if(value) {
            if(!sync_height) {
                console.log("Node synced at height " + value);
            }
            if(!sync_height || value > sync_height) {
                console.log("New peak at", value, "time", now, "delta", now - sync_time);
                sync_time = now;
                sync_height = value;
            }
        } else {
            if(sync_height) {
                console.error("Node lost sync");
            }
            sync_height = null;
        }
    } catch(e) {
        if(sync_height != null) {
            console.error("Failed to get current height:", e.message);
        }
        sync_height = null;
    }
}

async function main()
{
    db = await mongoose.connect(config.mongodb_uri);

    http.createServer(app).listen(config.server_port);

    setInterval(update_height, 500);

    console.log("Listening on port " + config.server_port);
}

main();
