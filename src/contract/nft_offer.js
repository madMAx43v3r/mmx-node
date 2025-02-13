
const FRACT_BITS = 64;
const UPDATE_INTERVAL = 1080;

var owner;
var partner;			// optional
var nft_contract;
var ask_currency;
var price;
var last_update;		// height

function init(owner_, nft_contract_, ask_currency_, price_, partner_)
{
	owner = bech32(owner_);
	nft_contract = bech32(nft_contract_);
	ask_currency = bech32(ask_currency_);
	price = uint(price_);
	
	assert(price > 0);
	assert(nft_contract != ask_currency);
	
	if(partner_) {
		partner = bech32(partner_);
	}
	last_update = this.height;
}

function check_owner()
{
	assert(this.user == owner, "user not owner", 1);
}

function check_partner()
{
	if(partner) {
		assert(this.user == partner, "user not partner", 2);
	}
}

function deposit() public
{
	check_owner();
}

function cancel() public
{
	check_owner();
	
	send(owner, balance(nft_contract), nft_contract, "mmx_offer_cancel");
}

function recover(amount, currency) public
{
	check_owner();
	
	send(owner, uint(amount), bech32(currency), "mmx_offer_recover");
}

function set_price(new_price) public
{
	check_owner();
	
	assert(this.height - last_update >= UPDATE_INTERVAL, "update too soon", 7);
	
	new_price = uint(new_price);
	
	assert(new_price > 0);
	
	if(new_price != price) {
		price = new_price;
		last_update = this.height;
	}
}

function accept(dst_addr, price_) public payable
{
	check_partner();
	
	assert(uint(price_) == price, "price changed", 6);
	assert(balance(nft_contract) >= 1, "empty offer", 5);
	
	assert(this.deposit.currency == ask_currency, "currency mismatch", 3);
	assert(this.deposit.amount == price, "invalid bid amount", 4);
	
	send(owner, price, ask_currency, "mmx_offer_proceeds");
	send(bech32(dst_addr), 1, nft_contract, "mmx_offer_accept");
}



