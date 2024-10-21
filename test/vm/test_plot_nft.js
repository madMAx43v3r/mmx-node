
interface __test;
interface plot_nft;

const MMX = bech32();
const binary = __test.compile("src/contract/plot_nft.js");

const owner = "mmx1kx69pm743rshqac5lgcstlr8nq4t93hzm8gumkkxmp5y9fglnkes6ve09z";

const plot_nft_addr = plot_nft.__deploy({
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
__test.assert(plot_nft.mmx_reward_target(null) == pool_target);
__test.assert(plot_nft.mmx_reward_target(farmer) == pool_target);

plot_nft.set_server_url("http://pool.mmx.network", {__test: 1, user: owner});

const pool_wallet = "mmx13nf0pdarcfdm7tmxxlchnq9yw8gdvnymg5avfad04t5yqu7myewq6wkcqp";

__test.send(plot_nft_addr, 500000, MMX);

plot_nft.claim_all(owner, MMX, {__test: 1, user: owner, assert_fail: true});
plot_nft.claim_all(pool_wallet, MMX, {__test: 1, user: pool_target});

__test.assert(__test.get_balance(pool_wallet, MMX) == 500000);

plot_nft.unlock({__test: true, user: owner});

__test.assert(plot_nft.is_locked() == true);
__test.inc_height(1);
__test.assert(plot_nft.is_locked() == true);
__test.inc_height(255);
__test.assert(plot_nft.is_locked() == false);

__test.assert(plot_nft.mmx_reward_target(null) == owner);
__test.assert(plot_nft.mmx_reward_target(farmer) == farmer);

const owner_wallet = "mmx1mlurm4px73xghkdn02u60qff4f795nnmt04e4as595tf4luvasmsggxmfk";

__test.send(plot_nft_addr, 500000, MMX);

plot_nft.claim_all(owner_wallet, MMX, {__test: 1, user: owner});
plot_nft.claim_all(pool_wallet, MMX, {__test: 1, user: pool_target, assert_fail: true});

__test.assert(__test.get_balance(owner_wallet, MMX) == 500000);

__test.inc_height(100);

__test.assert(plot_nft.is_locked() == false);

plot_nft.lock(pool_target, null, {__test: 1, user: owner});

__test.assert(plot_nft.is_locked() == true);
__test.assert(plot_nft.mmx_reward_target(null) == pool_target);
__test.assert(plot_nft.mmx_reward_target(farmer) == pool_target);

__test.inc_height(100);

__test.assert(plot_nft.is_locked() == true);

const new_owner = "mmx1rc6ezc4pmjw2vx7m7t63cl3qz6na56nnuywweekt88ds2j6q3c0qvy4qmr";

plot_nft.transfer(new_owner, null, {__test: 1, user: owner});

plot_nft.unlock({__test: true, user: owner, assert_fail: true});
plot_nft.unlock({__test: true, user: new_owner});


