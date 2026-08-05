interface __test;
interface poker2_timeout;
interface poker2_commit_timeout;
interface poker2_refund;

const MMX = string_bech32(bech32());
const binary = __test.compile("src/contract/poker2.js");

const dealer_key = __test.get_public_key(sha256("poker2_timeout_dealer"));
const dealer = string_bech32(sha256(dealer_key));

const alice_skey = sha256("poker2_timeout_alice");
const bob_skey = sha256("poker2_timeout_bob");
const carol_skey = sha256("poker2_timeout_carol");
const alice_key = __test.get_public_key(alice_skey);
const bob_key = __test.get_public_key(bob_skey);
const carol_key = __test.get_public_key(carol_skey);
const alice = string_bech32(sha256(alice_key));
const bob = string_bech32(sha256(bob_key));
const carol = string_bech32(sha256(carol_key));

const timeout_addr = poker2_timeout.__deploy({
    __type: "mmx.contract.Executable",
    binary: binary,
    init_args: [MMX, dealer, 10, 5, 3, 5, 100, 100]
});

const commit_timeout_addr = poker2_commit_timeout.__deploy({
    __type: "mmx.contract.Executable",
    binary: binary,
    init_args: [MMX, dealer, 10, 9, 3, 5, 100, 100]
});

const refund_addr = poker2_refund.__deploy({
    __type: "mmx.contract.Executable",
    binary: binary,
    init_args: [MMX, dealer, 10, 5, 3, 5, 100, 100]
});

function make_seeds(name)
{
    return [
        sha256(concat(name, "_0")),
        sha256(concat(name, "_1")),
        sha256(concat(name, "_2")),
        sha256(concat(name, "_3")),
        sha256(concat(name, "_pocket")),
    ];
}

function make_commits(address, seeds)
{
    const result = [];
    for(var i = 0; i < 5; i++) {
        push(result, string_hex(
            poker2_timeout.get_seed_commit(address, i, string_hex(seeds[i]))));
    }
    return result;
}

function sign_commit(skey, address, commits)
{
    return string_hex(__test.ecdsa_sign(
        skey, poker2_timeout.get_commit_hash(address, commits)));
}

function sign_action(skey, address, amount, checkpoint)
{
    return string_hex(__test.ecdsa_sign(skey,
        poker2_timeout.get_action_hash(address, 0, 0, 1, amount,
                                       string_hex(checkpoint))));
}

function get_first_action_checkpoint(alice_commits, bob_commits, carol_commits,
                                     alice_seed, bob_seed, carol_seed)
{
    var checkpoint = poker2_timeout.get_start_checkpoint();
    checkpoint = poker2_timeout.checkpoint_step(string_hex(checkpoint), 1, 0, 0,
        alice, 1, 100,
        string_hex(poker2_timeout.get_commit_hash(alice, alice_commits)));
    checkpoint = poker2_timeout.checkpoint_step(string_hex(checkpoint), 1, 0, 0,
        bob, 1, 100,
        string_hex(poker2_timeout.get_commit_hash(bob, bob_commits)));
    checkpoint = poker2_timeout.checkpoint_step(string_hex(checkpoint), 1, 0, 0,
        carol, 1, 100,
        string_hex(poker2_timeout.get_commit_hash(carol, carol_commits)));
    checkpoint = poker2_timeout.checkpoint_step(string_hex(checkpoint), 2, 0, 0,
        alice, 1, 10, string_hex(sha256(alice_seed)));
    checkpoint = poker2_timeout.checkpoint_step(string_hex(checkpoint), 2, 0, 0,
        bob, 1, 10, string_hex(sha256(bob_seed)));
    checkpoint = poker2_timeout.checkpoint_step(string_hex(checkpoint), 2, 0, 0,
        carol, 1, 10, string_hex(sha256(carol_seed)));
    return checkpoint;
}

