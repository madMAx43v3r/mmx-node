
interface __test;
interface template;
interface nft_1;
interface nft_2;
interface nft_test;

const nft_binary = __test.compile("src/contract/nft.js");
const template_binary = __test.compile("src/contract/template.js");

const creator_skey = sha256("skey");
const creator_key = __test.get_public_key(creator_skey);
const creator_key_hex = to_string_hex(creator_key);
const creator = to_string_bech32(sha256(creator_key));

const template_addr = template.__deploy({
	__type: "mmx.contract.Executable",
	binary: template_binary,
	init_args: [creator]
});

const txid = sha256("nft_1");
const signature = to_string_hex(__test.ecdsa_sign(creator_skey, txid));

// fail due to serial 0
nft_1.__deploy({
	__type: "mmx.contract.Executable",
	binary: nft_binary,
	depends: {template: template_addr},
	init_method: "init_n",
	init_args: [creator_key_hex, 0, signature, {__test: 1, user: to_string_bech32(txid), assert_fail: true}]
});

const nft_1_addr = nft_1.__deploy({
	__type: "mmx.contract.Executable",
	binary: nft_binary,
	depends: {template: template_addr},
	init_method: "init_n",
	init_args: [creator_key_hex, 1, signature, {__test: 1, user: to_string_bech32(txid)}]
});

const txid_test = sha256("nft_test");
const signature_test = to_string_hex(__test.ecdsa_sign(creator_skey, txid_test));

// fail due to duplicate serial
nft_test.__deploy({
	__type: "mmx.contract.Executable",
	binary: nft_binary,
	depends: {template: template_addr},
	init_method: "init_n",
	init_args: [creator_key_hex, 1, signature_test, {__test: 1, user: to_string_bech32(txid_test), assert_fail: true}]
});

const txid_2 = sha256("nft_2");
const signature_2 = to_string_hex(__test.ecdsa_sign(creator_skey, txid_2));

const nft_2_addr = nft_2.__deploy({
	__type: "mmx.contract.Executable",
	binary: nft_binary,
	depends: {template: template_addr},
	init_method: "init_n",
	init_args: [creator_key_hex, 2, signature_2, {__test: 1, user: to_string_bech32(txid_2)}]
});
