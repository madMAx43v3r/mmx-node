
var creator;
var nfts = {};

function init(creator_)
{
	creator = bech32(creator_);
}

function add(serial, creator_key, signature) public
{
	assert(this.user);
	assert(is_uint(serial));
	assert(serial > 0);
	
	assert(nfts[serial] == null, "already minted", 2);
	
	assert(sha256(creator_key) == creator, "invalid creator", 3);
	
	const msg = concat(string_bech32(this.address), "/", string(serial));
	
	assert(ecdsa_verify(sha256(msg), creator_key, signature), "invalid signature", 4);
	
	nfts[serial] = this.user;
}
