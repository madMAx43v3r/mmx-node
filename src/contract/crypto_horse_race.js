
const FRACT_BITS = 64;
const PRICE_ITERS = 20;
const TRANSFER_EXPIRE = 360;	// blocks

var currency;
var PRECISION = 1;				// price increment
var TOKEN_SCALE = 1;
var activation_height;

var wallets = {};				// user token balances
var balance = [];				// currency per horse


function init(num_horses, currency_, precision, decimals, activation_height_)
{
	for(var i = 0; i < num_horses; ++i) {
		push(balance, 0);
	}
	currency = bech32(currency_);
	activation_height = uint(activation_height_);
	
	for(var i = 0; i < precision; ++i) {
		PRECISION *= 10;
	}
	for(var i = 0; i < decimals; ++i) {
		TOKEN_SCALE *= 10;
	}
}

function calc_price(x) const public
{
	x /= PRECISION;
	var y = x;
	for(var i = 0; i < PRICE_ITERS && y > 0; ++i) {
		y = ((y << FRACT_BITS) + (x << FRACT_BITS) / y) >> (FRACT_BITS + 1);
	}
	return y * PRECISION;
}

function get_price(index) const public
{
	return calc_price(balance[index]);
}

function get_buy_price(index, amount) const public
{
	return calc_price(balance[index] + uint(amount));
}

function get_sell_price(index, amount) const public
{
	return calc_price(balance[index] - ((uint(amount) * get_price(index)) / TOKEN_SCALE));
}

function get_wallet(user)
{
	assert(user, "invalid user");
	
	var wallet = wallets[user];
	if(!wallet) {
		wallet = {};
		wallets[user] = wallet;
	}
	return wallet;
}

function get_wallet_balance(wallet, index)
{
	var balance = wallet[index];
	if(!balance) {
		balance = 0;
	}
	return balance;
}

function buy_horse(index) public payable
{
	assert(index < size(balance), "no such horse");
	assert(this.height >= activation_height, "not active yet");
	assert(this.deposit.currency == currency, "wrong currency");
	
	const price = get_buy_price(index, this.deposit.amount);
	const token_amount = (this.deposit.amount * TOKEN_SCALE) / price;
	
	const wallet = get_wallet(this.user);
	wallet[index] = get_wallet_balance(wallet, index) + token_amount;
	
	balance[index] += this.deposit.amount;
}

function sell_horse(index, amount) public
{
	amount = uint(amount);
	
	assert(amount > 0);
	assert(index < size(balance), "no such horse");
	
	const wallet = get_wallet(this.user);
	const wallet_balance = get_wallet_balance(wallet, index);
	
	assert(amount <= wallet_balance, "insufficient balance");
	
	const price = get_sell_price(index, amount);
	const ret_amount = (amount * price) / TOKEN_SCALE;
	
	wallet[index] = wallet_balance - amount;
	balance[index] -= ret_amount;
	
	send(this.user, ret_amount, currency);
}

function transfer(target, amount, index) public
{
	target = bech32(target);
	amount = uint(amount);
	
	assert(amount > 0);
	assert(index < size(balance), "no such horse");
	
	const wallet = get_wallet(this.user);
	const wallet_balance = get_wallet_balance(wallet, index);
	
	assert(amount <= wallet_balance, "insufficient balance");
	
	const target_wallet = get_wallet(target);
	
	assert(target_wallet, "no such wallet");
	
	wallet[index] -= amount;
	target_wallet[index] = get_wallet_balance(target_wallet, index) + amount;
}





