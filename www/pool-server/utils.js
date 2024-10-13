
const axios = require("axios");
const config = require('./config.js');

async function get_synced_height()
{
    const res = await axios.get(config.node_url + '/api/node/get_synced_height');
    return res.data;
}

function calc_eff_space(points_rate)
{
    // points_rate = points per block (height)
    return points_rate * config.space_diff_constant * 2.467 * 1e-12;    // [TB]
}

exports.get_synced_height = get_synced_height;
exports.calc_eff_space = calc_eff_space;
