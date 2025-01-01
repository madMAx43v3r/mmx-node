
var creator;
var nfts = {};

function init(creator_)
{
	creator = bech32(creator_);
}

function add(serial, creator_key, signature) public
{
	// TODO: is_uint()
	if(serial == 0 || typeof(serial) != 4) {
		fail("invalid serial", 1);
	}
	if(nfts[serial] != null) {
		fail("already minted", 2);
	}
	if(sha256(creator_key) != creator) {
		fail("invalid creator", 3);
	}
	const msg = concat(string_bech32(this.address), "/", string(serial));
	
	if(!ecdsa_verify(sha256(msg), creator_key, signature)) {
		fail("invalid signature", 4);
	}
	nfts[serial] = this.user;
}
