
var owner;
var transfer_target;

function init(owner_)
{
	owner = bech32(owner_);
}

function claim(currency)
{
	if(this.user != owner) {
		fail("user != owner");
	}
	if(!currency) {
		currency = bech32();
	} else {
		currency = bech32(currency);
	}
	send(this.user, balance(currency), currency);
}

function transfer(new_owner)
{
	if(this.user != owner) {
		fail("user != owner");
	}
	transfer_target = bech32(new_owner);
}

function complete()
{
	if(!transfer_target) {
		fail("no pending transfer");
	}
	if(this.user != transfer_target) {
		fail("user != transfer_target");
	}
	owner = transfer_target;
	transfer_target = null;
}
