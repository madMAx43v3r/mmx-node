
var serial;
var creator;
var mint_height;

function init(creator_)
{
	if(read("decimals") != 0) {
		fail("decimals not zero");
	}
	creator = bech32(creator_);
}

function init_ex(serial_, creator_key, signature) static
{
	if(read("decimals") != 0) {
		fail("decimals not zero");
	}
	rcall("template", "add", serial_, creator_key, signature);
	
	serial = serial_;
	creator = sha256(creator_key);
}

function mint_to(address, memo) public
{
	if(this.user != creator) {
		fail("user != creator", 1);
	}
	if(is_minted()) {
		fail("already minted", 2);
	}
	mint_height = this.height;
	
	if(memo == null) {
		memo = "mmx_nft_mint";
	} else if(memo == false) {
		memo = null;
	}
	mint(bech32(address), 1, memo);
}

function get_creator() const public
{
	return creator;
}

function is_minted() const public
{
	return mint_height != null;
}

function get_mint_height() const public
{
	return mint_height;
}