function test_action_timeout()
{
    __test.set_height(0);

    poker2_timeout.join("Alice", string_hex(alice_key), {
        __test: true, user: alice, deposit: [100, MMX]
    });
    poker2_timeout.join("Bob", string_hex(bob_key), {
        __test: true, user: bob, deposit: [100, MMX]
    });
    poker2_timeout.join("Carol", string_hex(carol_key), {
        __test: true, user: carol, deposit: [100, MMX]
    });

    const alice_seeds = make_seeds("timeout_alice");
    const bob_seeds = make_seeds("timeout_bob");
    const carol_seeds = make_seeds("timeout_carol");
    const alice_commits = make_commits(alice, alice_seeds);
    const bob_commits = make_commits(bob, bob_seeds);
    const carol_commits = make_commits(carol, carol_seeds);
    const commits = [alice_commits, bob_commits, carol_commits];
    const commit_signatures = [
        sign_commit(alice_skey, alice, alice_commits),
        sign_commit(bob_skey, bob, bob_commits),
        sign_commit(carol_skey, carol, carol_commits),
    ];
    const reveals = [
        [string_hex(alice_seeds[0]), string_hex(alice_seeds[1]),
         string_hex(alice_seeds[2]), string_hex(alice_seeds[3])],
        [string_hex(bob_seeds[0]), string_hex(bob_seeds[1]),
         string_hex(bob_seeds[2]), string_hex(bob_seeds[3])],
        [string_hex(carol_seeds[0]), null, null, null],
    ];

    __test.set_height(5);
    const checkpoint = get_first_action_checkpoint(
        alice_commits, bob_commits, carol_commits,
        alice_seeds[0], bob_seeds[0], carol_seeds[0]);

    // Carol signs a valid increase to 20 against the old target of 10 while
    // Alice and Bob concurrently increase to 40. In the following epoch Carol
    // is still below 40; the dealer records her timeout, retaining 20 and
    // folding her. Alice and Bob's same timeout records are checks.
    const epoch = [
        [0, 1, 40, sign_action(alice_skey, alice, 40, checkpoint)],
        [1, 1, 40, sign_action(bob_skey, bob, 40, checkpoint)],
        [2, 1, 20, sign_action(carol_skey, carol, 20, checkpoint)],
    ];
    const betting = [[epoch, []], [[]], [[]], [[]]];
    const timeouts = [
        [0, 2, 0, 1], [1, 2, 0, 1], [2, 2, 0, 1],
        [0, 2, 1, 0], [1, 2, 1, 0],
        [0, 2, 2, 0], [1, 2, 2, 0],
        [0, 2, 3, 0], [1, 2, 3, 0],
    ];
    const shows = [
        [0, string_hex(alice_seeds[4]), [0, 1, 2, 3, 4]],
        [1, string_hex(bob_seeds[4]), [0, 1, 2, 3, 4]],
    ];

    // A signature from the wrong player cannot authorize Carol's action.
    const bad_epoch = [
        epoch[0], epoch[1],
        [2, 1, 20, sign_action(alice_skey, carol, 20, checkpoint)],
    ];
    poker2_timeout.settle(commits, commit_signatures, reveals,
                          [[bad_epoch, []], [[]], [[]], [[]]],
                          shows, timeouts, [], {
        __test: true, user: dealer, assert_fail: true
    });

    poker2_timeout.settle(commits, commit_signatures, reveals, betting,
                          shows, timeouts, [], {
        __test: true, user: dealer
    });

    const alice_status = poker2_timeout.get_player_status(alice);
    const bob_status = poker2_timeout.get_player_status(bob);
    const carol_status = poker2_timeout.get_player_status(carol);

    assert(alice_status[1] == 40 && !alice_status[2] && alice_status[3] == 110);
    assert(bob_status[1] == 40 && !bob_status[2] && bob_status[3] == 109);
    assert(carol_status[1] == 20 && carol_status[2] && carol_status[3] == 80);
    assert(poker2_timeout.get_table_status()[4] == 1);

    assert(__test.get_balance(timeout_addr, MMX) == 299);
    poker2_timeout.claim({__test: true, user: alice});
    poker2_timeout.claim({__test: true, user: bob});
    poker2_timeout.claim({__test: true, user: carol});
    assert(__test.get_balance(timeout_addr, MMX) == 0);
}

