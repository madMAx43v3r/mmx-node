
var source;
var target;

function init(source_, target_)
{
	source = bech32(source_);
	target = bech32(target_);
}

function receive(currency) public
{
	assert(this.user == target, "user not target", 1);
	
	currency = bech32(currency);
	
	send(target, balance(currency), currency);
}

function cancel(currency) public
{
	assert(this.user == source, "user not source", 2);
	
	currency = bech32(currency);
	
	send(source, balance(currency), currency);
}
