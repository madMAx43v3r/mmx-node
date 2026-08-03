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
var deadline = null;
var round = 0;
var state = 0;          // 0 - waiting for players, 1 - revealing, 2 - betting, 3 - showdown, 4 - finished
var sequence = 1;
var bet_amount = 0;
var is_raise = false;

var player_map = {};
var player_list = [];

var board = null;
var final_pot = null;
var global_seed = null;
var winning_hand = null;
var winning_players = null;

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
    assert(state == 0, "game already started");
    assert(!is_timeout(), "too late");

    const num_players = size(player_list);

    assert(num_players < max_players, "table full");
    assert(this.user, "missing user");
    assert(this.deposit.currency == currency, "invalid currency");
    assert(this.deposit.amount == blind_bet, "invalid deposit amount");

    commit = binary_hex(commit);
    private_commit = binary_hex(private_commit);

    assert(size(commit) == 32, "invalid commit");
    assert(size(private_commit) == 32, "invalid private commit");
    assert(is_string(name) && size(name) > 0 && size(name) <= 24, "invalid name");
    assert(player_map[this.user] == null, "already joined");

    const player = {
        name: name,
        address: this.user,
        step: 0,
        bet: blind_bet,
        seed: [],
        commit: commit,
        private_commit: private_commit,
    };
    
    if(num_players == 1) {
        extend_deadline(3);
    }
    player_map[this.user] = num_players;
    push(player_list, player);

    check_start();
}

function check_start()
{
    if(state == 0) {
        const num_players = size(player_list);
        if(num_players > 1) {
            if(num_players >= max_players || is_timeout()) {
                state = 1;  // revealing
                extend_deadline();
            }
        }
    }
}

