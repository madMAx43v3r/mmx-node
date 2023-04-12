
const FRACT_BITS = 64;

var owner;
var partner;
var bid_currency;
var ask_currency;
var inv_price;

function init(owner_, bid_currency_, ask_currency_, inv_price_, partner_)
{
	owner = bech32(owner_);
	bid_currency = bech32(bid_currency_);
	ask_currency = bech32(ask_currency_);
	inv_price = uint(inv_price_);
	
	if(partner_ != null) {
		partner = bech32(partner_);
	} else {
		partner = null;
	}
}

function check_owner()
{
	if(this.user != owner) {
		fail("user != owner", 1);
	}
}

function check_partner()
{
	if(partner != null) {
		if(this.user != partner) {
			fail("user != partner", 2);
		}
	}
}

function deposit() public
{
	check_owner();
}

function cancel() public
{
	check_owner();
	
	const amount = this.balance[bid_currency];
	if(amount > 0) {
		send(owner, amount, bid_currency);
	}
}

function withdraw() public
{
	check_owner();
	
	const amount = this.balance[ask_currency];
	if(amount > 0) {
		send(owner, amount, ask_currency);
	}
}

function trade(dst_addr) public payable
{
	check_partner();
	
	if(this.deposit.currency != ask_currency) {
		fail("currency mismatch", 3);
	}
	const bid_amount = (this.deposit.amount * inv_price) >> FRACT_BITS;
	send(bech32(dst_addr), bid_amount, bid_currency);
}

function accept(dst_addr) public payable
{
	check_partner();
	
	dst_addr = bech32(dst_addr);
	
	if(this.deposit.currency != ask_currency) {
		fail("currency mismatch", 3);
	}
	const bid_amount = this.balance[bid_currency];
	if(bid_amount == 0) {
		fail("empty offer");
	}
	const ask_amount = ((bid_amount << FRACT_BITS) + inv_price - 1) / inv_price;
	const ret_amount = this.deposit.amount - ask_amount;
	send(dst_addr, bid_amount, bid_currency);
	
	if(ret_amount > 0) {
		send(dst_addr, ret_amount, ask_currency);
	}
}



