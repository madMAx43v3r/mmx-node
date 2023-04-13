
var owner;
var target = null;
var unlock_height = 0;
var unlock_delay;

var name;
var server_url = null;
var reward_addr;


function init(name_, owner_, reward_addr_)
{
	name = name_;
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

function is_locked() const
{
	return unlock_height == null || this.height < unlock_height;
}

function lock(target_, unlock_delay_, server_url_)
{
	check_owner();
	
	if(is_locked()) {
		fail("contract still locked", 2);
	}
	target = bech32(target_);
	server_url = server_url_;
	unlock_delay = uint(unlock_delay_);
	unlock_height = null;
}

function unlock()
{
	check_owner();
	
	if(unlock_height == null) {
		unlock_height = this.height + unlock_delay;
	}
}

function claim_all(address, currency)
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

function transfer(owner_)
{
	check_owner();
	
	owner = owner_;
}

function set_reward_addr(reward_addr_)
{
	check_owner();
	
	reward_addr = reward_addr_;
}

function set_server_url(server_url_)
{
	check_owner();
	
	server_url = server_url_;
}

