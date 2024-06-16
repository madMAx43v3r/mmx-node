
var creator;
var mint_height;

function init(creator_)
{
	if(read("decimals") != 0) {
		fail("decimals not zero");
	}
	creator = bech32(creator_);
}

function mint_to(address) public
{
	if(this.user != creator) {
		fail("user != creator", 1);
	}
	if(is_minted()) {
		fail("already minted", 2);
	}
	mint_height = this.height;
	
	mint(bech32(address), 1);
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

