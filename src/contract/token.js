
var owner;

function init(owner_)
{
	owner = owner_;
}

function check_owner()
{
	if(this.user != owner) {
		fail("user != owner", 1);
	}
}

function mint_to(address, amount) public
{
	check_owner();
	
	mint(bech32(address), amount);
}

function transfer(owner_) public
{
	check_owner();
	
	owner = owner_;
}
