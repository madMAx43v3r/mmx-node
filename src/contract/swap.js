
const FRACT_BITS = 64;

const fee_rates = [
	1844674407370955,		// 0.0001
	3689348814741910,		// 0.0002
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
var volume = [0, 0];
var users = {};
var unfreeze_height;

function init(token, currency, unfreeze_delay)
{
	push(tokens, bech32(token));
	push(tokens, bech32(currency));
	
	for(var j = 0; j < size(fee_rates); ++j) {
		var out = {};
		out.balance = [0, 0];
		out.user_total = [0, 0];
		out.fees_paid = [0, 0];
		out.fees_claimed = [0, 0];
		push(state, out);
	}
	unfreeze_height = this.height + unfreeze_delay;
}

function _get_earned_fees(user) const
{
	const entry = state[user.pool_idx];
	
	const user_share = [0, 0];
	for(var i = 0; i < 2; ++i) {
		if(user.balance[i] > 0) {
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
	const user = users[bech32(address)];
	if(user == null) {
		fail("no such user", 2);
	}
	return _get_earned_fees(user);
}

function _payout(user)
{
	const entry = state[user.pool_idx];
	const user_share = _get_earned_fees(user);
	
	for(var i = 0; i < 2; ++i) {
		const amount = user_share[i];
		if(amount > 0) {
			entry.fees_claimed[i] += amount;
			send(this.user, amount, tokens[i]);
		}
		user.last_fees_paid[i] = entry.fees_paid[i];
	}
	return user_share;
}

function payout() public
{
	const user = users[this.user];
	if(user == null) {
		fail("no such user", 2);
	}
	return _payout(user);
}

function _disable(user)
{
	_payout(user);
	
	const entry = state[user.pool_idx];
	
	if(this.height >= unfreeze_height)
	{
		const user_sum = user.balance[0] + user.balance[1];
		const pool_sum = entry.balance[0] + entry.balance[1];
		const user_ratio = (user_sum << FRACT_BITS) / pool_sum;
		
		for(var i = 0; i < 2; ++i) {
			const amount = (entry.balance[i] * user_ratio) >> FRACT_BITS;
			
			entry.balance[i] -= amount;
			entry.user_total[i] -= user.balance[i];
			
			user.balance[i] = amount;
		}
	}
	else {
		for(var i = 0; i < 2; ++i) {
			const amount = user.balance[i];
			entry.balance[i] -= amount;
			entry.user_total[i] -= amount;
		}
	}
	user.pool_idx = null;
	
	return user.balance;
}

function disable() public
{
	var user = users[this.user];
	if(user == null) {
		return;
	}
	if(user.pool_idx == null) {
		return;
	}
	return _disable(user);
}

function _activate(user, pool_idx)
{
	if(user.pool_idx != null) {
		fail("already activated", 3);
	}
	const total_balance = get_total_balance();
	
	const amount = [0, 0];
	const balance = user.balance;
	
	if(this.height >= unfreeze_height && total_balance[0] > 0 && total_balance[1] > 0)
	{
		const price = (total_balance[0] << FRACT_BITS) / (total_balance[1]);
		
		amount[0] = (balance[1] * price) >> FRACT_BITS;
		
		if(amount[0] <= balance[0]) {
			amount[1] = balance[1];
		} else {
			amount[0] = balance[0];
			amount[1] = (balance[0] << FRACT_BITS) / price;
		}
	}
	else {
		amount[0] = balance[0];
		amount[1] = balance[1];
	}
	const entry = state[pool_idx];
	
	if(entry == null) {
		fail("invalid pool_idx", 8);
	}
	
	for(var i = 0; i < 2; ++i) {
		send(this.user, balance[i] - amount[i], tokens[i]);
		
		entry.balance[i] += amount[i];
		entry.user_total[i] += amount[i];
		
		balance[i] = amount[i];
		user.last_fees_paid[i] = entry.fees_paid[i];
		user.last_user_total[i] = entry.user_total[i];
	}
	
// 	for(var i = 0; i < 2; ++i) {
// 		const half_amount = amount_left[i] / 2;
// 		if(half_amount > 0) {
// 			const k = (i + 1) % 2;
// 			const trade_amount = _trade(i, k, half_amount, null)[2];
// 			
// 			balance[i] += half_amount;
// 			balance[k] += trade_amount;
// 			entry.balance[i] += half_amount;
// 			entry.balance[k] += trade_amount;
// 			entry.user_total[i] += half_amount;
// 			entry.user_total[k] += trade_amount;
// 		}
// 	}
	user.pool_idx = pool_idx;
	
	return balance;
}

function activate(pool_idx) public
{
	var user = users[this.user];
	if(user == null) {
		fail("no such user", 2);
	}
	return _activate(user, pool_idx);
}

function add_liquid(i) public payable
{
	if(this.deposit.currency != tokens[i]) {
		fail("currency mismatch", 1);
	}
	
	var user = users[this.user];
	if(user == null) {
		user = {};
		user.pool_idx = null;
		user.balance = [0, 0];
		user.last_fees_paid = [0, 0];
		user.last_user_total = [0, 0];
		users[this.user] = user;
	} else {
		if(user.pool_idx != null) {
			fail("already activated", 3);
		}
	}
	user.balance[i] += this.deposit.amount;
}

function rem_all_liquid() public
{
	const user = users[this.user];
	if(user == null) {
		fail("no such user", 2);
	}
	_disable(user);
	
	const out = clone(user.balance);
	
	for(var i = 0; i < 2; ++i) {
		const amount = user.balance[i];
		send(this.user, amount, tokens[i]);
		user.balance[i] = 0;
	}
	return out;
}

function switch_pool(pool_idx) public
{
	const user = users[this.user];
	if(user == null) {
		fail("no such user", 2);
	}
	_disable(user);
	_activate(user, pool_idx);
	
	return user.balance;
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

function _trade(i, k, amount, min_amount) public payable
{
	const total_balance = get_total_balance();
	
	const price = (total_balance[k] << FRACT_BITS) / (total_balance[i] + amount);
	
	const trade_amount = (amount * price) >> FRACT_BITS;
	
	var out = [0, 0, 0];
	var amount_left = amount;
	var trade_amount_left = trade_amount;
	
	for(var j = 0; j < size(state) && trade_amount_left > 0; ++j)
	{
		const entry = state[j];
		
		if(entry.balance[k] > 0) {
			const pool_trade_amount = min(trade_amount_left, entry.balance[k]);
			
			var pool_amount = 0;
			if(pool_trade_amount < trade_amount_left) {
				pool_amount = min((pool_trade_amount << FRACT_BITS) / price, amount_left);
			} else {
				pool_amount = amount_left;
			}
			const fee_amount = min(1 + ((pool_trade_amount * fee_rates[j]) >> FRACT_BITS), pool_trade_amount);
			
			out[0] += pool_trade_amount;
			out[1] += fee_amount;
			
			entry.balance[i] += pool_amount;
			entry.balance[k] -= pool_trade_amount;
			entry.fees_paid[k] += fee_amount;
			
			amount_left -= pool_amount;
			trade_amount_left -= pool_trade_amount;
		}
	}
	out[2] = out[0] - out[1];
	
	if(min_amount != null) {
		if(out[2] < min_amount) {
			fail("minimum amount not reached", 7);
		}
	}
	volume[k] += trade_amount;
	
	return out;
}

function trade(i, address, min_amount) public payable
{
	if(this.height < unfreeze_height) {
		fail("swap still freezed", 4);
	}
	if(this.deposit.currency != tokens[i]) {
		fail("currency mismatch", 1);
	}
	const k = (i + 1) % 2;
	const out = _trade(i, k, this.deposit.amount, min_amount);
	
	send(bech32(address), out[2], tokens[k]);
	
	return out;
}

