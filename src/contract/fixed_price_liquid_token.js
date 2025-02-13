
const FRACT_BITS = 64;
const RATIO_DIVIDER = 1000;

var owner;
var price;
var ratio;			// withdraw ratio (0 to 1000)
var currency;
var owner_balance = 0;

function init(owner_, price_, currency_, ratio_)
{
	owner = bech32(owner_);
	price = uint(price_);
	ratio = uint(ratio_);
	currency = bech32(currency_);
	
	assert(price > 0);
	assert(ratio < RATIO_DIVIDER);
}

function check_owner()
{
	assert(this.user == owner, "user not owner", 1);
}

function buy() public payable
{
	assert(this.deposit.currency == currency, "wrong currency", 2);
	
	owner_balance += (this.deposit.amount * ratio) / RATIO_DIVIDER;
	
	mint(this.user, (this.deposit.amount * price) >> FRACT_BITS);
}

function sell() public payable
{
	assert(this.deposit.currency == this.address, "wrong token", 2);
	
	var ask_amount = (this.deposit.amount << FRACT_BITS) / price;
	
	ask_amount = (ask_amount * (RATIO_DIVIDER - ratio)) / RATIO_DIVIDER;
	
	send(this.user, ask_amount, currency);
	send(bech32(), this.deposit.amount, this.address);
}

function withdraw(amount) public
{
	check_owner();
	
	if(!amount) {
		amount = owner_balance;
	} else {
		amount = uint(amount);
	}
	assert(amount <= owner_balance);
	
	owner_balance -= amount;
	
	send(owner, amount, currency);
}

function recover(amount, currency) public
{
	check_owner();
	
	send(owner, uint(amount), bech32(currency), "mmx_token_recover");
}

function transfer(owner_) public
{
	check_owner();
	
	owner = bech32(owner_);
}
