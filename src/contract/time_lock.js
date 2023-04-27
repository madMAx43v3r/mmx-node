
var owner;
var unlock_height;

function init(owner_, unlock_height_)
{
	owner = bech32(owner_);
	unlock_height = uint(unlock_height_);
}

function check_owner() const
{
	if(this.user != owner) {
		fail("user != owner", 1);
	}
}

function deposit() public {}

function withdraw(address, amount, currency) public
{
	check_owner();
	
	if(this.height < unlock_height) {
		fail("still locked", 2);
	}
	send(bech32(address), amount, bech32(currency));
}

function set_unlock_height(height) public
{
	check_owner();
	
	if(height < unlock_height) {
		fail("invalid height", 3);
	}
	unlock_height = height;
}