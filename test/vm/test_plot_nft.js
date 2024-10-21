
interface __test;
interface plot_nft;

const binary = __test.compile("src/contract/plot_nft.js", {});

const owner = "mmx1kx69pm743rshqac5lgcstlr8nq4t93hzm8gumkkxmp5y9fglnkes6ve09z";

const plot_nft_addr = __test.deploy("plot_nft", {
	__type: "mmx.contract.Executable",
	binary: binary,
	init_args: [owner]
});

plot_nft.check_owner({__test: 1, user: owner});
plot_nft.check_owner({__test: 1, user: bech32(owner)});

plot_nft.check_owner({__test: 1, assert_fail: true});

__test.assert(plot_nft.is_locked() == false);

__test.assert(__test.get_balance(plot_nft_addr, bech32()) == 0);

const farmer = "mmx1e7yktu9vpeyq7hx39cmagzfp2um3kddwjf4tlt8j3kmktwc7fk6qmyc6ns";

__test.assert(plot_nft.mmx_reward_target(null) == owner);
__test.assert(plot_nft.mmx_reward_target(farmer) == farmer);

const pool_target = "mmx1uj2dth7r9tcn3vas42f0hzz74dkz8ygv59mpx44n7px7j7yhvv4sfmkf0d";

plot_nft.lock(pool_target, null, {__test: 1, user: owner});

__test.assert(plot_nft.is_locked() == true);

plot_nft.set_server_url("http://pool.mmx.network", {__test: 1, user: owner});

plot_nft.unlock({__test: true, user: owner});

__test.assert(plot_nft.is_locked() == true);
__test.inc_height(1);
__test.assert(plot_nft.is_locked() == true);
__test.inc_height(255);
__test.assert(plot_nft.is_locked() == false);
