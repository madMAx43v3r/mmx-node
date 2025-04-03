import {equals, sort, reverse, compare} from "std";

// Poker Hand Ranking:
// 0 - High Card
// 1 - One Pair
// 2 - Two Pair
// 3 - Three of a Kind
// 4 - Ace Low Straight
// 5 - Straight
// 6 - Flush
// 7 - Full House
// 8 - Four of a Kind
// 9 - Ace Low Straight Flush
// 10 - Straight Flush

// Poker Suits:
// H - Hearts
// D - Diamonds
// C - Clubs
// S - Spades

var currency;
var blind_bet;
var bet_limit;
var max_players;
var timeout_interval;

var num_reveals = 0;
var num_actions = 0;
var deadline = null;
var round = 0;
var state = 0;          // 0 - waiting for players, 1 - revealing, 2 - betting, 3 - showdown, 4 - finished
var sequence = 1;
var bet_amount = 0;
var is_raise = false;

var player_map = {};
var player_list = [];

var board = null;
var winning_hand = null;
var winning_player = null;

function init(currency_, blind_bet_, bet_limit_, max_players_, timeout_)
{
    currency = bech32(currency_);
    blind_bet = uint(blind_bet_);
    bet_limit = uint(bet_limit_) * blind_bet;
    max_players = uint(max_players_);
    timeout_interval = uint(timeout_);
    assert(max_players >= 2);
    assert(timeout_interval >= 6);
}

function join(name, commit, private_commit) public payable
{
    commit = binary_hex(commit);
    private_commit = binary_hex(private_commit);

    const num_players = size(player_list);

    assert(state == 0, "game already started");
    assert(num_players < max_players, "table full");
    assert(this.user, "missing user");
    assert(this.deposit.currency == currency, "invalid currency");
    assert(this.deposit.amount == blind_bet, "invalid deposit amount");
    assert(is_string(name) && size(name) > 1 && size(name) <= 24, "invalid name");
    assert(size(commit) == 32, "invalid commit");
    assert(size(private_commit) == 32, "invalid private commit");
    assert(!player_map[this.user], "already joined");

    if(!num_players) {
        extend_deadline(3);
    }
    if(deadline) {
        assert(this.height < deadline, "too late");
    }

    const player = {
        name: name,
        address: this.user,
        step: 0,
        bet: blind_bet,
        seed: [],
        commit: commit,
        private_commit: private_commit,
    };
    
    player_map[this.user] = num_players;
    push(player_list, player);

    check_start();
}

function check_start()
{
    if(state == 0) {
        if(size(player_list) >= max_players || is_timeout()) {
            state = 1;  // revealing
            extend_deadline();
        }
    }
}

function reveal(seed, next_commit) public
{
    check_start();
    check_action();

    assert(state == 1, "wrong state");
    assert(this.height < deadline, "too late");
    assert(size(seed) == 32, "invalid seed length");

    const player = get_player(this.user);
    assert(is_active(player), "not active");
    assert(size(player.seed) == round, "already revealed");
    assert(sha256(seed) == player.commit, "invalid seed");

    if(round < 4) {
        next_commit = binary_hex(next_commit);
        assert(size(next_commit) == 32, "invalid commit");
        player.commit = next_commit;
    } else {
        player.commit = null;
    }
    push(player.seed, seed);
    num_reveals++;

    check_reveal();
}

function bet() public
{
    check_reveal();

    assert(state == 2, "wrong state");
    assert(this.height < deadline, "too late");
    assert(this.deposit.currency == currency, "invalid currency");

    const player = get_player(this.user);
    assert(is_active(player), "not active");
    assert(sequence > player.step, "duplicate action");
    
    player.bet += this.deposit.amount;
    assert(player.bet <= bet_limit, "bet limit exceeded");

    if(player.bet > bet_amount) {
        is_raise = true;
        bet_amount = player.bet;
    }
    player.step = sequence;
    num_actions++;
    
    check_action();
}

function check(auto_fold) public
{
    check_reveal();

    assert(state == 2, "wrong state");
    assert(this.height < deadline, "too late");

    const player = get_player(this.user);
    assert(is_active(player), "not active");
    assert(sequence > player.step, "duplicate action");

    if(is_raise && auto_fold) {
        player.fold = true;
    }
    player.step = sequence;
    num_actions++;

    check_action();
}

function fold() public
{
    check_reveal();

    assert(state == 2, "wrong state");
    assert(this.height < deadline, "too late");

    const player = get_player(this.user);
    assert(is_active(player), "not active");
    assert(sequence > player.step, "duplicate action");

    player.fold = true;
    player.step = sequence;
    num_actions++;

    check_action();
}

function show(hand, private_seed) public
{
    assert(state == 3, "wrong state");
    assert(size(hand) == 5, "invalid hand");
    assert(this.height < deadline, "too late");

    const player = get_player(this.user);
    assert(is_active(player), "not active");
    assert(player.private_seed == null, "already shown");
    assert(sha256(private_seed) == player.private_commit, "invalid private seed");

    player.private_seed = private_seed;

    // TODO: check hand
}

function claim() public
{
    assert(state == 3, "wrong state");
    assert(is_timeout(), "too soon");
    
    if(winning_player) {
        send(balance(currency), winning_player.address, currency, "poker_win");
    } else {
        // TODO: return all bets
    }
    state = 4;  // finished
}

function is_timeout() const public
{
    if(deadline) {
        return this.height >= deadline;
    }
    return false;
}

