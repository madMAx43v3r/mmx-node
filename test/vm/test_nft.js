
interface __test;
interface template;
interface nft;

const nft_binary = __test.compile("src/contract/nft.js");

const creator = "mmx1kx69pm743rshqac5lgcstlr8nq4t93hzm8gumkkxmp5y9fglnkes6ve09z";

const nft_addr = nft.__deploy({
	__type: "mmx.contract.Executable",
	binary: nft_binary,
	init_args: [creator]
});

const wallet = "mmx1e7yktu9vpeyq7hx39cmagzfp2um3kddwjf4tlt8j3kmktwc7fk6qmyc6ns";

nft.mint_to(wallet, null, {__test: 1, assert_fail: true});
nft.mint_to(wallet, null, {__test: 1, user: creator});
nft.mint_to(wallet, null, {__test: 1, user: creator, assert_fail: true});

__test.assert(__test.get_balance(wallet, nft_addr) == 1);
