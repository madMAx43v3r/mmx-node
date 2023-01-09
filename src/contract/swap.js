
const FRACT_BITS = 64;
const LOCK_DURATION = 8640;

var tokens = [];
var balance = [0, 0];
var user_total = [0, 0];
var fees_paid = [0, 0];
var fees_claimed = [0, 0];
var users = {};
var fee_rate;		// with FRACT_BITS

function init(token, currency, fee_rate_)
{
	push(tokens, token);
	push(tokens, currency);
	fee_rate = fee_rate_;
}

function _get_earned_fees(user) const
{
	const user_share = [0, 0];
	for(var i = 0; i < 2; ++i) {
		if(user.balance[i] > 0) {
			const total_fees = fees_paid[i] - user.last_fees_paid[i];
			user_share[i] = min(
					(2 * total_fees * user.balance[i]) /
					(user_total[i] + user.last_user_total[i]),
					this.balance[tokens[i]] - balance[i]);
		}
	}
	return user_share;
}

function get_earned_fees(address) const public
{
	const user = users[address];
	if(user == null) {
		fail("no such user", 2);
	}
	_get_earned_fees(user);
}

function _payout(user)
{
	const user_share = _get_earned_fees(user);
	for(var i = 0; i < 2; ++i) {
		if(user_share[i] > 0) {
			fees_claimed[i] += user_share[i];
			send(this.user, user_share[i], tokens[i]);
		}
		user.last_user_total[i] = user_total[i];
		user.last_fees_paid[i] = fees_paid[i];
	}
}

function payout() public
{
	const user = users[this.user];
	if(user == null) {
		fail("no such user", 2);
	}
	_payout(user);
}

function add_liquid(i) public payable
{
	if(this.deposit.currency != tokens[i]) {
		fail("currency mismatch", 1);
	}
	var user = users[this.user];
	
	if(user == null) {
		user = {};
		user.balance = [0, 0];
		user.last_fees_paid = [0, 0];
		user.last_user_total = [0, 0];
		users[this.user] = user;
	} else {
		_payout(user);
	}
	const amount = this.deposit.amount;

	balance[i] += amount;
	user_total[i] += amount;
	user.balance[i] += amount;
	user.last_user_total[i] = user_total[i];
	user.unlock_height = this.height + LOCK_DURATION;
}

function rem_liquid(i, amount) public
{
	if(amount == 0) {
		fail("amount == 0");
	}
	const user = users[this.user];
	if(user == null) {
		fail("no such user", 2);
	}
	if(this.height < user.unlock_height + LOCK_DURATION) {
		fail("liquidity still locked", 3);
	}
	if(amount > user.balance[i]) {
		fail("amount > user balance", 4);
	}
	var ret_amount = amount;

	if(balance[i] < user_total[i]) {
		ret_amount = (amount * balance[i]) / user_total[i];

		const k = (i + 1) % 2;
		if(balance[k] > user_total[k]) {
			const trade_amount = ((balance[k] - user_total[k]) * amount) / user_total[i];
			send(this.user, trade_amount, tokens[k]);
			balance[k] -= trade_amount;
		}
	}
	send(this.user, ret_amount, tokens[i]);
	
	user.balance[i] -= amount;
	user_total[i] -= amount;
	balance[i] -= ret_amount;
}

function trade(i, address, min_amount) public payable
{
	if(this.deposit.currency != tokens[i]) {
		fail("currency mismatch", 1);
	}
	const k = (i + 1) % 2;
	if(balance[k] == 0) {
		fail("nothing to buy", 5);
	}
	const amount = this.deposit.amount;
	balance[i] += amount;

	const trade_amount = (amount * balance[k]) / balance[i];
	if(trade_amount < 4) {
		fail("trade amount too small", 6);
	}
	const fee_amount = 1 + (trade_amount * fee_rate) >> FRACT_BITS;
	const actual_amount = trade_amount - fee_amount;
	
	if(actual_amount < min_amount) {
		fail("minimum amount not reached", 7);
	}
	send(bech32(address), actual_amount, tokens[k]);
	
	fees_paid[k] += fee_amount;
	balance[k] -= trade_amount;
}

