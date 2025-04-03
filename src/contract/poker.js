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
var timeout;

var num_players = 0;
var num_reveals = 0;
var deadline = null;
var global_seed = null;
var round = 0;
var pot_size = 0;
var bet_amount = 0;

var player_map = {};
var player_list = [];

var board = [];


function init(currency_, blind_bet_, bet_limit_, max_players_, timeout_)
{
    currency = bech32(currency_);
    blind_bet = uint(blind_bet_);
    bet_limit = uint(bet_limit_) * blind_bet;
    max_players = uint(max_players_);
    timeout = uint(timeout_);
    assert(max_players >= 2);
    assert(timeout >= 6);
}

function join(name, commit, private_commit) public payable
{
    commit = binary_hex(commit);
    private_commit = binary_hex(private_commit);

    assert(round == 0, "game already started");
    assert(num_players < max_players, "table full");
    assert(this.user, "missing user");
    assert(this.deposit.currency == currency, "invalid currency");
    assert(this.deposit.amount == blind_bet, "invalid deposit amount");
    assert(is_string(name) && size(name) > 1 && size(name) <= 24, "invalid name");
    assert(size(commit) == 32, "invalid commit");
    assert(size(private_commit) == 32, "invalid private commit");
    assert(!player_map[this.user], "already joined");

    if(!num_players) {
        deadline = this.height + 3 * timeout;
    }
    if(deadline) {
        assert(this.height < deadline, "too late");
    }

    player_map[this.user] = {
        name: name,
        bet: blind_bet,
        seed: null,
        commit: commit,
        private_seed: null,
        private_commit: private_commit,
    };
    push(player_list, this.user);

    pot_size += blind_bet;
    num_players++;

    if(num_players == max_players) {
        round++;
        deadline = this.height + timeout;
    }
}

function reveal(seed) public
{
    assert(size(seed) == 32, "invalid seed length");

    const player = player_map[this.user];
    assert(player, "not a player");
    assert(player.seed == null, "already revealed");
    assert(sha256(seed) == player.commit, "invalid seed");

    player.seed = seed;
    player.commit = null;
    num_reveals++;

    if(num_reveals == num_players) {
        num_reveals = 0;
        global_seed = sha256(concat(player.seed, player.private_commit));
        round++;
        deadline = this.height + timeout;
    }
}

// Function to leave the game if nobody else joined after the deadline

function leave() public
{
    assert(round == 0, "game already started");
    assert(num_players == 1, "cannot leave table");
    assert(this.user, "missing user");
    assert(this.height >= deadline, "too early");

    const player = player_map[this.user];
    assert(player, "not a player");

    pot_size -= player.bet;
    send(this.user, player.bet, currency);

    player_map[this.user] = null;
    num_players--;
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
