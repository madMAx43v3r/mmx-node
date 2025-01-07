
const FRACT_BITS = 64;
const LOCK_DURATION = 8640;	// 24 hours

const fee_rates = [
	9223372036854776,		// 0.0005
	46116860184273879,		// 0.0025
	184467440737095516,		// 0.01
	922337203685477581,		// 0.05
];

var tokens;
var state = [];
var volume = [0, 0];
var users = {};

// common variables:
// `i`: index of deposited token, 0 = token, 1 = currency
// `k`: index of token to be traded for, the opposite of `i` ((i == 0 && k == 1) || (i == 1 && k == 0))

function init(token, currency)
{
	tokens = [bech32(token), bech32(currency)];
	
	for(const fee_rate of fee_rates) {
		var out = {};
		out.balance = [0, 0];
		out.user_total = [0, 0];
		out.fee_rate = fee_rate;
		out.fees_paid = [0, 0];
		out.fees_claimed = [0, 0];
		push(state, out);
	}
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
	assert(user);
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
			send(this.user, amount, tokens[i], "mmx_swap_payout");
		}
		user.last_fees_paid[i] = entry.fees_paid[i];
		user.last_user_total[i] = entry.user_total[i];
	}
	return user_share;
}

function payout() public
{
	const user = users[this.user];
	assert(user);
	return _payout(user);
}

function _add_liquid(i, pool_idx, amount)
{
	assert(i < 2);
	assert(pool_idx < size(state));
	
	const entry = state[pool_idx];
	
	var user = users[this.user];
	if(user == null) {
		user = {};
		user.pool_idx = pool_idx;
		user.balance = [0, 0];
		user.last_fees_paid = [0, 0];
		user.last_user_total = [0, 0];
		users[this.user] = user;
	} else {
		if(user.balance[0] > 0 || user.balance[1] > 0) {
			assert(pool_idx == user.pool_idx, "pool_idx mismatch");
			// need to payout now, to avoid fee stealing
			_payout(user);
		}
		user.pool_idx = pool_idx;
	}
	entry.balance[i] += amount;
	entry.user_total[i] += amount;
	
	for(var k = 0; k < 2; ++k) {
		user.last_fees_paid[k] = entry.fees_paid[k];
		user.last_user_total[k] = entry.user_total[k];
	}
	user.balance[i] += amount;
	user.unlock_height = this.height + LOCK_DURATION;
}

function add_liquid(i, pool_idx) public payable
{
	assert(i < 2);
	assert(this.deposit.currency == tokens[i]);
	
	_add_liquid(i, pool_idx, this.deposit.amount);
}

function _rem_liquid(user, i, amount, do_send = true)
{
	assert(i < 2);
	
	const k = (i + 1) % 2;
	const entry = state[user.pool_idx];
	const balance = entry.balance;
	const user_total = entry.user_total;
	
	var ret_amount = (balance[i] * amount) / user_total[i];
	var trade_amount = 0;
	
	if(balance[i] < user_total[i]) {
		if(balance[k] > user_total[k]) {
			// token k was bought
			trade_amount = ((balance[k] - user_total[k]) * amount) / user_total[i];
		}
	} else if(balance[k] < user_total[k]) {
		// no trade happened
		ret_amount = amount;
	}
	
	if(ret_amount > 0) {
		if(do_send) {
			send(this.user, ret_amount, tokens[i], "mmx_swap_rem_liquid");
		}
		balance[i] -= ret_amount;
	}
	if(trade_amount > 0) {
		if(do_send) {
			send(this.user, trade_amount, tokens[k], "mmx_swap_rem_liquid_trade");
		}
		balance[k] -= trade_amount;
	}
	user.balance[i] -= amount;
	user_total[i] -= amount;
	
	const out = [0, 0];
	out[i] = ret_amount;
	out[k] = trade_amount;
	return out;
}

function rem_liquid(i, amount, dry_run = false) public
{
	assert(i < 2);
	
	amount = uint(amount);
	assert(amount);
	
	const user = users[this.user];
	assert(user);
	
	if(!dry_run) {
		assert(this.height >= user.unlock_height, "liquidity still locked", 3);
	}
	assert(amount <= user.balance[i], "amount > user balance", 4);
	
	return _rem_liquid(user, i, amount, !dry_run);
}

function rem_all_liquid(dry_run = false) public
{
	const user = users[this.user];
	assert(user);
	
	_payout(user);
	
	const out = [0, 0];
	for(var i = 0; i < 2; ++i) {
		const amount = user.balance[i];
		if(amount > 0) {
			const ret = rem_liquid(i, amount, dry_run);
			for(var k = 0; k < 2; ++k) {
				out[k] += ret[k];
			}
		}
	}
	return out;
}

function switch_pool(pool_idx) public
{
	assert(pool_idx < size(state));
	
	const user = users[this.user];
	assert(user);
	assert(this.height >= user.unlock_height, "liquidity still locked", 3);
	
	_payout(user);
	
	const new_amount = [0, 0];
	
	for(var i = 0; i < 2; ++i) {
		const amount = user.balance[i];
		if(amount > 0) {
			const ret = _rem_liquid(user, i, amount, false);
			new_amount[0] += ret[0];
			new_amount[1] += ret[1];
		}
	}
	for(var i = 0; i < 2; ++i) {
		if(new_amount[i] > 0) {
			_add_liquid(i, pool_idx, new_amount[i]);
		}
	}
	return new_amount;
}

function get_total_balance() public const
{
	const total = [0, 0];
	for(const entry of state) {
		total[0] += entry.balance[0];
		total[1] += entry.balance[1];
	}
	return total;
}

function trade(i, address, min_trade, num_iter) public payable
{
	assert(i < 2);
	assert(num_iter > 0);
	assert(this.deposit.currency == tokens[i]);
	
	const k = (i + 1) % 2;
	const amount = this.deposit.amount;
	const chunk_size = (amount + num_iter - 1) / num_iter;

	const out = [0, 0];
	var amount_left = amount;
	var total_actual_amount = 0;
	
	for(var iter = 0; iter < num_iter && amount_left > 0; ++iter)
	{
		const amount_i = min(chunk_size, amount_left);

		var entry;
		var fee_amount_i = 0;
		var trade_amount_i = 0;
		var actual_amount_i = 0;
		
		for(const entry_j of state)
		{
			const balance = entry_j.balance;
			if(balance[k] > 0) {
				const trade_amount = balance[k] - (balance[i] * balance[k]) / (balance[i] + amount_i);
				
				const fee_amount = min(((trade_amount * entry_j.fee_rate) >> FRACT_BITS) + 1, trade_amount);
				
				const actual_amount = trade_amount - fee_amount;
				if(actual_amount > actual_amount_i) {
					entry = entry_j;
					fee_amount_i = fee_amount;
					trade_amount_i = trade_amount;
					actual_amount_i = actual_amount;
				}
			}
		}
		if(entry) {
			total_actual_amount += actual_amount_i;
			
			out[0] += trade_amount_i;
			out[1] += fee_amount_i;
			
			volume[k] += trade_amount_i;
			
			entry.balance[i] += amount_i;
			entry.balance[k] -= trade_amount_i;
			entry.fees_paid[k] += fee_amount_i;
			
			amount_left -= amount_i;
		}
	}
	
	assert(amount_left == 0);
	
	if(min_trade) {
		assert(total_actual_amount >= uint(min_trade), "minimum trade amount not reached", 7);
	}
	assert(total_actual_amount > 0, "empty trade", 7);
	
	send(bech32(address), total_actual_amount, tokens[k], "mmx_swap_trade");
	
	return out;
}