function reveal(seed, next_commit) public
{
    check_start();
    check_action();

    seed = binary_hex(seed);

    assert(state == 1, "wrong state");
    assert(!is_timeout(), "too late");
    assert(size(seed) == 32, "seed must be 32 bytes");

    const player = get_player(this.user, true);
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

function bet() public payable
{
    check_reveal();

    assert(state == 2, "wrong state");
    assert(!is_timeout(), "too late");
    assert(this.deposit.currency == currency, "invalid currency");

    const player = get_player(this.user, true);
    assert(sequence > player.step, "duplicate action");
    
    player.bet += this.deposit.amount;
    assert(player.bet <= bet_limit, "bet limit exceeded");

    if(player.bet > bet_amount) {
        is_raise = true;
        bet_amount = player.bet;
    }
    player.step = sequence;
    
    check_action();
}

function check(auto_fold) public
{
    check_reveal();

    assert(state == 2, "wrong state");
    assert(!is_timeout(), "too late");

    const player = get_player(this.user, true);
    assert(sequence > player.step, "duplicate action");

    if(is_raise && auto_fold) {
        player.fold = true;
    }
    player.step = sequence;

    check_action();
}

function fold() public
{
    check_reveal();

    assert(state == 2, "wrong state");
    assert(!is_timeout(), "too late");

    const player = get_player(this.user, true);
    assert(sequence > player.step, "duplicate action");

    player.fold = true;
    player.step = sequence;

    check_action();
}

function show(hand, private_seed) public
{
    check_action();

    private_seed = binary_hex(private_seed);

    assert(state == 3, "wrong state");
    assert(!is_timeout(), "too late");
    assert(size(hand) == 5, "invalid hand");
    assert(size(private_seed) == 32, "private seed must be 32 bytes");

    const player = get_player(this.user, true);
    assert(!player.private_seed, "already shown");
    assert(sha256(private_seed) == player.private_commit, "invalid private seed");

    player.private_seed = private_seed;

    const source = sha256(concat(global_seed, private_seed));
    // Pocket cards must only depend on the first-round global seed and the
    // player's private seed, so the player can know them before the board.
    const pocket = deal_cards([
        sha256(concat(binary_hex("A1"), source)),
        sha256(concat(binary_hex("A2"), source))
    ]);
    const cards = select_hand(board, pocket, hand);
    const rank = get_rank(cards);

    if(winning_hand) {
        const res = compare_rank(rank, winning_hand);
        if(res == "GT") {
            winning_hand = rank;
            winning_players = [player];
        } else if(res == "EQ") {
            push(winning_players, player);
        }
    } else {
        winning_hand = rank;
        winning_players = [player];
    }
    num_reveals++;
}

function claim() public
{
    check_start();      // handle total timeout
    check_finish();

    assert(state == 4, "wrong state");

    var user = null;
    var amount = 0;
    
    if(winning_players) {
        const winners = get_winners();
        const index = find_player(winners, this.user);
        if(index != null) {
            user = winners[index];
            amount = get_split_amount(final_pot, size(winners), index);
        }
    } else {
        const active = get_active();
        const num_active = size(active);
        if(num_active > 0) {
            // nobody showed their hand, split the pot among remaining players
            const index = find_player(active, this.user);
            if(index != null) {
                user = active[index];
                amount = get_split_amount(final_pot, num_active, index);
            }
        } else {
            // everybody folded or timed out, return bets to each player
            user = get_player(this.user);
            amount = user.bet;
        }
    }

    assert(user, "cannot claim");
    assert(!user.claim, "already claimed");

    send(this.user, amount, currency, "poker_win");
    user.claim = true;
}

function get_winners() const
{
    const winners = [];
    for(const player of player_list) {
        for(const winner of winning_players) {
            if(player.address == winner.address) {
                push(winners, player);
            }
        }
    }
    return winners;
}

function find_player(players, address) const
{
    for(var i = 0; i < size(players); i++) {
        if(players[i].address == address) {
            return i;
        }
    }
    return null;
}

function get_split_amount(total, count, index) const public
{
    total = uint(total);
    count = uint(count);
    index = uint(index);

    assert(count > 0, "invalid split count");
    assert(index < count, "invalid split index");

    var amount = total / count;
    if(index < total % count) {
        amount++;
    }
    return amount;
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

function get_player(address, active_only = false) const
{
    const index = player_map[address];
    assert(index != null, "not a player");

    const player = player_list[index];
    if(active_only) {
        assert(is_active(player), "player folded or timed out");
    }
    return player;
}

function get_player_status(address) const public
{
    const player = get_player(bech32(address));
    return {
        bet: player.bet,
        folded: bool(player.fold),
        revealed: size(player.seed),
        shown: bool(player.private_seed),
        claimed: bool(player.claim),
    };
}

function is_active(player) const
{
    if(player.fold) {
        return false;
    }
    var curr_round = round;
    if(state == 1 && is_timeout()) {
        curr_round++;
    }
    return size(player.seed) >= curr_round;
}

function get_active() const
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
            round++;
            state = 2;  // betting
            num_reveals = 0;
            extend_deadline();
        }
    }
}

function check_action()
{
    if(state == 2) {
        if(all_active_acted() || is_timeout()) {
            var done = true;
            if(bet_amount) {
                for(const player of get_active()) {
                    if(player.bet < bet_amount) {
                        if(is_raise) {
                            done = false;
                        } else {
                            player.fold = true;
                        }
                    }
                }
            }
            if(done) {
                bet_amount = 0;
                if(round < 4) {
                    state = 1;  // revealing
                } else {
                    state = 3;  // showdown
                    compute();
                }
            }
            is_raise = false;
            sequence++;
            extend_deadline();
        }
    }
}

function all_active_acted() const
{
    for(const player of player_list) {
        if(is_active(player) && player.step < sequence) {
            return false;
        }
    }
    return true;
}

function check_finish()
{
    if(state == 0 || final_pot) {
        return;
    }
    const num_active = get_num_active();
    if(state == 3 || num_active == 0) {
        if(num_reveals == num_active || is_timeout()) {
            final_pot = balance(currency);
            state = 4;  // finished
        }
    }
}

// Function to leave the table if nobody else joined

function leave() public
{
    assert(state == 0, "game already started");
    assert(is_timeout(), "too early");
    assert(size(player_list) == 1, "cannot leave table");

    const player = get_player(this.user);

    send(this.user, player.bet, currency);

    player_map = {};
    player_list = [];
}

