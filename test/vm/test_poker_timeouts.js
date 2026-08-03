interface __test;
interface poker_leave;
interface poker_timeout;
interface poker_bet_timeout;
interface poker_no_show;
interface poker_all_fold;

const MMX = string_bech32(bech32());
const poker_binary = __test.compile("src/contract/poker.js");

const leave_addr = poker_leave.__deploy({
    __type: "mmx.contract.Executable",
    binary: poker_binary,
    init_args: [MMX, 10, 10, 3, 6]
});
const timeout_addr = poker_timeout.__deploy({
    __type: "mmx.contract.Executable",
    binary: poker_binary,
    init_args: [MMX, 10, 10, 3, 6]
});
const bet_timeout_addr = poker_bet_timeout.__deploy({
    __type: "mmx.contract.Executable",
    binary: poker_binary,
    init_args: [MMX, 10, 10, 2, 6]
});
const no_show_addr = poker_no_show.__deploy({
    __type: "mmx.contract.Executable",
    binary: poker_binary,
    init_args: [MMX, 1, 10, 3, 6]
});
const all_fold_addr = poker_all_fold.__deploy({
    __type: "mmx.contract.Executable",
    binary: poker_binary,
    init_args: [MMX, 5, 10, 2, 6]
});

function main()
{
    // A lone player can wait indefinitely or leave voluntarily. The empty
    // table can then be used again without inheriting a stale deadline.
    {
        __test.set_height(0);
        const alice = string_bech32(sha256("leave_alice"));
        const bob = string_bech32(sha256("leave_bob"));
        const alice_seed = sha256("leave_alice_seed");
        const bob_seed = sha256("leave_bob_seed");

        poker_leave.join("Alice", string_hex(sha256(alice_seed)), string_hex(sha256("leave_alice_private")), {
            __test: true, user: alice, deposit: [10, MMX]
        });
        __test.inc_height(1000);
        assert(!poker_leave.is_timeout());
        poker_leave.leave({__test: true, user: alice});
        assert(__test.get_balance(alice, MMX) == 10);
        assert(__test.get_balance(leave_addr, MMX) == 0);

        poker_leave.join("Bob", string_hex(sha256(bob_seed)), string_hex(sha256("leave_bob_private")), {
            __test: true, user: bob, deposit: [10, MMX]
        });
        assert(!poker_leave.is_timeout());
        poker_leave.leave({__test: true, user: bob});
        assert(__test.get_balance(bob, MMX) == 10);
        assert(__test.get_balance(leave_addr, MMX) == 0);
    }

    // A two-player non-full table starts when its lobby deadline expires. A
    // player missing the first reveal becomes inactive, and the survivor can
    // finish the game alone. If nobody shows, the survivor receives the pot.
    {
        __test.set_height(0);
        const alice = string_bech32(sha256("timeout_alice"));
        const bob = string_bech32(sha256("timeout_bob"));
        const alice_seed_0 = sha256("timeout_alice_seed_0");
        const alice_seed_1 = sha256("timeout_alice_seed_1");
        const alice_seed_2 = sha256("timeout_alice_seed_2");
        const alice_seed_3 = sha256("timeout_alice_seed_3");
        const alice_seed_4 = sha256("timeout_alice_seed_4");
        const bob_seed_0 = sha256("timeout_bob_seed_0");

        poker_timeout.join("Alice", string_hex(sha256(alice_seed_0)), string_hex(sha256("timeout_alice_private")), {
            __test: true, user: alice, deposit: [10, MMX]
        });
        __test.inc_height(10);
        poker_timeout.join("Bob", string_hex(sha256(bob_seed_0)), string_hex(sha256("timeout_bob_private")), {
            __test: true, user: bob, deposit: [10, MMX]
        });

        __test.inc_height(17);
        assert(!poker_timeout.is_timeout());
        __test.inc_height(1);
        assert(poker_timeout.is_timeout());
        poker_timeout.update();
        assert(!poker_timeout.is_timeout());

        poker_timeout.reveal(string_hex(alice_seed_0), string_hex(sha256(alice_seed_1)), {__test: true, user: alice});
        __test.inc_height(6);
        poker_timeout.update();
        assert(poker_timeout.get_num_active() == 1);
        assert(poker_timeout.get_player_status(bob).revealed == 0);

        poker_timeout.check(false, {__test: true, user: alice});
        poker_timeout.reveal(string_hex(alice_seed_1), string_hex(sha256(alice_seed_2)), {__test: true, user: alice});
        poker_timeout.check(false, {__test: true, user: alice});
        poker_timeout.reveal(string_hex(alice_seed_2), string_hex(sha256(alice_seed_3)), {__test: true, user: alice});
        poker_timeout.check(false, {__test: true, user: alice});
        poker_timeout.reveal(string_hex(alice_seed_3), string_hex(sha256(alice_seed_4)), {__test: true, user: alice});
        poker_timeout.check(false, {__test: true, user: alice});

        __test.inc_height(6);
        poker_timeout.update();
        poker_timeout.claim({__test: true, user: bob, assert_fail: true});
        poker_timeout.claim({__test: true, user: alice});
        assert(__test.get_balance(alice, MMX) == 20);
        assert(__test.get_balance(bob, MMX) == 0);
        assert(__test.get_balance(timeout_addr, MMX) == 0);
    }

    // An expired sequence with an outstanding raise opens a response sequence.
    // If an underbetted player checks instead of calling, that player folds.
    {
        __test.set_height(0);
        const alice = string_bech32(sha256("bet_timeout_alice"));
        const bob = string_bech32(sha256("bet_timeout_bob"));
        const alice_seed_0 = sha256("bet_timeout_alice_seed_0");
        const alice_seed_1 = sha256("bet_timeout_alice_seed_1");
        const bob_seed_0 = sha256("bet_timeout_bob_seed_0");
        const bob_seed_1 = sha256("bet_timeout_bob_seed_1");

        poker_bet_timeout.join("Alice", string_hex(sha256(alice_seed_0)), string_hex(sha256("bet_timeout_alice_private")), {
            __test: true, user: alice, deposit: [10, MMX]
        });
        poker_bet_timeout.join("Bob", string_hex(sha256(bob_seed_0)), string_hex(sha256("bet_timeout_bob_private")), {
            __test: true, user: bob, deposit: [10, MMX]
        });
        poker_bet_timeout.reveal(string_hex(alice_seed_0), string_hex(sha256(alice_seed_1)), {__test: true, user: alice});
        poker_bet_timeout.reveal(string_hex(bob_seed_0), string_hex(sha256(bob_seed_1)), {__test: true, user: bob});

        poker_bet_timeout.bet({__test: true, user: alice, deposit: [10, MMX]});
        __test.inc_height(6);
        poker_bet_timeout.update();
        assert(!poker_bet_timeout.get_player_status(bob).folded);

        poker_bet_timeout.check(false, {__test: true, user: bob});
        poker_bet_timeout.check(false, {__test: true, user: alice});
        assert(poker_bet_timeout.get_player_status(bob).folded);
        assert(poker_bet_timeout.get_num_active() == 1);
        poker_bet_timeout.reveal(string_hex(alice_seed_1), string_hex(sha256("bet_timeout_alice_seed_2")), {
            __test: true, user: alice
        });
        assert(__test.get_balance(bet_timeout_addr, MMX) == 30);
    }

    // With no shown hands, the pot is split among active players. The first
    // active player receives the odd remainder; the folded player cannot claim.
    {
        __test.set_height(0);
        const alice = string_bech32(sha256("no_show_alice"));
        const bob = string_bech32(sha256("no_show_bob"));
        const carol = string_bech32(sha256("no_show_carol"));
        const alice_seed = [sha256("no_show_alice_0"), sha256("no_show_alice_1"), sha256("no_show_alice_2"), sha256("no_show_alice_3"), sha256("no_show_alice_4")];
        const bob_seed = [sha256("no_show_bob_0"), sha256("no_show_bob_1"), sha256("no_show_bob_2"), sha256("no_show_bob_3"), sha256("no_show_bob_4")];
        const carol_seed = [sha256("no_show_carol_0"), sha256("no_show_carol_1")];

        poker_no_show.join("Alice", string_hex(sha256(alice_seed[0])), string_hex(sha256("no_show_alice_private")), {
            __test: true, user: alice, deposit: [1, MMX]
        });
        poker_no_show.join("Bob", string_hex(sha256(bob_seed[0])), string_hex(sha256("no_show_bob_private")), {
            __test: true, user: bob, deposit: [1, MMX]
        });
        poker_no_show.join("Carol", string_hex(sha256(carol_seed[0])), string_hex(sha256("no_show_carol_private")), {
            __test: true, user: carol, deposit: [1, MMX]
        });

        poker_no_show.reveal(string_hex(alice_seed[0]), string_hex(sha256(alice_seed[1])), {__test: true, user: alice});
        poker_no_show.reveal(string_hex(bob_seed[0]), string_hex(sha256(bob_seed[1])), {__test: true, user: bob});
        poker_no_show.reveal(string_hex(carol_seed[0]), string_hex(sha256(carol_seed[1])), {__test: true, user: carol});
        poker_no_show.fold({__test: true, user: carol});
        poker_no_show.check(false, {__test: true, user: bob});
        poker_no_show.check(false, {__test: true, user: alice});

        for(var round = 1; round < 4; round++) {
            poker_no_show.reveal(string_hex(bob_seed[round]), string_hex(sha256(bob_seed[round + 1])), {__test: true, user: bob});
            poker_no_show.reveal(string_hex(alice_seed[round]), string_hex(sha256(alice_seed[round + 1])), {__test: true, user: alice});
            poker_no_show.check(false, {__test: true, user: alice});
            poker_no_show.check(false, {__test: true, user: bob});
        }

        __test.inc_height(6);
        poker_no_show.update();
        poker_no_show.claim({__test: true, user: carol, assert_fail: true});
        poker_no_show.claim({__test: true, user: alice});
        poker_no_show.claim({__test: true, user: bob});
        assert(__test.get_balance(alice, MMX) == 2);
        assert(__test.get_balance(bob, MMX) == 1);
        assert(__test.get_balance(carol, MMX) == 0);
        assert(__test.get_balance(no_show_addr, MMX) == 0);
    }

    // If everybody folds, each player receives exactly their own contribution.
    {
        __test.set_height(0);
        const alice = string_bech32(sha256("all_fold_alice"));
        const bob = string_bech32(sha256("all_fold_bob"));
        const alice_seed_0 = sha256("all_fold_alice_seed_0");
        const bob_seed_0 = sha256("all_fold_bob_seed_0");

        poker_all_fold.join("Alice", string_hex(sha256(alice_seed_0)), string_hex(sha256("all_fold_alice_private")), {
            __test: true, user: alice, deposit: [5, MMX]
        });
        poker_all_fold.join("Bob", string_hex(sha256(bob_seed_0)), string_hex(sha256("all_fold_bob_private")), {
            __test: true, user: bob, deposit: [5, MMX]
        });
        poker_all_fold.reveal(string_hex(alice_seed_0), string_hex(sha256("all_fold_alice_seed_1")), {__test: true, user: alice});
        poker_all_fold.reveal(string_hex(bob_seed_0), string_hex(sha256("all_fold_bob_seed_1")), {__test: true, user: bob});
        poker_all_fold.fold({__test: true, user: alice});
        poker_all_fold.fold({__test: true, user: bob});

        poker_all_fold.claim({__test: true, user: bob});
        poker_all_fold.claim({__test: true, user: alice});
        assert(__test.get_balance(alice, MMX) == 5);
        assert(__test.get_balance(bob, MMX) == 5);
        assert(__test.get_balance(all_fold_addr, MMX) == 0);
    }
}

main();
