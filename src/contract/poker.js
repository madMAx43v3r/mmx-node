
const RANK_MAP = {
    "2": 0, "3": 1, "4": 2, "5": 3, "6": 4, "7": 5, "8": 6, "9": 7, "10": 8,
    "T": 8, "J": 9, "Q": 10, "K": 11, "A": 12
};

const LOW_STRAIGHT = [12, 3, 2, 1, 0]; // A, 5, 4, 3, 2

/*
 * Poker Hand Rankings:
 * 0 - High Card
 * 1 - One Pair
 * 2 - Two Pair
 * 3 - Three of a Kind
 * 4 - Straight
 * 5 - Flush
 * 6 - Full House
 * 7 - Four of a Kind
 * 8 - Straight Flush
 */

/*
 * Function to get the rank of a poker hand (5 cards)
 * @param hand - array of array representing the poker hand
 * @return - array with the rank and sorted values of the hand
 * 
 * Example:
 *   const hand = [["2", "H"], ["3", "D"], ["4", "C"], ["5", "S"], ["6", "H"]];
 *   const rank = get_rank(hand);
 *   Output: [4, [6, 5, 4, 3, 2]]
 */
function get_rank(hand) public
{
    assert(size(hand) == 5, "Hand must contain exactly 5 cards");

    var values = [];
    var suit_map = {"H": 0, "D": 0, "C": 0, "S": 0};
    
    for(const card of hand) {
        push(values, RANK_MAP[card[0]]);
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
    
    var straight = (size(unique_values) == 5 && (unique_values[0] - unique_values[4] == 4))
        || equals(unique_values, LOW_STRAIGHT);

    var flush = false;
    for(const suit of ["H", "D", "C", "S"]) {
        if(suit_map[suit] == 5) {
            flush = true;
        }
    }

    var count = {0: 0, 1: 0, 2: 0, 3: 0, 4: 0, 5: 0, 6: 0, 7: 0, 8: 0, 9: 0, 10: 0, 11: 0, 12: 0};
    for(const v of values) {
        count[v]++;
    }
    
    var pairs = 0;
    var threes = 0;
    var fours = 0;
    for(const v of values) {
        if (count[v] == 2) pairs++;
        if (count[v] == 3) threes++;
        if (count[v] == 4) fours++;
    }
    
    if (flush && straight) return [8, values]; // Straight flush
    if (fours) return [7, values]; // Four of a kind
    if (threes && pairs) return [6, values]; // Full house
    if (flush) return [5, values]; // Flush
    if (straight) return [4, values]; // Straight
    if (threes) return [3, values]; // Three of a kind
    if (pairs === 2) return [2, values]; // Two pair
    if (pairs === 1) return [1, values]; // One pair
    return [0, values]; // High card
}
