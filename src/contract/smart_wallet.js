
var owner;
var plans = {};

function init(owner_)
{
	owner = bech32(owner_);
}

function check_owner()
{
	assert(this.user == owner, "user not owner", 1);
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
		assert(!prev.active, "plan already exists", 2);
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
	assert(plan, "no such plan", 3);
	
	plan.active = false;
}

function plan_payment(name) public
{
	const plan = plans[name];
	assert(plan, "no such plan", 3);
	
	assert(plan.active, "plan has been canceled", 4);
	
	assert(this.height >= plan.next_pay, "payment too soon", 5);
	
	plan.next_pay += plan.interval;
	
	send(plan.target, plan.amount, plan.currency, plan.memo);
}


