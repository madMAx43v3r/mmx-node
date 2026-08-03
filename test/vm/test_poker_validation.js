import {equals} from "std";

interface __test;
interface poker_bad_players;
interface poker_bad_timeout;
interface poker_validation;
interface poker_concurrent_bets;

const MMX = string_bech32(bech32());
const USD = string_bech32(sha256("poker_validation_USD"));
const ZERO_SEED = "0000000000000000000000000000000000000000000000000000000000000000";
const ONE_SEED = "0000000000000000000000000000000000000000000000000000000000000001";
const poker_binary = __test.compile("src/contract/poker.js");

poker_bad_players.__deploy({
    __type: "mmx.contract.Executable",
    binary: poker_binary,
    init_args: [MMX, 10, 2, 1, 6, {__test: true, assert_fail: true}]
});
poker_bad_timeout.__deploy({
    __type: "mmx.contract.Executable",
    binary: poker_binary,
    init_args: [MMX, 10, 2, 2, 5, {__test: true, assert_fail: true}]
});

const poker_addr = poker_validation.__deploy({
    __type: "mmx.contract.Executable",
    binary: poker_binary,
    init_args: [MMX, 10, 2, 3, 6]
});
const concurrent_bets_addr = poker_concurrent_bets.__deploy({
    __type: "mmx.contract.Executable",
    binary: poker_binary,
    init_args: [MMX, 10, 10, 3, 6]
});

