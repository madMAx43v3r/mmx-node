import {equals} from "std";

interface __test;
interface poker_showdown;

const MMX = string_bech32(bech32());
const poker_binary = __test.compile("src/contract/poker.js");

const poker_addr = poker_showdown.__deploy({
    __type: "mmx.contract.Executable",
    binary: poker_binary,
    init_args: [MMX, 1, 10, 2, 6]
});

function get_pocket(global_seed, private_seed)
{
    const source = sha256(concat(global_seed, private_seed));
    return poker_showdown.deal_cards([
        string_hex(sha256(concat(binary_hex("A1"), source))),
        string_hex(sha256(concat(binary_hex("A2"), source)))
    ]);
}

function collides(board, card)
{
    for(const board_card of board) {
        if(equals(board_card, card)) {
            return true;
        }
    }
    return false;
}

function make_pocket_hand(board, pocket, omit, pocket_index)
{
    const indices = [];
    const cards = [];
    for(var i = 0; i < 5; i++) {
        if(i != omit) {
            push(indices, i);
            push(cards, board[i]);
        }
    }
    push(indices, 5 + pocket_index);
    push(cards, pocket[pocket_index]);
    return [indices, cards];
}

function main()
{
    const alice = string_bech32(sha256("showdown_alice"));
    const bob = string_bech32(sha256("showdown_bob"));
    const alice_seed = [sha256("showdown_alice_0"), sha256("showdown_alice_1"), sha256("showdown_alice_2"), sha256("showdown_alice_3"), sha256("showdown_alice_4")];
    const bob_seed = [sha256("showdown_bob_0"), sha256("showdown_bob_1"), sha256("showdown_bob_2"), sha256("showdown_bob_3"), sha256("showdown_bob_4")];

    const source = [];
    for(var i = 0; i < 4; i++) {
        var hash = sha256(concat(alice_seed[i], bob_seed[i]));
        if(i > 0) {
            hash = sha256(concat(source[i - 1], hash));
        }
        push(source, hash);
    }
    const global_seed = source[0];
    const expected_board = poker_showdown.deal_cards([
        string_hex(sha256(concat(binary_hex("F1"), source[1]))),
        string_hex(sha256(concat(binary_hex("F2"), source[1]))),
        string_hex(sha256(concat(binary_hex("F3"), source[1]))),
        string_hex(source[2]),
        string_hex(source[3])
    ]);
    const board_rank = poker_showdown.get_rank(expected_board);

    // Find a deterministic Alice pocket hand that outranks playing the board.
    var alice_private_seed = null;
    var alice_hand = null;
    var nonce = 0;
    while(alice_private_seed == null && nonce < 256) {
        const candidate = sha256(concat("showdown_alice_private_", string(nonce)));
        const pocket = get_pocket(global_seed, candidate);
        for(var pocket_index = 0; pocket_index < 2; pocket_index++) {
            if(!collides(expected_board, pocket[pocket_index])) {
                for(var omit = 0; omit < 5; omit++) {
                    const choice = make_pocket_hand(expected_board, pocket, omit, pocket_index);
                    const rank = poker_showdown.get_rank(choice[1]);
                    if(alice_private_seed == null && poker_showdown.compare_rank(rank, board_rank) == "GT") {
                        alice_private_seed = candidate;
                        alice_hand = choice[0];
                    }
                }
            }
        }
        nonce++;
    }
    assert(alice_private_seed != null, "failed to find winning pocket");

    // Find a deterministic Bob pocket with a board collision.
    var bob_private_seed = null;
    var bob_collision_index = null;
    nonce = 0;
    while(bob_private_seed == null && nonce < 256) {
        const candidate = sha256(concat("showdown_bob_private_", string(nonce)));
        const pocket = get_pocket(global_seed, candidate);
        for(var pocket_index = 0; pocket_index < 2; pocket_index++) {
            if(bob_private_seed == null && collides(expected_board, pocket[pocket_index])) {
                bob_private_seed = candidate;
                bob_collision_index = 5 + pocket_index;
            }
        }
        nonce++;
    }
    assert(bob_private_seed != null, "failed to find colliding pocket");

    poker_showdown.join("Alice", string_hex(sha256(alice_seed[0])), string_hex(sha256(alice_private_seed)), {
        __test: true, user: alice, deposit: [1, MMX]
    });
    poker_showdown.join("Bob", string_hex(sha256(bob_seed[0])), string_hex(sha256(bob_private_seed)), {
        __test: true, user: bob, deposit: [1, MMX]
    });

    for(var round = 0; round < 4; round++) {
        poker_showdown.reveal(string_hex(bob_seed[round]), string_hex(sha256(bob_seed[round + 1])), {__test: true, user: bob});
        poker_showdown.reveal(string_hex(alice_seed[round]), string_hex(sha256(alice_seed[round + 1])), {__test: true, user: alice});
        poker_showdown.check(false, {__test: true, user: alice});
        poker_showdown.check(false, {__test: true, user: bob});
    }

    assert(equals(poker_showdown.compute(), expected_board));

    poker_showdown.show([0, 1, 2, 3], string_hex(bob_private_seed), {
        __test: true, user: bob, assert_fail: true
    });
    poker_showdown.show([0, 1, 2, 3, bob_collision_index], string_hex(bob_private_seed), {
        __test: true, user: bob, assert_fail: true
    });
    poker_showdown.show([0, 1, 2, 3, 4], string_hex(bob_private_seed), {__test: true, user: bob});
    poker_showdown.show([0, 1, 2, 3, 4], string_hex(bob_private_seed), {
        __test: true, user: bob, assert_fail: true
    });

    poker_showdown.show(alice_hand, string_hex(sha256("wrong_private_seed")), {
        __test: true, user: alice, assert_fail: true
    });
    poker_showdown.show(alice_hand, string_hex(alice_private_seed), {__test: true, user: alice});

    poker_showdown.claim({__test: true, user: bob, assert_fail: true});
    poker_showdown.claim({__test: true, user: alice});

    assert(__test.get_balance(alice, MMX) == 2);
    assert(__test.get_balance(bob, MMX) == 0);
    assert(__test.get_balance(poker_addr, MMX) == 0);
    assert(poker_showdown.get_player_status(alice).claimed);
    assert(!poker_showdown.get_player_status(bob).claimed);
}

main();
