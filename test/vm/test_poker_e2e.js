import {equals} from "std";

interface __test;
interface poker;

const MMX = string_bech32(bech32());
const poker_binary = __test.compile("src/contract/poker.js");

const poker_addr = poker.__deploy({
    __type: "mmx.contract.Executable",
    binary: poker_binary,
    init_args: [MMX, 10, 10, 3, 6]
});

function main()
{
    const alice = "mmx1kx69pm743rshqac5lgcstlr8nq4t93hzm8gumkkxmp5y9fglnkes6ve09z";
    const bob = "mmx1e7yktu9vpeyq7hx39cmagzfp2um3kddwjf4tlt8j3kmktwc7fk6qmyc6ns";
    const carol = "mmx1uj2dth7r9tcn3vas42f0hzz74dkz8ygv59mpx44n7px7j7yhvv4sfmkf0d";

    const alice_seed_0 = sha256("e2e_alice_seed_0");
    const alice_seed_1 = sha256("e2e_alice_seed_1");
    const alice_seed_2 = sha256("e2e_alice_seed_2");
    const alice_seed_3 = sha256("e2e_alice_seed_3");
    const alice_seed_4 = sha256("e2e_alice_seed_4");
    const alice_private_seed = sha256("e2e_alice_private_seed");

    const bob_seed_0 = sha256("e2e_bob_seed_0");
    const bob_seed_1 = sha256("e2e_bob_seed_1");
    const bob_seed_2 = sha256("e2e_bob_seed_2");
    const bob_seed_3 = sha256("e2e_bob_seed_3");
    const bob_seed_4 = sha256("e2e_bob_seed_4");
    const bob_private_seed = sha256("e2e_bob_private_seed");

    const carol_seed_0 = sha256("e2e_carol_seed_0");
    const carol_seed_1 = sha256("e2e_carol_seed_1");
    const carol_seed_2 = sha256("e2e_carol_seed_2");
    const carol_seed_3 = sha256("e2e_carol_seed_3");
    const carol_seed_4 = sha256("e2e_carol_seed_4");
    const carol_private_seed = sha256("e2e_carol_private_seed");

    poker.join("Alice", string_hex(sha256(alice_seed_0)), string_hex(sha256(alice_private_seed)), {
        __test: true, user: alice, deposit: [10, MMX]
    });
    poker.join("Bob", string_hex(sha256(bob_seed_0)), string_hex(sha256(bob_private_seed)), {
        __test: true, user: bob, deposit: [10, MMX]
    });
    poker.join("Carol", string_hex(sha256(carol_seed_0)), string_hex(sha256(carol_private_seed)), {
        __test: true, user: carol, deposit: [10, MMX]
    });

    assert(__test.get_balance(poker_addr, MMX) == 30);
    assert(poker.get_num_active() == 3);

    // Reaching max_players starts the reveal phase, where betting is rejected.
    poker.check(false, {__test: true, user: alice, assert_fail: true});

    // Round one reveals arrive in arbitrary player order.
    poker.reveal(string_hex(carol_seed_0), string_hex(sha256(carol_seed_1)), {__test: true, user: carol});
    poker.reveal(string_hex(alice_seed_0), string_hex(sha256(alice_seed_1)), {__test: true, user: alice});
    poker.reveal(string_hex(bob_seed_0), string_hex(sha256(bob_seed_1)), {__test: true, user: bob});

    // Carol checks before Alice raises. Bob calls immediately, while Carol
    // gets another parallel action sequence in which to call.
    poker.check(false, {__test: true, user: carol});
    poker.bet({__test: true, user: alice, deposit: [10, MMX]});
    poker.bet({__test: true, user: bob, deposit: [10, MMX]});

    // The reveal phase cannot start while an active player is below the raise.
    poker.reveal(string_hex(alice_seed_1), string_hex(sha256(alice_seed_2)), {
        __test: true, user: alice, assert_fail: true
    });

    poker.check(false, {__test: true, user: bob});
    poker.check(false, {__test: true, user: alice});
    poker.bet({__test: true, user: carol, deposit: [10, MMX]});

    const alice_after_call = poker.get_player_status(alice);
    const bob_after_call = poker.get_player_status(bob);
    const carol_after_call = poker.get_player_status(carol);
    assert(alice_after_call.bet == 20 && alice_after_call.revealed == 1);
    assert(bob_after_call.bet == 20 && bob_after_call.revealed == 1);
    assert(carol_after_call.bet == 20 && carol_after_call.revealed == 1);
    assert(!alice_after_call.folded && !bob_after_call.folded && !carol_after_call.folded);
    assert(poker.get_num_active() == 3);
    assert(__test.get_balance(poker_addr, MMX) == 60);

    // Round two: Carol raises, Alice calls, and Bob auto-folds after the raise.
    poker.reveal(string_hex(bob_seed_1), string_hex(sha256(bob_seed_2)), {__test: true, user: bob});
    poker.reveal(string_hex(carol_seed_1), string_hex(sha256(carol_seed_2)), {__test: true, user: carol});
    poker.reveal(string_hex(alice_seed_1), string_hex(sha256(alice_seed_2)), {__test: true, user: alice});

    poker.bet({__test: true, user: carol, deposit: [10, MMX]});
    poker.bet({__test: true, user: alice, deposit: [10, MMX]});
    poker.check(true, {__test: true, user: bob});

    const bob_after_fold = poker.get_player_status(bob);
    assert(bob_after_fold.bet == 20);
    assert(bob_after_fold.folded);
    assert(bob_after_fold.revealed == 2);
    assert(poker.get_num_active() == 2);
    assert(__test.get_balance(poker_addr, MMX) == 80);

    // Folded players no longer reveal or act.
    poker.reveal(string_hex(bob_seed_2), string_hex(sha256(bob_seed_3)), {
        __test: true, user: bob, assert_fail: true
    });

    // Round three.
    poker.reveal(string_hex(alice_seed_2), string_hex(sha256(alice_seed_3)), {__test: true, user: alice});
    poker.reveal(string_hex(carol_seed_2), string_hex(sha256(carol_seed_3)), {__test: true, user: carol});
    poker.check(false, {__test: true, user: carol});
    poker.check(false, {__test: true, user: alice});

    // Round four and final betting phase.
    poker.reveal(string_hex(carol_seed_3), string_hex(sha256(carol_seed_4)), {__test: true, user: carol});
    poker.reveal(string_hex(alice_seed_3), string_hex(sha256(alice_seed_4)), {__test: true, user: alice});
    poker.check(false, {__test: true, user: alice});
    poker.check(false, {__test: true, user: carol});

    const board = poker.compute();
    assert(size(board) == 5);
    for(var i = 0; i < size(board); i++) {
        for(var j = i + 1; j < size(board); j++) {
            assert(!equals(board[i], board[j]));
        }
    }

    // Both remaining players deliberately play the board, producing a tie.
    // A claim is rejected until every active player has shown.
    poker.show([0, 1, 2, 3, 4], string_hex(alice_private_seed), {__test: true, user: alice});
    poker.claim({__test: true, user: alice, assert_fail: true});
    poker.show([0, 1, 2, 3, 4], string_hex(carol_private_seed), {__test: true, user: carol});

    const alice_after_show = poker.get_player_status(alice);
    const carol_after_show = poker.get_player_status(carol);
    assert(alice_after_show.revealed == 4 && alice_after_show.shown);
    assert(carol_after_show.revealed == 4 && carol_after_show.shown);

    poker.claim({__test: true, user: bob, assert_fail: true});
    poker.claim({__test: true, user: alice});
    poker.claim({__test: true, user: alice, assert_fail: true});
    poker.claim({__test: true, user: carol});

    assert(__test.get_balance(alice, MMX) == 40);
    assert(__test.get_balance(bob, MMX) == 0);
    assert(__test.get_balance(carol, MMX) == 40);
    assert(__test.get_balance(poker_addr, MMX) == 0);
    assert(poker.get_player_status(alice).claimed);
    assert(!poker.get_player_status(bob).claimed);
    assert(poker.get_player_status(carol).claimed);
}

main();