// Function to get the rank of a poker hand (5 cards)
// @param hand - array of array representing the poker hand
// @return - array with the rank and sorted values of the hand
//
// Example:
//   hand = [["2", "H"], ["3", "D"], ["4", "C"], ["5", "S"], ["6", "H"]]
//   return => [5, [4]]

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
    
    const pairs = [];
    const singles = [];
    var threes = null;
    var fours = null;
    for(const v of unique_values) {
        if(count[v] == 1) push(singles, v);
        if(count[v] == 2) push(pairs, v);
        if(count[v] == 3) threes = v;
        if(count[v] == 4) fours = v;
    }

    if(flush && straight) return [10, [values[0]]];  // Straight Flush
    if(flush && low_straight) return [9, [3]];       // Ace Low Straight Flush
    if(fours != null) return [8, [fours, singles[0]]];
    if(threes != null && size(pairs)) return [7, [threes, pairs[0]]];
    if(flush) return [6, values];				    // Flush
    if(straight) return [5, [values[0]]];            // Straight
    if(low_straight) return [4, [3]];                // Ace Low Straight
    if(threes != null) return [3, [threes, singles[0], singles[1]]];
    if(size(pairs) == 2) return [2, [pairs[0], pairs[1], singles[0]]];
    if(size(pairs) == 1) return [1, [pairs[0], singles[0], singles[1], singles[2]]];
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

    return compare_rank(L, R);
}

function compare_rank(L, R) const public
{
    assert(is_array(L) && size(L) == 2, "invalid hand rank");
    assert(is_array(R) && size(R) == 2, "invalid hand rank");

    const res = compare(L[0], R[0]);
    if(res == "EQ") {
        return compare(L[1], R[1]);
    }
    return res;
}

// Selects a five-card hand from five board cards followed by two pocket cards.
// A pocket card that also appears on the board is unavailable to the player.

function select_hand(board_, pocket, hand) const public
{
    assert(is_array(board_) && size(board_) == 5, "invalid board");
    assert(is_array(pocket) && size(pocket) == 2, "invalid pocket");
    assert(is_array(hand) && size(hand) == 5, "invalid hand");

    const all_cards = concat(board_, pocket);
    const cards = [];
    const card_map = {};

    for(const i of hand) {
        assert(is_uint(i) && i < 7, "invalid card index");
        assert(card_map[i] == null, "duplicate card index");
        card_map[i] = true;

        if(i >= 5) {
            for(const board_card of board_) {
                assert(!equals(all_cards[i], board_card), "pocket card collides with board");
            }
        }
        push(cards, all_cards[i]);
    }
    return cards;
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

// Deals cards without replacement.

function deal_cards(seed_list) const public
{
    assert(is_array(seed_list), "seed list must be an array");

    var num_used = 0;
    const used_map = {};

    const cards = [];
    for(const seed of seed_list) {
        push(cards, draw_card(binary_hex(seed), used_map, num_used));
        num_used++;
    }
    return cards;
}

function draw_card(seed, used_map, num_used) const
{
    assert(is_binary(seed) && size(seed) == 32, "invalid seed");
    assert(num_used < 52, "deck is empty");

    const target = uint(seed) % (52 - num_used);

    var offset = 0;
    for(var index = 0; index < 52; index++) {
        if(used_map[index] == null) {
            if(offset == target) {
                used_map[index] = true;
                return get_card(index);
            }
            offset++;
        }
    }
    assert(false, "failed to draw card");
}

function compute() public
{
    if(board) {
        return board;
    }
    assert(state == 3, "wrong state");

    const seed_list = [binary(), binary(), binary(), binary()];
    for(const player of player_list) {
        for(var i = 0; i < size(player.seed) && i < 4; i++) {
            seed_list[i] = concat(seed_list[i], player.seed[i]);
        }
    }

    const source = [];
    for(var i = 0; i < 4; i++) {
        var hash = sha256(seed_list[i]);
        if(i > 0) {
            hash = sha256(concat(source[i - 1], hash));
        }
        push(source, hash);
    }
    global_seed = source[0];

    board = deal_cards([
        sha256(concat(binary_hex("F1"), source[1])),
        sha256(concat(binary_hex("F2"), source[1])),
        sha256(concat(binary_hex("F3"), source[1])),
        source[2],
        source[3]
    ]);
}
