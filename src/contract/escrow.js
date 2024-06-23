
var source;
var beneficiary;
var agent;

function init(source_, beneficiary_, agent_)
{
	source = bech32(source_);
	beneficiary = bech32(beneficiary_);
	agent = bech32(agent_);
}

function deposit() public {}

function unlock(currency) public
{
	if(this.user != agent) {
		fail("user != agent");
	}
	send(beneficiary, this.balance[currency], currency, "mmx_escrow_unlock");
}

function revoke(currency) public
{
	if(this.user != agent) {
		fail("user != agent");
	}
	send(source, this.balance[currency], currency, "mmx_escrow_revoke");
}
