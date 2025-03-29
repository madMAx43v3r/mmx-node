export class ChainParams {
    port = 0;
    decimals = 6;
    min_ksize = 29;
    max_ksize = 32;
    plot_filter = 4;
    post_filter = 10;
    commit_delay = 18;
    infuse_delay = 6;
    challenge_delay = 9;
    challenge_interval = 48;
    max_diff_adjust = 10;
    max_vdf_count = 100;
    avg_proof_count = 3;
    max_proof_count = 50;
    max_validators = 11;
    min_reward = 200000;
    vdf_reward = 500000;
    vdf_reward_interval = 50;
    vdf_segment_size = 50000;
    reward_adjust_div = 100;
    reward_adjust_tick = 10000;
    reward_adjust_interval = 8640;
    target_mmx_gold_price = 2000;
    time_diff_divider = 1000;
    time_diff_constant = 1000000;
    space_diff_constant = 100000000;
    initial_time_diff = 50;
    initial_space_diff = 10;
    initial_time_stamp = 0;
    min_txfee = 100;
    min_txfee_io = 100;
    min_txfee_sign = 1000;
    min_txfee_memo = 50;
    min_txfee_exec = 10000;
    min_txfee_deploy = 100000;
    min_txfee_depend = 50000;
    min_txfee_byte = 10;
    min_txfee_read = 1000;
    min_txfee_read_kbyte = 1000;
    max_block_size = 10000000;
    max_block_cost = 100000000;
    max_tx_cost = 1000000;
    max_rcall_depth = 3;
    max_rcall_width = 10;
    min_fee_ratio = null;
    block_interval_ms = 10000;
    network = null;
    nft_binary = null;
    swap_binary = null;
    offer_binary = null;
    token_binary = null;
    plot_nft_binary = null;
    escrow_binary = null;
    time_lock_binary = null;
    relay_binary = null;
    fixed_project_reward = 50000;
    project_ratio = null;
    reward_activation = 50000;
    transaction_activation = 100000;
    hardfork1_height = 1000000;

    constructor(params) {
        if (!params) {
            throw new Error("params is required");
        }
        Object.assign(this, params);
    }
}
