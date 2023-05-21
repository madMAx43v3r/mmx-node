
const FRACT_BITS = 64;
const LOCK_DURATION = 8640;	// 24 hours

const fee_rates = [
	1844674407370955,		// 0.0001
	9223372036854776,		// 0.0005
	36893488147419103,		// 0.002
	184467440737095516,		// 0.01
];

var state = [];
var tokens = [];
var volume = [0, 0];
var users = {};

function init(token, currency)
{
	push(tokens, bech32(token));
	push(tokens, bech32(currency));
	
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

function _add_liquid(i, pool_idx, amount)
{
	if(pool_idx >= size(state)) {
		fail("invalid pool_idx", 8);
	}
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
			if(pool_idx != user.pool_idx) {
				fail("pool_idx mismatch", 9);
			}
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
	if(this.deposit.currency != tokens[i]) {
		fail("currency mismatch", 1);
	}
	_add_liquid(i, pool_idx, this.deposit.amount);
}

function _rem_liquid(user, i, amount, do_send = true)
{
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
			send(this.user, ret_amount, tokens[i]);
		}
		balance[i] -= ret_amount;
	}
	if(trade_amount > 0) {
		if(do_send) {
			send(this.user, trade_amount, tokens[k]);
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
	if(amount == 0) {
		fail("amount == 0");
	}
	const user = users[this.user];
	if(user == null) {
		fail("no such user", 2);
	}
	if(!dry_run) {
		if(this.height < user.unlock_height) {
			fail("liquidity still locked", 3);
		}
	}
	if(amount > user.balance[i]) {
		fail("amount > user balance", 4);
	}
	return _rem_liquid(user, i, amount, !dry_run);
}

function rem_all_liquid(dry_run = false) public
{
	const user = users[this.user];
	if(user == null) {
		fail("no such user", 2);
	}
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
	if(pool_idx >= size(state)) {
		fail("invalid pool_idx", 8);
	}
	const user = users[this.user];
	if(user == null) {
		fail("no such user", 2);
	}
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

function trade(i, address, min_amount, num_iter) public payable
{
	if(this.deposit.currency != tokens[i]) {
		fail("currency mismatch", 1);
	}
	const k = (i + 1) % 2;
	const amount = this.deposit.amount;
	const chunk_size = (amount + num_iter - 1) / num_iter;

	const out = [0, 0];
	var actual_amount = 0;
	var amount_left = amount;
	
	for(var iter = 0; iter < num_iter && amount_left > 0; ++iter)
	{
		const amount_i = min(chunk_size, amount_left);

		var entry;
		var best_price = 0;
		
		for(const entry_j of state)
		{
			const balance = entry_j.balance;
			if(balance[k] > 0) {
				const rate = (1 << FRACT_BITS) - entry_j.fee_rate;
				const price = (balance[k] * rate) / (balance[i] + amount_i);
				if(price > best_price) {
					entry = entry_j;
					best_price = price;
				}
			}
		}
		if(entry != null) {
			const price = (entry.balance[k] << FRACT_BITS) / (entry.balance[i] + amount_i);
			const trade_amount = (amount_i * price) >> FRACT_BITS;
			
			const fee_amount = min(((trade_amount * entry.fee_rate) >> FRACT_BITS) + 1, trade_amount);
			actual_amount += trade_amount - fee_amount;
			
			out[0] += trade_amount;
			out[1] += fee_amount;
			
			volume[k] += trade_amount;
			
			entry.balance[i] += amount_i;
			entry.balance[k] -= trade_amount;
			entry.fees_paid[k] += fee_amount;
			
			amount_left -= amount_i;
		}
	}
	
	if(min_amount != null) {
		if(actual_amount < min_amount) {
			fail("minimum amount not reached", 7);
		}
	}
	if(actual_amount == 0) {
		fail("empty trade", 7);
	}
	send(bech32(address), actual_amount, tokens[k]);
	
	return out;
}

