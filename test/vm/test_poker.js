import {equals} from "std";

interface __test;
interface poker;

const MMX = string_bech32(bech32());
const ZERO_SEED = "0000000000000000000000000000000000000000000000000000000000000000";
const poker_binary = __test.compile("src/contract/poker.js");

const poker_addr = poker.__deploy({
	__type: "mmx.contract.Executable",
	binary: poker_binary,
	init_args: [MMX, 1, 10, 2, 6]
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

assert(poker.compare_rank(
    poker.get_rank([["A", "H"], ["4", "D"], ["7", "C"], ["9", "S"], ["J", "H"]]),
    poker.get_rank([["2", "H"], ["4", "D"], ["7", "C"], ["9", "S"], ["J", "H"]])
) == "GT");

assert(poker.get_split_amount(10, 3, 0) == 4);
assert(poker.get_split_amount(10, 3, 1) == 3);
assert(poker.get_split_amount(10, 3, 2) == 3);
poker.get_split_amount(10, 0, 0, {__test: 1, assert_fail: true});
poker.get_split_amount(10, 3, 3, {__test: 1, assert_fail: true});

// Grouped ranks must be compared before kickers.
assert(poker.check_win(
    [["2", "H"], ["2", "D"], ["A", "C"], ["K", "S"], ["Q", "H"]],
    [["3", "H"], ["3", "D"], ["5", "C"], ["4", "S"], ["2", "C"]]
) == "LT");
assert(poker.check_win(
    [["K", "H"], ["K", "D"], ["2", "C"], ["2", "S"], ["A", "H"]],
    [["K", "C"], ["K", "S"], ["Q", "H"], ["Q", "D"], ["J", "H"]]
) == "LT");
assert(poker.check_win(
    [["2", "H"], ["2", "D"], ["2", "C"], ["A", "S"], ["K", "H"]],
    [["3", "H"], ["3", "D"], ["3", "C"], ["5", "S"], ["4", "H"]]
) == "LT");
assert(poker.check_win(
    [["2", "H"], ["2", "D"], ["2", "C"], ["A", "S"], ["A", "H"]],
    [["3", "H"], ["3", "D"], ["3", "C"], ["2", "S"], ["2", "H"]]
) == "LT");
assert(poker.check_win(
    [["2", "H"], ["2", "D"], ["2", "C"], ["2", "S"], ["A", "H"]],
    [["3", "H"], ["3", "D"], ["3", "C"], ["3", "S"], ["2", "H"]]
) == "LT");

{
    const deal = poker.deal_cards([ZERO_SEED, ZERO_SEED, ZERO_SEED, ZERO_SEED, ZERO_SEED]);
    assert(equals(deal, [
        ["2", "H"], ["3", "H"], ["4", "H"], ["5", "H"], ["6", "H"]
    ]));
}

{
    const board = [["2", "H"], ["3", "H"], ["4", "H"], ["5", "H"], ["6", "H"]];
    const pocket = [["2", "H"], ["2", "D"]];

    assert(equals(
        poker.select_hand(board, pocket, [0, 1, 2, 3, 4]),
        board
    ));
    assert(equals(
        poker.select_hand(board, pocket, [0, 1, 2, 3, 6]),
        [["2", "H"], ["3", "H"], ["4", "H"], ["5", "H"], ["2", "D"]]
    ));

    poker.select_hand(board, pocket, [0, 1, 2, 3, 5], {__test: 1, assert_fail: true});
    poker.select_hand(board, pocket, [0, 1, 2, 3, 3], {__test: 1, assert_fail: true});
}

{
    const alice = "mmx1kx69pm743rshqac5lgcstlr8nq4t93hzm8gumkkxmp5y9fglnkes6ve09z";
    const bob = "mmx1e7yktu9vpeyq7hx39cmagzfp2um3kddwjf4tlt8j3kmktwc7fk6qmyc6ns";
    const alice_seed = sha256("alice_seed");
    const alice_seed_1 = sha256("alice_seed_1");
    const alice_seed_2 = sha256("alice_seed_2");
    const alice_seed_3 = sha256("alice_seed_3");
    const alice_seed_4 = sha256("alice_seed_4");
    const alice_private_seed = sha256("alice_private_seed");
    const bob_seed = sha256("bob_seed");
    const bob_seed_1 = sha256("bob_seed_1");
    const bob_seed_2 = sha256("bob_seed_2");
    const bob_seed_3 = sha256("bob_seed_3");
    const bob_seed_4 = sha256("bob_seed_4");
    const bob_private_seed = sha256("bob_private_seed");

    poker.join("Alice", string_hex(sha256(alice_seed)), string_hex(sha256(alice_private_seed)), {
        __test: 1, user: alice, deposit: [1, MMX]
    });
    poker.join("Alice Again", string_hex(sha256(alice_seed)), string_hex(sha256(alice_private_seed)), {
        __test: 1, user: alice, deposit: [1, MMX], assert_fail: true
    });
    poker.join("Bob", string_hex(sha256(bob_seed)), string_hex(sha256(bob_private_seed)), {
        __test: 1, user: bob, deposit: [1, MMX]
    });

    poker.reveal([0], string_hex(sha256(alice_seed_1)), {
        __test: 1, user: alice, assert_fail: true
    });
    poker.reveal(string_hex(alice_seed), string_hex(sha256(alice_seed_1)), {__test: 1, user: alice});
    poker.reveal(string_hex(bob_seed), string_hex(sha256(bob_seed_1)), {__test: 1, user: bob});

    poker.check(false, {__test: 1, user: alice});
    poker.check(false, {__test: 1, user: bob});
    poker.reveal(string_hex(alice_seed_1), string_hex(sha256(alice_seed_2)), {__test: 1, user: alice});
    poker.reveal(string_hex(bob_seed_1), string_hex(sha256(bob_seed_2)), {__test: 1, user: bob});

    poker.check(false, {__test: 1, user: alice});
    poker.check(false, {__test: 1, user: bob});
    poker.reveal(string_hex(alice_seed_2), string_hex(sha256(alice_seed_3)), {__test: 1, user: alice});
    poker.reveal(string_hex(bob_seed_2), string_hex(sha256(bob_seed_3)), {__test: 1, user: bob});

    poker.check(false, {__test: 1, user: alice});
    poker.check(false, {__test: 1, user: bob});
    poker.reveal(string_hex(alice_seed_3), string_hex(sha256(alice_seed_4)), {__test: 1, user: alice});
    poker.reveal(string_hex(bob_seed_3), string_hex(sha256(bob_seed_4)), {__test: 1, user: bob});

    poker.check(false, {__test: 1, user: alice});
    poker.check(false, {__test: 1, user: bob});

    poker.show([0, 1, 2, 3, 4], string_hex(alice_private_seed), {__test: 1, user: alice});
    poker.show([0, 1, 2, 3, 4], string_hex(bob_private_seed), {__test: 1, user: bob});

    poker.claim({__test: 1, user: alice});
    poker.claim({__test: 1, user: bob});

    assert(__test.get_balance(alice, MMX) == 1);
    assert(__test.get_balance(bob, MMX) == 1);
}

} // main

main();
