
var owner;

function init(owner_)
{
	owner = bech32(owner_);
}

function check_owner()
{
	if(this.user != owner) {
		fail("user != owner", 1);
	}
}

function mint_to(address, amount, memo) public
{
	check_owner();
	
	mint(bech32(address), amount, memo, "mmx_token_mint");
}

function transfer(owner_) public
{
	check_owner();
	
	owner = owner_;
}