function test_emergency_refund()
{
    __test.set_height(0);

    poker2_refund.join("Too small", string_hex(alice_key), {
        __test: true, user: alice, deposit: [49, MMX], assert_fail: true
    });
    poker2_refund.join("Alice", string_hex(alice_key), {
        __test: true, user: alice, deposit: [70, MMX]
    });
    poker2_refund.join("Bob", string_hex(bob_key), {
        __test: true, user: bob, deposit: [130, MMX]
    });
    assert(__test.get_balance(refund_addr, MMX) == 200);

    __test.set_height(104);
    poker2_refund.refund({__test: true, user: alice, assert_fail: true});

    __test.set_height(105);
    poker2_refund.settle([], [], [], [], [], [], [], {
        __test: true, user: dealer, assert_fail: true
    });

    const alice_before = __test.get_balance(alice, MMX);
    const bob_before = __test.get_balance(bob, MMX);
    poker2_refund.refund({__test: true, user: alice});
    poker2_refund.refund({__test: true, user: bob});

    assert(__test.get_balance(alice, MMX) == alice_before + 70);
    assert(__test.get_balance(bob, MMX) == bob_before + 130);
    assert(__test.get_balance(refund_addr, MMX) == 0);
    assert(poker2_refund.get_table_status()[0] == 2);
    assert(poker2_refund.get_player_status(alice)[5]);
    assert(poker2_refund.get_player_status(bob)[5]);
}

