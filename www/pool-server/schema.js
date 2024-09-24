
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
    total_paid: {type: String, default: '0', index: true},
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

const pool = new mongoose.Schema({
    id: {type: String, unique: true},
    farmers: {type: Number, default: 0},
    points_rate: {type: Number, default: 0},
    partial_rate: {type: Number, default: 0},
    partial_errors: {type: Object, minimize: false},
    last_update: {type: Number, default: 0},
});

exports.Partial = mongoose.model('Partial', partial);
exports.Account = mongoose.model('Account', account);
exports.Block = mongoose.model('Block', block);
exports.Pool = mongoose.model('Pool', pool);
