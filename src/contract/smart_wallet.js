
var owner;
var plans = {};

function init(owner_)
{
	owner = bech32(owner_);
}

function check_owner()
{
	if(this.user != owner) {
		fail("user != owner", 1);
	}
}

function transfer(address, amount, currency, memo) public
{
	check_owner();
	
	send(bech32(address), uint(amount), bech32(currency), memo);
}

function add_plan(name, amount, currency, target, memo, interval, start) public
{
	check_owner();
	
	const prev = plans[name];
	if(prev) {
		if(prev.active) {
			fail("plan already exists", 2);
		}
	}
	if(memo != null) {
		memo = to_string(memo);
	}
	if(start == null) {
		start = this.height;
	}
	plans[name] = {
		amount: amount,
		currency: bech32(currency),
		target: bech32(target),
		memo: memo,
		interval: interval,
		next_pay: start,
		active: true
	};
}

function cancel_plan(name) public
{
	check_owner();
	
	const plan = plans[name];
	if(!plan) {
		fail("no such plan", 3);
	}
	plan.active = false;
}

function plan_payment(name) public
{
	const plan = plans[name];
	if(!plan) {
		fail("no such plan", 3);
	}
	if(!plan.active) {
		fail("plan has been canceled", 4);
	}
	if(this.height < plan.next_pay) {
		fail("payment too soon", 5);
	}
	plan.next_pay += plan.interval;
	
	send(plan.target, plan.amount, plan.currency, plan.memo);
}


