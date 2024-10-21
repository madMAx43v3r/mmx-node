
const UNLOCK_DELAY = 256;

var owner;
var target;
var unlock_height = 0;
var server_url;

function init(owner_)
{
	owner = bech32(owner_);
}

function check_owner() const
{
	if(this.user != owner) {
		fail("user != owner", 1);
	}
}

function is_locked() const public
{
	var res = (unlock_height == null);
	if(!res) {
		res = (this.height < unlock_height);
	}
	return res;
}

function mmx_reward_target(farmer) const public
{
	var addr = owner;
	if(is_locked()) {
		addr = target;
	} else if(farmer) {
		addr = bech32(farmer);
	}
	return to_string_bech32(addr);
}

function lock(target_, server_url_) public
{
	check_owner();
	
	if(is_locked()) {
		fail("contract still locked", 2);
	}
	target = bech32(target_);
	server_url = server_url_;
	unlock_height = null;
}

function unlock() public
{
	check_owner();
	
	if(unlock_height == null) {
		unlock_height = this.height + UNLOCK_DELAY;
	}
}

function claim_all(address, currency) public
{
	if(is_locked()) {
		if(this.user != target) {
			fail("user != target", 3);
		}
	} else {
		check_owner();
	}
	if(currency) {
		currency = bech32(currency);
	} else {
		currency = bech32();
	}
	const amount = this.balance[currency];
	if(amount == 0) {
		fail("nothing to claim", 5);
	}
	send(bech32(address), amount, currency, "mmx_plot_nft_claim");
}

function transfer(owner_) public
{
	check_owner();
	
	owner = bech32(owner_);
}

function set_server_url(server_url_) public
{
	check_owner();
	
	server_url = server_url_;
}

