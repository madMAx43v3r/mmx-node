
var serial;
var creator;
var mint_height;

interface template;

function init(creator_)
{
	if(read("decimals") != 0) {
		fail("decimals not zero");
	}
	creator = bech32(creator_);
}

function init_n(creator_key, serial_, signature) static
{
	if(read("decimals") != 0) {
		fail("decimals not zero");
	}
	creator_key = binary_hex(creator_key);
	
	serial = uint(serial_);
	creator = sha256(creator_key);
	
	template.add(serial, creator_key, binary_hex(signature));
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

