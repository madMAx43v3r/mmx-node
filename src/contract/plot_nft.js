
const MAX_UNLOCK_DELAY = 10000;

var owner;
var target;
var unlock_height = 0;
var unlock_delay;

var server_url;
var reward_addr;

function init(owner_, reward_addr_)
{
	owner = bech32(owner_);
	
	if(reward_addr_ != null) {
		reward_addr = bech32(reward_addr_);
	} else {
		reward_addr = owner;
	}
}

function check_owner() const
{
	if(this.user != owner) {
		fail("user != owner", 1);
	}
}

function is_locked() const public
{
	return unlock_height == null || this.height < unlock_height;
}

function lock(target_, unlock_delay_, server_url_) public
{
	check_owner();
	
	if(is_locked()) {
		fail("contract still locked", 2);
	}
	unlock_delay_ = uint(unlock_delay_);
	if(unlock_delay_ > MAX_UNLOCK_DELAY) {
		fail("unlock delay too high", 4);
	}
	target = bech32(target_);
	server_url = server_url_;
	unlock_delay = unlock_delay_;
	unlock_height = null;
}

function unlock() public
{
	check_owner();
	
	if(unlock_height == null) {
		unlock_height = this.height + unlock_delay;
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
	if(currency != null) {
		currency = bech32(currency);
	} else {
		currency = 0;
	}
	send(bech32(address), this.balance[currency], currency);
}

function transfer(owner_) public
{
	check_owner();
	
	owner = owner_;
}

function set_reward_addr(reward_addr_) public
{
	check_owner();
	
	reward_addr = reward_addr_;
}

function set_server_url(server_url_) public
{
	check_owner();
	
	server_url = server_url_;
}

