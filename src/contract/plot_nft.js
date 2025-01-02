
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
	assert(this.user == owner, "user not owner", 1);
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
	return string_bech32(addr);
}

function lock(target_, server_url_) public
{
	check_owner();
	
	assert(!is_locked(), "contract still locked", 2);
	
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

function claim_all(currency) public
{
	if(is_locked()) {
		assert(this.user == target, "user not target", 3);
	} else {
		check_owner();
	}
	if(currency) {
		currency = bech32(currency);
	} else {
		currency = bech32();
	}
	send(this.user, balance(currency), currency, "mmx_plot_nft_claim");
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

