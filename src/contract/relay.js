
var owner;
var transfer_target;

function init(owner_)
{
	owner = bech32(owner_);
}

function claim_all(currency) public
{
	assert(this.user == owner, "user not owner", 1);
	
	if(!currency) {
		currency = bech32();
	} else {
		currency = bech32(currency);
	}
	send(this.user, balance(currency), currency);
}

function transfer(new_owner) public
{
	assert(this.user == owner, "user not owner", 1);
	
	transfer_target = bech32(new_owner);
}

function complete() public
{
	assert(transfer_target, "no pending transfer");
	
	assert(this.user == transfer_target, "user not target");
	
	owner = transfer_target;
	transfer_target = null;
}
