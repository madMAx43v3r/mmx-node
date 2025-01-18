
var source;
var target;
var agent;

function init(source_, agent_, target_)
{
	source = bech32(source_);
	target = bech32(target_);
	agent = bech32(agent_);
}

function deposit() public {}

function unlock(currency) public
{
	assert(this.user == agent, "invalid user");
	
	if(!currency) {
		currency = bech32();
	} else {
		currency = bech32(currency);
	}
	send(target, balance(currency), currency, "mmx_escrow_unlock");
}

function cancel(currency) public
{
	assert(this.user == agent, "invalid user");
	
	if(!currency) {
		currency = bech32();
	} else {
		currency = bech32(currency);
	}
	send(source, balance(currency), currency, "mmx_escrow_revoke");
}