function test_commit_timeout()
{
    __test.set_height(0);

    poker2_commit_timeout.join("Alice", string_hex(alice_key), {
        __test: true, user: alice, deposit: [100, MMX]
    });
    poker2_commit_timeout.join("Bob", string_hex(bob_key), {
        __test: true, user: bob, deposit: [100, MMX]
    });
    poker2_commit_timeout.join("Carol", string_hex(carol_key), {
        __test: true, user: carol, deposit: [100, MMX]
    });

    const seeds_0 = make_seeds("commit_timeout_alice_0");
    const commits_0 = [];
    for(var i = 0; i < 5; i++) {
        push(commits_0, string_hex(poker2_commit_timeout.get_seed_commit(
            alice, i, string_hex(seeds_0[i]))));
    }
    const commit_hash_0 = poker2_commit_timeout.get_commit_hash(alice, commits_0);
    const signature_0 = string_hex(__test.ecdsa_sign(alice_skey, commit_hash_0));

    __test.set_height(5);

    var checkpoint_0 = poker2_commit_timeout.get_start_checkpoint();
    checkpoint_0 = poker2_commit_timeout.checkpoint_step(
        string_hex(checkpoint_0), 1, 0, 0, alice, 1, 100,
        string_hex(commit_hash_0));
    checkpoint_0 = poker2_commit_timeout.checkpoint_step(
        string_hex(checkpoint_0), 1, 0, 0, bob, 2, 100, null);
    checkpoint_0 = poker2_commit_timeout.checkpoint_step(
        string_hex(checkpoint_0), 1, 0, 0, carol, 2, 100, null);

    const alice_continue_0 = string_hex(__test.ecdsa_sign(
        alice_skey, poker2_commit_timeout.get_continue_hash(
            alice, 120, string_hex(checkpoint_0))));
    const bob_continue_0 = string_hex(__test.ecdsa_sign(
        bob_skey, poker2_commit_timeout.get_continue_hash(
            bob, 90, string_hex(checkpoint_0))));

    poker2_commit_timeout.settle(
        [commits_0, [], []], [signature_0, null, null],
        [[null, null, null, null], [null, null, null, null],
         [null, null, null, null]],
        [[], [], [], []], [], [[1, 0, 0, 0], [2, 0, 0, 0]],
        [[0, alice_continue_0], [1, bob_continue_0]],
        {__test: true, user: dealer}
    );

    var alice_status = poker2_commit_timeout.get_player_status(alice);
    var bob_status = poker2_commit_timeout.get_player_status(bob);
    var carol_status = poker2_commit_timeout.get_player_status(carol);
    assert(alice_status[0] == 120 && alice_status[3] == 120 && alice_status[4]);
    assert(bob_status[0] == 90 && bob_status[3] == 90 && bob_status[4]);
    assert(carol_status[0] == 90 && carol_status[3] == 90 && !carol_status[4]);
    assert(poker2_commit_timeout.get_board() == null);
    assert(poker2_commit_timeout.get_table_status()[6] == 1);
    assert(poker2_commit_timeout.get_table_status()[7] == 2);
    assert(poker2_commit_timeout.get_table_status()[2] == 10);
    assert(poker2_commit_timeout.get_table_status()[3] == 110);
    assert(__test.get_balance(commit_timeout_addr, MMX) == 300);

    // A continuation signature locks the balance into the next hand.
    poker2_commit_timeout.claim({__test: true, user: alice, assert_fail: true});

    // Hand one starts with Alice and Bob. Carol requests activation after it
    // has started, which queues her for hand two without changing hand one's
    // active roster or checkpoint.
    __test.set_height(10);
    const checkpoint_before_activation = poker2_commit_timeout.get_start_checkpoint();
    poker2_commit_timeout.activate({__test: true, user: carol});
    assert(poker2_commit_timeout.get_num_active() == 2);
    assert(poker2_commit_timeout.get_num_next_active() == 1);
    assert(poker2_commit_timeout.get_start_checkpoint() == checkpoint_before_activation);
    assert(poker2_commit_timeout.get_player_status(carol)[6]);
    poker2_commit_timeout.claim({__test: true, user: carol, assert_fail: true});

    // All seed and action domains include hand_id, so the old commitment
    // signature cannot be replayed in this hand.
    const seeds_1 = make_seeds("commit_timeout_alice_1");
    const commits_1 = [];
    for(var i = 0; i < 5; i++) {
        push(commits_1, string_hex(poker2_commit_timeout.get_seed_commit(
            alice, i, string_hex(seeds_1[i]))));
    }
    const commit_hash_1 = poker2_commit_timeout.get_commit_hash(alice, commits_1);
    const signature_1 = string_hex(__test.ecdsa_sign(alice_skey, commit_hash_1));

    var checkpoint_1 = poker2_commit_timeout.get_start_checkpoint();
    checkpoint_1 = poker2_commit_timeout.checkpoint_step(
        string_hex(checkpoint_1), 1, 0, 0, alice, 1, 120,
        string_hex(commit_hash_1));
    checkpoint_1 = poker2_commit_timeout.checkpoint_step(
        string_hex(checkpoint_1), 1, 0, 0, bob, 2, 90, null);
    checkpoint_1 = poker2_commit_timeout.checkpoint_step(
        string_hex(checkpoint_1), 1, 0, 0, carol, 3, 0, null);

    const alice_continue_1 = string_hex(__test.ecdsa_sign(
        alice_skey, poker2_commit_timeout.get_continue_hash(
            alice, 130, string_hex(checkpoint_1))));
    const bob_continue_1 = string_hex(__test.ecdsa_sign(
        bob_skey, poker2_commit_timeout.get_continue_hash(
            bob, 80, string_hex(checkpoint_1))));

    poker2_commit_timeout.settle(
        [commits_1, [], []], [signature_0, null, null],
        [[null, null, null, null], [null, null, null, null],
         [null, null, null, null]],
        [[], [], [], []], [], [[1, 0, 0, 0]],
        [[0, alice_continue_1], [1, bob_continue_1]],
        {__test: true, user: dealer, assert_fail: true}
    );

    poker2_commit_timeout.settle(
        [commits_1, [], []], [signature_1, null, null],
        [[null, null, null, null], [null, null, null, null],
         [null, null, null, null]],
        [[], [], [], []], [], [[1, 0, 0, 0]],
        [[0, alice_continue_1], [1, bob_continue_1]],
        {__test: true, user: dealer}
    );

    alice_status = poker2_commit_timeout.get_player_status(alice);
    bob_status = poker2_commit_timeout.get_player_status(bob);
    carol_status = poker2_commit_timeout.get_player_status(carol);
    assert(alice_status[0] == 130 && alice_status[3] == 130 && alice_status[4]);
    // Bob signed continuation, but 80 is below this table's minimum stack of
    // 90, so settlement makes him inactive. Carol's queued activation fills
    // the second active seat.
    assert(bob_status[0] == 80 && bob_status[3] == 80 && !bob_status[4]);
    assert(carol_status[0] == 90 && carol_status[3] == 90 && carol_status[4]);
    assert(poker2_commit_timeout.get_num_active() == 2);
    assert(poker2_commit_timeout.get_num_next_active() == 0);
    assert(poker2_commit_timeout.get_table_status()[6] == 2);
    assert(poker2_commit_timeout.get_table_status()[2] == 15);
    assert(poker2_commit_timeout.get_table_status()[3] == 115);

    // Carol now participates in hand two while underfunded Bob sits out. Carol
    // times out during the commit phase; only Alice requests another hand, so
    // the minimum-two rule deactivates everybody after this settlement.
    __test.set_height(15);
    const seeds_2 = make_seeds("commit_timeout_alice_2");
    const commits_2 = [];
    for(var i = 0; i < 5; i++) {
        push(commits_2, string_hex(poker2_commit_timeout.get_seed_commit(
            alice, i, string_hex(seeds_2[i]))));
    }
    const commit_hash_2 = poker2_commit_timeout.get_commit_hash(alice, commits_2);
    const signature_2 = string_hex(__test.ecdsa_sign(alice_skey, commit_hash_2));

    var checkpoint_2 = poker2_commit_timeout.get_start_checkpoint();
    checkpoint_2 = poker2_commit_timeout.checkpoint_step(
        string_hex(checkpoint_2), 1, 0, 0, alice, 1, 130,
        string_hex(commit_hash_2));
    checkpoint_2 = poker2_commit_timeout.checkpoint_step(
        string_hex(checkpoint_2), 1, 0, 0, bob, 3, 0, null);
    checkpoint_2 = poker2_commit_timeout.checkpoint_step(
        string_hex(checkpoint_2), 1, 0, 0, carol, 2, 90, null);

    const alice_continue_2 = string_hex(__test.ecdsa_sign(
        alice_skey, poker2_commit_timeout.get_continue_hash(
            alice, 140, string_hex(checkpoint_2))));

    poker2_commit_timeout.settle(
        [commits_2, [], []], [signature_2, null, null],
        [[null, null, null, null], [null, null, null, null],
         [null, null, null, null]],
        [[], [], [], []], [], [[2, 0, 0, 0]],
        [[0, alice_continue_2]],
        {__test: true, user: dealer}
    );

    alice_status = poker2_commit_timeout.get_player_status(alice);
    bob_status = poker2_commit_timeout.get_player_status(bob);
    carol_status = poker2_commit_timeout.get_player_status(carol);
    assert(alice_status[0] == 140 && alice_status[3] == 140);
    assert(bob_status[0] == 80 && bob_status[3] == 80);
    assert(carol_status[0] == 80 && carol_status[3] == 80);
    assert(!alice_status[4] && !bob_status[4] && !carol_status[4]);
    assert(poker2_commit_timeout.get_num_active() == 0);
    assert(poker2_commit_timeout.get_table_status()[6] == 3);
    assert(poker2_commit_timeout.get_table_status()[3] == null);

    poker2_commit_timeout.claim({__test: true, user: alice});
    poker2_commit_timeout.claim({__test: true, user: bob});
    poker2_commit_timeout.claim({__test: true, user: carol});
    assert(__test.get_balance(commit_timeout_addr, MMX) == 0);
}

test_action_timeout();
test_commit_timeout();
test_emergency_refund();
