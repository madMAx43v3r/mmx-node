
const FRACT_BITS = 64;

var owner;
var partner;			// optional
var bid_currency;
var ask_currency;
var inv_price;			// [bid / ask]
var last_update;		// height

function init(owner_, bid_currency_, ask_currency_, inv_price_, partner_)
{
	owner = bech32(owner_);
	bid_currency = bech32(bid_currency_);
	ask_currency = bech32(ask_currency_);
	inv_price = uint(inv_price_);
	
	if(partner_ != null) {
		partner = bech32(partner_);
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
	
	send(owner, balance(bid_currency), bid_currency, "mmx_offer_cancel");
}

function withdraw() public
{
	check_owner();
	
	send(owner, balance(ask_currency), ask_currency, "mmx_offer_withdraw");
}

function recover(amount, currency) public
{
	check_owner();
	
	send(owner, uint(amount), bech32(currency));
}

function set_price(new_price) public
{
	check_owner();
	
	new_price = uint(new_price);
	
	if(new_price != inv_price) {
		inv_price = new_price;
		last_update = this.height;
	}
}

function trade(dst_addr, price) public payable
{
	check_partner();
	
	if(uint(price) != inv_price) {
		fail("price changed", 6);
	}
	if(this.deposit.currency != ask_currency) {
		fail("currency mismatch", 3);
	}
	const bid_amount = (this.deposit.amount * inv_price) >> FRACT_BITS;
	if(bid_amount == 0) {
		fail("empty trade", 4);
	}
	send(bech32(dst_addr), bid_amount, bid_currency, "mmx_offer_trade");
}

function accept(dst_addr, price) public payable
{
	check_partner();
	
	if(uint(price) != inv_price) {
		fail("price changed", 6);
	}
	if(this.deposit.currency != ask_currency) {
		fail("currency mismatch", 3);
	}
	// take whatever is left in case another trade happened before
	const bid_amount = min((this.deposit.amount * inv_price) >> FRACT_BITS, balance(bid_currency));
	if(bid_amount == 0) {
		fail("empty offer", 5);
	}
	const ask_amount = ((bid_amount << FRACT_BITS) + inv_price - 1) / inv_price;	// round up
	const ret_amount = this.deposit.amount - ask_amount;	// will fail on underflow
	
	dst_addr = bech32(dst_addr);
	send(dst_addr, bid_amount, bid_currency, "mmx_offer_accept");
	send(dst_addr, ret_amount, ask_currency, "mmx_offer_accept_return");
}



