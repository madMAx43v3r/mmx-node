
interface __test;
interface template;
interface nft_1;
interface nft_2;
interface nft_test;

const nft_binary = __test.compile("src/contract/nft.js");
const template_binary = __test.compile("src/contract/template.js");

const creator_skey = sha256("skey");
const creator_key = __test.get_public_key(creator_skey);
const creator_key_hex = string_hex(creator_key);
const creator = string_bech32(sha256(creator_key));

const template_addr = template.__deploy({
	__type: "mmx.contract.Executable",
	binary: template_binary,
	init_args: [creator]
});

const msg_0 = concat(string_bech32(template_addr), "/", string(0));
const signature_0 = string_hex(__test.ecdsa_sign(creator_skey, sha256(msg_0)));

// fail due to serial 0
nft_1.__deploy({
	__type: "mmx.contract.Executable",
	binary: nft_binary,
	depends: {template: template_addr},
	init_method: "init_n",
	init_args: [creator_key_hex, 0, signature_0, {__test: 1, assert_fail: true}]
});

const msg_1 = concat(string_bech32(template_addr), "/", string(1));
const signature_1 = string_hex(__test.ecdsa_sign(creator_skey, sha256(msg_1)));

const nft_1_addr = nft_1.__deploy({
	__type: "mmx.contract.Executable",
	binary: nft_binary,
	depends: {template: template_addr},
	init_method: "init_n",
	init_args: [creator_key_hex, 1, signature_1]
});

// fail due to duplicate serial
nft_test.__deploy({
	__type: "mmx.contract.Executable",
	binary: nft_binary,
	depends: {template: template_addr},
	init_method: "init_n",
	init_args: [creator_key_hex, 1, signature_1, {__test: 1, assert_fail: true}]
});

const msg_2 = concat(string_bech32(template_addr), "/", string(2));
const signature_2 = string_hex(__test.ecdsa_sign(creator_skey, sha256(msg_2)));

const nft_2_addr = nft_2.__deploy({
	__type: "mmx.contract.Executable",
	binary: nft_binary,
	depends: {template: template_addr},
	init_method: "init_n",
	init_args: [creator_key_hex, 2, signature_2]
});
