
const FRACT_BITS = 64;
const LOCK_DURATION = 8640;

const fee_rates = [
	9223372036854776,		// 0.0005
	18446744073709552,		// 0.001
	36893488147419103,		// 0.002
	92233720368547758,		// 0.005
	184467440737095516,		// 0.01
	368934881474191032,		// 0.02
	922337203685477581,		// 0.05
	1844674407370955162		// 0.1
];

var state = [];
var tokens = [];
var users = {};

function init(token, currency)
{
	push(tokens, token);
	push(tokens, currency);
	
	for(var j = 0; j < size(fee_rates); ++j) {
		var out = {};
		out.balance = [0, 0];
		out.user_total = [0, 0];
		out.fees_paid = [0, 0];
		out.fees_claimed = [0, 0];
		push(state, out);
	}
}

function _get_earned_fees(user) const
{
	const user_share = [0, 0];
	for(var i = 0; i < 2; ++i) {
		if(user.balance[i] > 0) {
			const entry = state[user.fee_idx];
			const total_fees = entry.fees_paid[i] - user.last_fees_paid[i];
			if(total_fees > 0) {
				user_share[i] = min(
						(2 * total_fees * user.balance[i]) /
						(entry.user_total[i] + user.last_user_total[i]),
						entry.fees_paid[i] - entry.fees_claimed[i]);
			}
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
	const entry = state[user.fee_idx];
	const user_share = _get_earned_fees(user);
	
	for(var i = 0; i < 2; ++i) {
		if(user_share[i] > 0) {
			entry.fees_claimed[i] += user_share[i];
			send(this.user, user_share[i], tokens[i]);
		}
		user.last_user_total[i] = entry.user_total[i];
		user.last_fees_paid[i] = entry.fees_paid[i];
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

function add_liquid(i, fee_idx) public payable
{
	if(this.deposit.currency != tokens[i]) {
		fail("currency mismatch", 1);
	}
	if(fee_idx >= size(fee_rates)) {
		fail("invalid fee_idx", 8);
	}
	var user = users[this.user];
	
	if(user == null) {
		user = {};
		user.fee_idx = fee_idx;
		user.balance = [0, 0];
		user.last_fees_paid = [0, 0];
		user.last_user_total = [0, 0];
		users[this.user] = user;
	} else {
		if(fee_idx != user.fee_idx) {
			fail("fee_idx mismatch", 9);
		}
		_payout(user);
	}
	const entry = state[user.fee_idx];
	const amount = this.deposit.amount;

	entry.balance[i] += amount;
	entry.user_total[i] += amount;
	
	user.balance[i] += amount;
	user.last_user_total[i] = entry.user_total[i];
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
	const entry = state[user.fee_idx];
	var ret_amount = amount;

	if(entry.balance[i] < entry.user_total[i]) {
		ret_amount = (amount * entry.balance[i]) / entry.user_total[i];

		const k = (i + 1) % 2;
		if(entry.balance[k] > entry.user_total[k]) {
			const trade_amount = ((entry.balance[k] - entry.user_total[k]) * amount) / entry.user_total[i];
			send(this.user, trade_amount, tokens[k]);
			entry.balance[k] -= trade_amount;
		}
	}
	send(this.user, ret_amount, tokens[i]);
	
	user.balance[i] -= amount;
	
	entry.user_total[i] -= amount;
	entry.balance[i] -= ret_amount;
}

function change_pool() public
{
	const user = users[this.user];
	if(user == null) {
		fail("no such user", 2);
	}
	_payout(user);
	
	// TODO
}

function get_total_balance() public const
{
	var total = [0, 0];
	for(const entry of state) {
		total[0] += entry.balance[0];
		total[1] += entry.balance[1];
	}
	return total;
}

function trade(i, address, min_amount) public payable
{
	if(this.deposit.currency != tokens[i]) {
		fail("currency mismatch", 1);
	}
	const total_balance = get_total_balance();
	
	const k = (i + 1) % 2;
	if(total_balance[k] == 0) {
		fail("liquidity pool is empty", 5);
	}
	const amount = this.deposit.amount;

	const price = (total_balance[k] << FRACT_BITS) / (total_balance[i] + amount);
	const trade_amount = (amount * price) >> FRACT_BITS;
	if(trade_amount < 64) {
		fail("trade amount too small", 6);
	}
	var out = {};
	var actual_amount = 0;
	var amount_left = amount;
	var trade_amount_left = trade_amount;
	
	for(var j = 0; j < size(fee_rates) && trade_amount_left > 0; ++j) {
		const entry = state[j];
		
		if(entry.balance[k] > 0) {
			const pool_trade_amount = min(trade_amount_left, entry.balance[k]);
			
			var pool_amount = 0;
			if(pool_trade_amount < trade_amount_left) {
				pool_amount = min((pool_trade_amount << FRACT_BITS) / price, amount_left);
			} else {
				pool_amount = amount_left;
			}
			
			const fee_amount = min(1 + (pool_trade_amount * fee_rates[j]) >> FRACT_BITS, pool_trade_amount);
			actual_amount += pool_trade_amount - fee_amount;
			
			var info = {};
			info.amount = pool_amount;
			info.fee_amount = fee_amount;
			info.trade_amount = pool_trade_amount;
			out[j] = info;
			
			entry.balance[i] += pool_amount;
			entry.balance[k] -= pool_trade_amount;
			entry.fees_paid[k] += fee_amount;
			
			amount_left -= pool_amount;
			trade_amount_left -= pool_trade_amount;
		}
	}
	
	if(min_amount != null) {
		if(actual_amount < min_amount) {
			fail("minimum amount not reached", 7);
		}
	}
	send(bech32(address), actual_amount, tokens[k]);
	
	return out;
}

