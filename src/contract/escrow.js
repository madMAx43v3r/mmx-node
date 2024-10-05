
var source;
var target;
var agent;

function init(source_, target_, agent_)
{
	source = bech32(source_);
	target = bech32(target_);
	agent = bech32(agent_);
}

function deposit() public {}

function unlock(currency) public
{
	if(this.user != agent) {
		fail("user != agent");
	}
	send(target, balance(currency), currency, "mmx_escrow_unlock");
}

function revoke(currency) public
{
	if(this.user != agent) {
		fail("user != agent");
	}
	send(source, balance(currency), currency, "mmx_escrow_revoke");
}
