
const mongoose = require('mongoose');

const partial = new mongoose.Schema({
	hash: {type: String, unique: true},
	height: {type: Number, index: true},
    account: {type: String, index: true},
    contract: {type: String, index: true},
    harvester: {type: String, index: true},
    difficulty: Number,
    lookup_time: Number,
    response_time: Number,
    time: {type: Date, index: true},
    data: Object,
    points: {type: Number, default: 0},
    pending: {type: Boolean, default: true, index: true},
    valid: {type: Boolean, index: true},
    error_code: {type: String, index: true},
    error_message: String,
});

const account = new mongoose.Schema({
    address: {type: String, unique: true},
    balance: {type: Number, default: 0, index: true},
    total_paid: {type: Number, default: 0, index: true},
    difficulty: {type: Number, default: 1, index: true},
    pool_share: {type: Number, default: 0, index: true},
    points_rate: {type: Number, default: 0, index: true},
    partial_rate: {type: Number, default: 0, index: true},
    created: {type: Date, default: Date.now, index: true},
    last_update: {type: Number, index: true},
});

const block = new mongoose.Schema({
    hash: {type: String, unique: true},
    height: {type: Number, index: true},
    vdf_height: {type: Number, index: true},
    account: {type: String, index: true},
    contract: {type: String, index: true},
    farmer_key: {type: String, index: true},
    reward_addr: {type: String, index: true},
    reward: String,
    reward_value: {type: Number, index: true},
    time: Date,
    pending: {type: Boolean, default: true, index: true},
    valid: {type: Boolean, index: true},
});

const payout = new mongoose.Schema({
    txid: {type: String, unique: true},
    total_amount: Number,
    tx_fee: Number,
    amounts: Array,     // [[account, amount], ...]
    count: Number,
    time: Date,
    height: {type: Number, index: true},
    pending: {type: Boolean, default: true, index: true},
    expired: {type: Boolean, index: true},
    valid: {type: Boolean, index: true},
});

const user_payout = new mongoose.Schema({
    account: {type: String, index: true},
    height: {type: Number, index: true},
    amount: Number,
    txid: String,
});

const pool = new mongoose.Schema({
    id: {type: String, unique: true},
    farmers: {type: Number, default: 0},
    points_rate: {type: Number, default: 0},
    partial_rate: {type: Number, default: 0},
    partial_errors: {type: Object},
    last_update: {type: Number, default: 0},
    last_payout: {type: Number, default: 0},
    payout_enable: {type: Boolean, default: true},
}, {
    minimize: false,
});

exports.Partial = mongoose.model('Partial', partial);
exports.Account = mongoose.model('Account', account);
exports.Block = mongoose.model('Block', block);
exports.Payout = mongoose.model('Payout', payout);
exports.UserPayout = mongoose.model('UserPayout', user_payout);
exports.Pool = mongoose.model('Pool', pool);