function main()
{
    const alice = string_bech32(sha256("validation_alice"));
    const bob = string_bech32(sha256("validation_bob"));
    const carol = string_bech32(sha256("validation_carol"));
    const dave = string_bech32(sha256("validation_dave"));

    const alice_seed_0 = sha256("validation_alice_seed_0");
    const alice_seed_1 = sha256("validation_alice_seed_1");
    const bob_seed_0 = sha256("validation_bob_seed_0");
    const bob_seed_1 = sha256("validation_bob_seed_1");
    const carol_seed_0 = sha256("validation_carol_seed_0");
    const carol_seed_1 = sha256("validation_carol_seed_1");

    const alice_commit = string_hex(sha256(alice_seed_0));
    const alice_private_commit = string_hex(sha256("validation_alice_private"));

    poker_validation.join("Alice", alice_commit, alice_private_commit, {
        __test: true, user: alice, deposit: [10, USD], assert_fail: true
    });
    poker_validation.join("Alice", alice_commit, alice_private_commit, {
        __test: true, user: alice, deposit: [9, MMX], assert_fail: true
    });
    poker_validation.join("Alice", "00", alice_private_commit, {
        __test: true, user: alice, deposit: [10, MMX], assert_fail: true
    });
    poker_validation.join("Alice", alice_commit, "00", {
        __test: true, user: alice, deposit: [10, MMX], assert_fail: true
    });
    poker_validation.join("", alice_commit, alice_private_commit, {
        __test: true, user: alice, deposit: [10, MMX], assert_fail: true
    });
    poker_validation.join("1234567890123456789012345", alice_commit, alice_private_commit, {
        __test: true, user: alice, deposit: [10, MMX], assert_fail: true
    });

    poker_validation.join("Alice", alice_commit, alice_private_commit, {
        __test: true, user: alice, deposit: [10, MMX]
    });
    poker_validation.join("Alice Again", alice_commit, alice_private_commit, {
        __test: true, user: alice, deposit: [10, MMX], assert_fail: true
    });
    poker_validation.join("Bob", string_hex(sha256(bob_seed_0)), string_hex(sha256("validation_bob_private")), {
        __test: true, user: bob, deposit: [10, MMX]
    });
    poker_validation.join("Carol", string_hex(sha256(carol_seed_0)), string_hex(sha256("validation_carol_private")), {
        __test: true, user: carol, deposit: [10, MMX]
    });
    poker_validation.join("Dave", string_hex(sha256("validation_dave_seed")), string_hex(sha256("validation_dave_private")), {
        __test: true, user: dave, deposit: [10, MMX], assert_fail: true
    });
    assert(__test.get_balance(poker_addr, MMX) == 30);

    poker_validation.get_player_status(dave, {__test: true, assert_fail: true});

    poker_validation.reveal(string_hex(sha256("wrong_seed")), string_hex(sha256(alice_seed_1)), {
        __test: true, user: alice, assert_fail: true
    });
    poker_validation.reveal(string_hex(alice_seed_0), "00", {
        __test: true, user: alice, assert_fail: true
    });
    poker_validation.reveal(string_hex(alice_seed_0), string_hex(sha256(alice_seed_1)), {__test: true, user: alice});
    poker_validation.reveal(string_hex(alice_seed_0), string_hex(sha256(alice_seed_1)), {
        __test: true, user: alice, assert_fail: true
    });
    poker_validation.reveal(string_hex(bob_seed_0), string_hex(sha256(bob_seed_1)), {__test: true, user: bob});
    poker_validation.reveal(string_hex(carol_seed_0), string_hex(sha256(carol_seed_1)), {__test: true, user: carol});

    poker_validation.bet({__test: true, user: alice, deposit: [10, USD], assert_fail: true});
    poker_validation.bet({__test: true, user: alice, deposit: [9, MMX], assert_fail: true});
    poker_validation.bet({__test: true, user: alice, deposit: [11, MMX], assert_fail: true});
    poker_validation.bet({__test: true, user: alice, deposit: [10, MMX]});
    poker_validation.check(false, {__test: true, user: alice, assert_fail: true});
    poker_validation.bet({__test: true, user: bob, deposit: [10, MMX]});
    poker_validation.bet({__test: true, user: carol, deposit: [10, MMX]});

    assert(poker_validation.get_player_status(alice).bet == 20);
    assert(poker_validation.get_player_status(bob).bet == 20);
    assert(poker_validation.get_player_status(carol).bet == 20);
    assert(__test.get_balance(poker_addr, MMX) == 60);

    poker_validation.reveal(string_hex(sha256("wrong_second_seed")), string_hex(sha256("next")), {
        __test: true, user: bob, assert_fail: true
    });

    assert(equals(poker_validation.deal_cards([ONE_SEED]), [["3", "H"]]));
    poker_validation.deal_cards(null, {__test: true, assert_fail: true});
    poker_validation.deal_cards(["00"], {__test: true, assert_fail: true});

    const too_many_seeds = [];
    for(var i = 0; i < 53; i++) {
        push(too_many_seeds, ZERO_SEED);
    }
    poker_validation.deal_cards(too_many_seeds, {__test: true, assert_fail: true});

    // Concurrent bets can increase the target in any arrival order. All active
    // players then receive another parallel sequence in which to match it.
    {
        const raise_alice = string_bech32(sha256("reraise_alice"));
        const raise_bob = string_bech32(sha256("reraise_bob"));
        const raise_carol = string_bech32(sha256("reraise_carol"));
        const raise_alice_seed = sha256("reraise_alice_seed");
        const raise_bob_seed = sha256("reraise_bob_seed");
        const raise_carol_seed = sha256("reraise_carol_seed");

        poker_concurrent_bets.join("Alice", string_hex(sha256(raise_alice_seed)), string_hex(sha256("reraise_alice_private")), {
            __test: true, user: raise_alice, deposit: [10, MMX]
        });
        poker_concurrent_bets.join("Bob", string_hex(sha256(raise_bob_seed)), string_hex(sha256("reraise_bob_private")), {
            __test: true, user: raise_bob, deposit: [10, MMX]
        });
        poker_concurrent_bets.join("Carol", string_hex(sha256(raise_carol_seed)), string_hex(sha256("reraise_carol_private")), {
            __test: true, user: raise_carol, deposit: [10, MMX]
        });
        poker_concurrent_bets.reveal(string_hex(raise_carol_seed), string_hex(sha256("reraise_carol_seed_1")), {__test: true, user: raise_carol});
        poker_concurrent_bets.reveal(string_hex(raise_alice_seed), string_hex(sha256("reraise_alice_seed_1")), {__test: true, user: raise_alice});
        poker_concurrent_bets.reveal(string_hex(raise_bob_seed), string_hex(sha256("reraise_bob_seed_1")), {__test: true, user: raise_bob});

        poker_concurrent_bets.check(false, {__test: true, user: raise_carol});
        poker_concurrent_bets.bet({__test: true, user: raise_alice, deposit: [10, MMX]});
        poker_concurrent_bets.bet({__test: true, user: raise_bob, deposit: [20, MMX]});

        poker_concurrent_bets.check(false, {__test: true, user: raise_bob});
        poker_concurrent_bets.bet({__test: true, user: raise_carol, deposit: [20, MMX]});
        poker_concurrent_bets.bet({__test: true, user: raise_alice, deposit: [15, MMX], assert_fail: true});
        poker_concurrent_bets.bet({__test: true, user: raise_alice, deposit: [20, MMX]});

        poker_concurrent_bets.check(false, {__test: true, user: raise_alice});
        poker_concurrent_bets.bet({__test: true, user: raise_bob, deposit: [10, MMX]});
        poker_concurrent_bets.bet({__test: true, user: raise_carol, deposit: [10, MMX]});

        assert(poker_concurrent_bets.get_player_status(raise_alice).bet == 40);
        assert(poker_concurrent_bets.get_player_status(raise_bob).bet == 40);
        assert(poker_concurrent_bets.get_player_status(raise_carol).bet == 40);
        assert(__test.get_balance(concurrent_bets_addr, MMX) == 120);
    }
}

main();
