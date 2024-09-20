
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
    pending: {type: Boolean, default: true, index: true},
    valid: {type: Boolean, index: true},
    error_code: {type: String, index: true},
    error_message: String,
});

const account = new mongoose.Schema({
    address: {type: String, unique: true},
    balance: {type: String, default: '0', index: true},
    total_paid: {type: String, default: '0', index: true},
    difficulty: {type: Number, default: 1, index: true},
    pool_share: {type: Number, default: 0, index: true},
    created: Date,
});

const block = new mongoose.Schema({
    hash: {type: String, unique: true},
    height: {type: Number, index: true},
    account: {type: String, index: true},
    contract: {type: String, index: true},
    reward_addr: {type: String, index: true},
    reward: String,
    reward_value: {type: Number, index: true},
    time: Date,
    pending: {type: Boolean, default: true, index: true},
    valid: {type: Boolean, index: true},
});

exports.Partial = mongoose.model('Partial', partial);
exports.Account = mongoose.model('Account', account);
exports.Block = mongoose.model('Block', block);
