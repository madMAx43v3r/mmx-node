import {equals, sort, reverse, compare} from "std";

const RANK_MAP = {
    "2": 0, "3": 1, "4": 2, "5": 3, "6": 4, "7": 5, "8": 6, "9": 7, "10": 8,
    "T": 8, "J": 9, "Q": 10, "K": 11, "A": 12
};

const ACE_LOW_STRAIGHT = [12, 3, 2, 1, 0]; // A, 5, 4, 3, 2

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
// 9 - Straight Flush

// Poker Suits:
// H - Hearts
// D - Diamonds
// C - Clubs
// S - Spades

function init() {
    // TODO
}

// Function to get the rank of a poker hand (5 cards)
// @param hand - array of array representing the poker hand
// @return - array with the rank and sorted values of the hand
//
// Example:
//   hand = [["2", "H"], ["3", "D"], ["4", "C"], ["5", "S"], ["6", "H"]]
//   return => [5, [4, 3, 2, 1, 0]]

function get_rank(hand) public
{
    assert(size(hand) == 5, "hand must be 5 cards");

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
    const low_straight = equals(unique_values, ACE_LOW_STRAIGHT);
    if(low_straight) {
        straight = true;
    }

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
    
    if(flush && straight) return [9, values];	// Straight Flush
    if(fours) return [8, values];				// Four of a kind
    if(threes && pairs) return [7, values];		// Full House
    if(flush) return [6, values];				// Flush
    if(low_straight) return [4, values];	    // Ace Low Straight
    if(straight) return [5, values];			// Straight
    if(threes) return [3, values];				// Three of a kind
    if(pairs == 2) return [2, values];			// Two Pair
    if(pairs == 1) return [1, values];			// One Pair
    return [0, values];							// High Card
}

// Function to compare two poker hands
// @param hand - first poker hand (array of array)
// @param other - second poker hand (array of array)
// @return - "GT" if hand wins, "LT" if other wins, "EQ" if tie

function check_win(hand, other) public
{
    assert(size(hand) == 5, "hand must be 5 cards");
    assert(size(other) == 5, "hand must be 5 cards");

    const L = get_rank(hand);
    const R = get_rank(other);

    const res = compare(L[0], R[0]);
    if(res == "EQ") {
        return compare(L[1], R[1]);
    }
    return res;
}
