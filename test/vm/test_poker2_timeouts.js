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
    init_args: [MMX, dealer, 10, 5, 2, 5, 100, 100]
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
                          shows, timeouts, {
        __test: true, user: dealer, assert_fail: true
    });

    poker2_timeout.settle(commits, commit_signatures, reveals, betting,
                          shows, timeouts, {
        __test: true, user: dealer
    });

    const alice_status = poker2_timeout.get_player_status(alice);
    const bob_status = poker2_timeout.get_player_status(bob);
    const carol_status = poker2_timeout.get_player_status(carol);

    assert(alice_status[1] == 40 && !alice_status[2] && alice_status[3] == 110);
    assert(bob_status[1] == 40 && !bob_status[2] && bob_status[3] == 109);
    assert(carol_status[1] == 20 && carol_status[2] && carol_status[3] == 80);
    assert(poker2_timeout.get_table_status()[4] == 1);

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
    poker2_refund.settle([], [], [], [], [], [], {
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
    assert(poker2_refund.get_player_status(alice)[4]);
    assert(poker2_refund.get_player_status(bob)[4]);
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

    const seeds = make_seeds("commit_timeout_alice");
    const commits = [];
    for(var i = 0; i < 5; i++) {
        push(commits, string_hex(poker2_commit_timeout.get_seed_commit(
            alice, i, string_hex(seeds[i]))));
    }
    const signature = string_hex(__test.ecdsa_sign(
        alice_skey, poker2_commit_timeout.get_commit_hash(alice, commits)));

    __test.set_height(5);
    poker2_commit_timeout.settle(
        [commits, []], [signature, null],
        [[null, null, null, null], [null, null, null, null]],
        [[], [], [], []], [], [[1, 0, 0, 0]],
        {__test: true, user: dealer}
    );

    const alice_status = poker2_commit_timeout.get_player_status(alice);
    const bob_status = poker2_commit_timeout.get_player_status(bob);
    assert(!alice_status[2] && alice_status[3] == 110);
    assert(bob_status[2] && bob_status[3] == 90);
    assert(poker2_commit_timeout.get_board() == null);

    poker2_commit_timeout.claim({__test: true, user: alice});
    poker2_commit_timeout.claim({__test: true, user: bob});
    assert(__test.get_balance(commit_timeout_addr, MMX) == 0);
}

test_action_timeout();
test_commit_timeout();
test_emergency_refund();
