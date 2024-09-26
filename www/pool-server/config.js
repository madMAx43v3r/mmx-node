const config = {};

config.pool_fee = 0.01;                     // [%]
config.mmx_divider = 1e6;
config.challenge_delay = 6;                 // [blocks]
config.partial_expiry = 100;                // [blocks]
config.account_delay = 24;                  // [blocks]
config.share_window = 8640;                 // [blocks]
config.block_interval = 10 * 1000;          // [ms]
config.max_response_time = 50 * 1000;       // [ms]
config.share_window_hours = 24;             // [hours]
config.share_interval = 5 * 60;             // [sec]
config.min_difficulty = 1;                  // needs to be >= 2 for mainnet
config.default_difficulty = 1;
config.space_diff_constant = 10000000000;
config.payout_interval = 8640;              // [blocks]
config.payout_tx_expire = 1000;             // [blocks]
config.payout_threshold = 5;                // [MMX]
config.max_payout_count = 2000;

config.server_port = 8080;
config.wallet_index = 0;                    // for pool wallet (payout)
config.node_url = "http://localhost:11380";
config.fee_account = "mmx1e7yktu9vpeyq7hx39cmagzfp2um3kddwjf4tlt8j3kmktwc7fk6qmyc6ns";
config.pool_target = "mmx1uj2dth7r9tcn3vas42f0hzz74dkz8ygv59mpx44n7px7j7yhvv4sfmkf0d";
config.api_token = "B01255C3AD4640292FAB5D0078439E945349EE13A9DD8C1DFE9F69FD0AE8B18C";
config.mongodb_uri = "mongodb://127.0.0.1:27017/mmx-pool?replicaSet=rs";

module.exports = config;