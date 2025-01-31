
const FRACT_BITS = 64;

var owner;
var price;
var currency;

function init(owner_, price_, currency_)
{
	owner = bech32(owner_);
	price = uint(price_);
	currency = bech32(currency_);
}

function check_owner()
{
	assert(this.user == owner, "user not owner", 1);
}

function buy() public payable
{
	assert(this.deposit.currency == currency, "wrong currency", 2);
	
	mint(this.user, (this.deposit.amount * price) >> FRACT_BITS);
}

function withdraw(amount) public
{
	check_owner();
	
	if(!amount) {
		amount = balance(currency);
	} else {
		amount = uint(amount);
	}
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
