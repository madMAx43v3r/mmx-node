const config = {};

config.challenge_delay = 6;                 // [blocks]
config.block_interval = 10 * 1000;          // [ms]
config.max_response_time = 55 * 1000;       // [ms]

config.server_port = 8080;
config.node_url = "http://localhost:11380";
config.mongodb_uri = "mongodb://127.0.0.1:27017/mmx-pool?replicaSet=rs";

module.exports = config;