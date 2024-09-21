
const axios = require("axios");
const config = require('./config.js');

async function get_synced_height()
{
    const res = await axios.get(config.node_url + '/api/node/get_synced_height');
    return res.data;
}

exports.get_synced_height = get_synced_height;
