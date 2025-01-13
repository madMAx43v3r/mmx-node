
interface __test;
interface offer;

const MMX = string_bech32(bech32());
const USD = string_bech32(sha256("USD"));
const binary = __test.compile("src/contract/offer.js");

const owner = "mmx1kx69pm743rshqac5lgcstlr8nq4t93hzm8gumkkxmp5y9fglnkes6ve09z";
var price = (1 << 64) / 2;

const offer_addr = offer.__deploy({
	__type: "mmx.contract.Executable",
	binary: binary,
	init_args: [owner, MMX, USD, string(price), null]
});

__test.send(offer_addr, 10000000, MMX);

const user = "mmx1e7yktu9vpeyq7hx39cmagzfp2um3kddwjf4tlt8j3kmktwc7fk6qmyc6ns";

offer.trade(user, string(price), {__test: 1, assert_fail: true});
offer.trade(user, string(price), {__test: 1, assert_fail: true, deposit: [1000, MMX]});
offer.trade(user, string(price), {__test: 1, assert_fail: true, deposit: [1000000000000000, USD]});

offer.trade(user, string(price), {__test: 1, deposit: [1000000, USD]});

assert(__test.get_balance(user, MMX) = 500000);

price = (1 << 64);
offer.set_price(string(price), {__test: 1, user: owner});
offer.set_price(string(price), {__test: 1, user: owner, assert_fail: true});

offer.trade(user, string(price), {__test: 1, deposit: [1000000, USD]});

assert(__test.get_balance(user, MMX) = 1500000);

__test.inc_height(1080);

price = (100 << 64);
offer.set_price(string(price), {__test: 1, user: owner});