function extend_deadline(factor = 1)
{
    deadline = this.height + factor * timeout_interval;
}

function get_player(address) const public
{
    const index = player_map[address];
    assert(index != null, "not a player");
    return player_list[index];
}

function is_active(player) const public
{
    var curr_round = round;
    if(state == 1 && is_timeout()) {
        curr_round++;
    }
    return size(player.seed) >= curr_round && player.fold == null;
}

function get_active() const public
{
    const active = [];
    for(const player of player_list) {
        if(is_active(player)) {
            push(active, player);
        }
    }
    return active;
}

function get_num_active() const public
{
    var count = 0;
    for(const player of player_list) {
        if(is_active(player)) {
            count++;
        }
    }
    return count;
}

function check_reveal()
{
    if(state == 1) {
        if(num_reveals == get_num_active() || is_timeout()) {
            num_reveals = 0;
            round++;
            state = 2;  // betting
            extend_deadline();
        }
    }
}

function check_action()
{
    if(state == 2) {
        if(num_actions == get_num_active() || is_timeout()) {
            var done = true;
            if(bet_amount) {
                for(const player of get_active()) {
                    if(player.bet < bet_amount) {
                        if(is_raise) {
                            done = false;
                            break;
                        } else {
                            player.fold = true;
                        }
                    }
                }
            }
            if(done) {
                bet_amount = 0;
                state = 1;  // revealing
            }
            is_raise = false;
            sequence++;
            num_actions = 0;
            extend_deadline();
        }
    }
}

// Function to leave the game if nobody else joined after the deadline

function leave() public
{
    assert(state == 0, "game already started");
    assert(is_timeout(), "too early");
    assert(size(player_list) == 1, "cannot leave table");

    const player = get_player(this.user);

    send(this.user, player.bet, currency);

    player_map = {};
    player_list = [];
    deadline = null;
}

// Function to get the rank of a poker hand (5 cards)
// @param hand - array of array representing the poker hand
// @return - array with the rank and sorted values of the hand
//
// Example:
//   hand = [["2", "H"], ["3", "D"], ["4", "C"], ["5", "S"], ["6", "H"]]
//   return => [5, [4, 3, 2, 1, 0]]

function get_rank(hand) const public
{
    assert(size(hand) == 5, "hand must be 5 cards");

    const RANK_MAP = {
        "2": 0, "3": 1, "4": 2, "5": 3, "6": 4, "7": 5, "8": 6, "9": 7, "10": 8,
        "T": 8, "J": 9, "Q": 10, "K": 11, "A": 12
    };
    
    var values = [];
    var suit_map = {"H": 0, "D": 0, "C": 0, "S": 0};
    
    for(const card of hand) {
        const rank = RANK_MAP[card[0]];
        assert(rank != null, "invalid card number");
        assert(suit_map[card[1]] != null, "invalid card suit");
        push(values, rank);
        suit_map[card[1]]++;
    }
    values = reverse(sort(values));
    
    var unique_values = [];     // sorted as well (DSC)
    {
        var prev = null;
        for(const v of values) {
            if(v != prev) {
                push(unique_values, v);
                prev = v;
            }
        }
    }
    
    var straight = false;
    if(size(unique_values) == 5) {
        straight = (unique_values[0] - unique_values[4] == 4);
    }
    const low_straight = equals(unique_values, [12, 3, 2, 1, 0]);

    var flush = false;
    for(const suit of ["H", "D", "C", "S"]) {
        if(suit_map[suit] == 5) {
            flush = true;
        }
    }

    var count = [];
    for(var i = 0; i <= 12; i++) {
        push(count, 0);
    }
    for(const v of values) {
        count[v]++;
    }
    
    var pairs = 0;
    var threes = 0;
    var fours = 0;
    for(const v of unique_values) {
        if(count[v] == 2) pairs++;
        if(count[v] == 3) threes++;
        if(count[v] == 4) fours++;
    }
    
    if(flush && straight) return [10, values];	    // Straight Flush
    if(flush && low_straight) return [9, values];	// Ace Low Straight Flush
    if(fours) return [8, values];				    // Four of a kind
    if(threes && pairs) return [7, values];		    // Full House
    if(flush) return [6, values];				    // Flush
    if(straight) return [5, values];			    // Straight
    if(low_straight) return [4, values];	        // Ace Low Straight
    if(threes) return [3, values];				    // Three of a kind
    if(pairs == 2) return [2, values];			    // Two Pair
    if(pairs == 1) return [1, values];			    // One Pair
    return [0, values];							    // High Card
}

// Function to compare two poker hands
// @param hand - first poker hand (array of array)
// @param other - second poker hand (array of array)
// @return - "GT" if hand wins, "LT" if other wins, "EQ" if tie

function check_win(hand, other) const public
{
    const L = get_rank(hand);
    const R = get_rank(other);

    const res = compare(L[0], R[0]);
    if(res == "EQ") {
        return compare(L[1], R[1]);
    }
    return res;
}

// Returns a random card based on a seed integer
// @return - array with rank and suit of the card

function get_card(seed) const public
{
    seed = uint(seed);
    const RANK_MAP = ["2", "3", "4", "5", "6", "7", "8", "9", "T", "J", "Q", "K", "A"];
    const SUIT_MAP = ["H", "D", "C", "S"];
    return [RANK_MAP[seed % 13], SUIT_MAP[(seed / 13) % 4]];
}
