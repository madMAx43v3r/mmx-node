
var owner;
var unlock_height;

function init(owner_, unlock_height_)
{
	owner = bech32(owner_);
	unlock_height = uint(unlock_height_);
}

function check_owner() const
{
	assert(this.user == owner, "user not owner", 1);
}

function is_locked() const public
{
	return this.height < unlock_height;
}

function deposit() public {}

function withdraw(address, amount, currency) public
{
	check_owner();
	
	assert(!is_locked(), "still locked", 2);
	
	send(bech32(address), amount, bech32(currency));
}

function set_unlock_height(height) public
{
	check_owner();
	
	assert(height >= unlock_height, "invalid height", 3);
	
	unlock_height = height;
}