
const WITHDRAW_FACTOR = 90;

var owner;

function init(owner_) {
	owner = bech32(owner_);
}

function deposit() public {}

function withdraw(amount) public
{
	if(this.user != owner) {
		fail("user != owner", 1);
	}
	const ret_amount = (amount * WITHDRAW_FACTOR) / 100;
	
	send(owner, ret_amount);
	send(0, amount - ret_amount);
}
