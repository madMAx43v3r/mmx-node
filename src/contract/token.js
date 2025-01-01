
var owner;

function init(owner_)
{
	owner = bech32(owner_);
}

function check_owner()
{
	assert(this.user == owner, "user not owner", 1);
}

function mint_to(address, amount, memo) public
{
	check_owner();
	
	if(memo == null) {
		memo = "mmx_token_mint";
	} else if(memo == false) {
		memo = null;
	}
	mint(bech32(address), uint(amount), memo);
}

function transfer(owner_) public
{
	check_owner();
	
	owner = bech32(owner_);
}

function recover(amount, currency) public
{
	check_owner();
	
	send(owner, uint(amount), bech32(currency), "mmx_token_recover");
}
