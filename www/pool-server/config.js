const config = {};

config.challenge_delay = 6;                 // [blocks]
config.block_interval = 10 * 1000;          // [ms]
config.max_response_time = 55 * 1000;       // [ms]
config.partial_expiry = 100;                // [blocks]

config.server_port = 8080;
config.node_url = "http://localhost:11380";
config.pool_target = "mmx1uj2dth7r9tcn3vas42f0hzz74dkz8ygv59mpx44n7px7j7yhvv4sfmkf0d";
config.api_token = "B01255C3AD4640292FAB5D0078439E945349EE13A9DD8C1DFE9F69FD0AE8B18C";
config.mongodb_uri = "mongodb://127.0.0.1:27017/mmx-pool?replicaSet=rs";

module.exports = config;