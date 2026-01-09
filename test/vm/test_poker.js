import {equals} from "std";

interface __test;
interface poker;

const poker_binary = __test.compile("src/contract/poker.js");

const poker_addr = poker.__deploy({
	__type: "mmx.contract.Executable",
	binary: poker_binary,
	init_args: []
});

function main() {

{
    const rank = poker.get_rank([["2", "H"], ["4", "D"], ["7", "C"], ["9", "S"], ["J", "H"]]);
    assert(rank[0] == 0); // High Card
}
{
    const rank = poker.get_rank([["4", "H"], ["4", "D"], ["7", "C"], ["9", "S"], ["J", "H"]]);
    assert(rank[0] == 1); // One Pair
}
{
    const rank = poker.get_rank([["4", "H"], ["4", "D"], ["7", "C"], ["7", "S"], ["J", "H"]]);
    assert(rank[0] == 2); // Two Pair
}
{
    const rank = poker.get_rank([["6", "H"], ["6", "D"], ["6", "C"], ["9", "S"], ["J", "H"]]);
    assert(rank[0] == 3); // Three of a Kind
}
{
    const rank = poker.get_rank([["A", "H"], ["2", "H"], ["3", "D"], ["4", "C"], ["5", "S"]]);
    assert(rank[0] == 4); // Ace Low Straight
}
{
    const rank = poker.get_rank([["2", "H"], ["3", "D"], ["4", "C"], ["5", "S"], ["6", "H"]]);
    assert(rank[0] == 5); // Straight
}
{
    const rank = poker.get_rank([["2", "H"], ["5", "H"], ["8", "H"], ["J", "H"], ["K", "H"]]);
    assert(rank[0] == 6); // Flush
}
{
    const rank = poker.get_rank([["3", "D"], ["3", "H"], ["3", "S"], ["5", "C"], ["5", "H"]]);
    assert(rank[0] == 7); // Full House
}
{
    const rank = poker.get_rank([["9", "H"], ["9", "D"], ["9", "C"], ["9", "S"], ["K", "H"]]);
    assert(rank[0] == 8); // Four of a Kind
}
{
    const rank = poker.get_rank([["A", "H"], ["2", "H"], ["3", "H"], ["4", "H"], ["5", "H"]]);
    assert(rank[0] == 9); // Ace Low Straight Flush
}
{
    const rank = poker.get_rank([["5", "S"], ["6", "S"], ["7", "S"], ["8", "S"], ["9", "S"]]);
    assert(rank[0] == 10); // Straight Flush
}
{
    const rank = poker.get_rank([["10", "S"], ["J", "S"], ["Q", "S"], ["K", "S"], ["A", "S"]]);
    assert(rank[0] == 10); // Royal Flush
}

assert(poker.check_win(
    [["2", "H"], ["4", "D"], ["7", "C"], ["9", "S"], ["J", "H"]],   // High Card
    [["2", "H"], ["4", "D"], ["7", "C"], ["9", "S"], ["J", "H"]]    // High Card
) == "EQ");

assert(poker.check_win(
    [["A", "H"], ["4", "D"], ["7", "C"], ["9", "S"], ["J", "H"]],   // High Card
    [["2", "H"], ["4", "D"], ["7", "C"], ["9", "S"], ["J", "H"]]    // High Card
) == "GT");

assert(poker.check_win(
    [["2", "H"], ["4", "D"], ["7", "C"], ["9", "S"], ["J", "H"]],   // High Card
    [["A", "H"], ["4", "D"], ["7", "C"], ["9", "S"], ["J", "H"]]    // High Card
) == "LT");

assert(poker.check_win(
    [["2", "H"], ["3", "D"], ["4", "C"], ["5", "S"], ["6", "H"]],   // Straight
    [["2", "H"], ["3", "D"], ["4", "C"], ["5", "S"], ["6", "H"]]    // Straight
) == "EQ");

assert(poker.check_win(
    [["2", "H"], ["5", "H"], ["8", "H"], ["J", "H"], ["K", "H"]],   // Flush
    [["2", "H"], ["5", "H"], ["8", "H"], ["J", "H"], ["K", "H"]]    // Flush
) == "EQ");

assert(poker.check_win(
    [["A", "H"], ["5", "H"], ["8", "H"], ["J", "H"], ["K", "H"]],   // Flush
    [["2", "H"], ["5", "H"], ["8", "H"], ["J", "H"], ["K", "H"]]    // Flush
) == "GT");

assert(poker.check_win(
    [["2", "H"], ["5", "H"], ["8", "H"], ["J", "H"], ["K", "H"]],   // Flush
    [["A", "H"], ["5", "H"], ["8", "H"], ["J", "H"], ["K", "H"]]    // Flush
) == "LT");

assert(poker.check_win(
    [["A", "H"], ["2", "H"], ["3", "D"], ["4", "C"], ["5", "S"]],   // Ace Low Straight
    [["2", "H"], ["3", "D"], ["4", "C"], ["5", "S"], ["6", "H"]]    // Straight
) == "LT");

assert(poker.check_win(
    [["A", "H"], ["2", "H"], ["3", "H"], ["4", "H"], ["5", "H"]],   // Ace Low Straight Flush
    [["2", "H"], ["3", "H"], ["4", "H"], ["5", "H"], ["6", "H"]]    // Straight Flush
) == "LT");

} // main

main();
