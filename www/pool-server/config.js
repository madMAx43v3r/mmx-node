const config = {};

config.pool_fee = 0.01;                     // [%]
config.challenge_delay = 6;                 // [blocks]
config.partial_expiry = 100;                // [blocks]
config.account_delay = 48;                  // [blocks]
config.share_window = 8640;                 // [blocks]
config.block_interval = 10 * 1000;          // [ms]
config.share_window_hours = 24;             // [hours]
config.share_interval = 5 * 60;             // [sec]
config.min_difficulty = 1;                  // diff 1 gives ~0.7 partials per k32 per height
config.default_difficulty = 10;
config.space_diff_constant = 100000000000;
config.payout_interval = 8640;              // [blocks]
config.payout_tx_expire = 1000;             // [blocks]
config.payout_threshold = 10;               // [MMX]
config.tx_output_cost = 0.01;               // [MMX]
config.max_payout_count = 1000;             // max number of payouts per transaction

config.server_port = 8080;
config.wallet_index = 0;                    // for pool wallet (payout)
config.node_url = "http://localhost:11380";
config.pool_name = "MMX Pool";
config.pool_description = "MMX Pool (reference implementation)";
config.logo_path = "/img/logo.png";
config.fee_account = "mmx1e7yktu9vpeyq7hx39cmagzfp2um3kddwjf4tlt8j3kmktwc7fk6qmyc6ns";      // where the pool fee goes
config.pool_target = "mmx1uj2dth7r9tcn3vas42f0hzz74dkz8ygv59mpx44n7px7j7yhvv4sfmkf0d";      // where the block rewards go
config.api_token = "B01255C3AD4640292FAB5D0078439E945349EE13A9DD8C1DFE9F69FD0AE8B18C";      // for node API
config.mongodb_uri = "mongodb://127.0.0.1:27017/mmx-pool?replicaSet=rs";

module.exports = config;