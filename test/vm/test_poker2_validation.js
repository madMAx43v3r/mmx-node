import {equals} from "std";

interface __test;
interface poker2_join_validation;
interface poker2_lifecycle_validation;
interface poker2_waiting_validation;
interface poker2_settle_validation;
interface poker2_continue_validation;
interface poker2_bet_validation;
interface poker2_card_validation;
interface poker2_refund_history_validation;
interface poker2_large_rake_validation;

const MMX = string_bech32(bech32());
const OTHER = string_bech32(sha256("poker2_validation_other_currency"));
const binary = __test.compile("src/contract/poker2.js");

const dealer_key = __test.get_public_key(sha256("poker2_validation_dealer"));
const dealer = string_bech32(sha256(dealer_key));

function make_init_args(min_stack_blinds, max_players, min_rake, assert_fail = false)
{
    const args = [MMX, dealer, 10, min_stack_blinds, max_players, 5, 100, 100, min_rake];
    if(assert_fail) {
        push(args, {__test: true, assert_fail: true});
    }
    return args;
}

poker2_large_rake_validation.__deploy({
    __type: "mmx.contract.Executable",
    binary: binary,
    init_args: make_init_args(5, 2, 11, true)
});

const alice_skey = sha256("poker2_validation_alice");
const bob_skey = sha256("poker2_validation_bob");
const carol_skey = sha256("poker2_validation_carol");
const alice_key = __test.get_public_key(alice_skey);
const bob_key = __test.get_public_key(bob_skey);
const carol_key = __test.get_public_key(carol_skey);
const alice = string_bech32(sha256(alice_key));
const bob = string_bech32(sha256(bob_key));
const carol = string_bech32(sha256(carol_key));

const join_addr = poker2_join_validation.__deploy({
    __type: "mmx.contract.Executable",
    binary: binary,
    init_args: make_init_args(5, 2, 1)
});

const lifecycle_addr = poker2_lifecycle_validation.__deploy({
    __type: "mmx.contract.Executable",
    binary: binary,
    init_args: make_init_args(5, 2, 1)
});

const waiting_addr = poker2_waiting_validation.__deploy({
    __type: "mmx.contract.Executable",
    binary: binary,
    init_args: make_init_args(5, 3, 1)
});

const settle_addr = poker2_settle_validation.__deploy({
    __type: "mmx.contract.Executable",
    binary: binary,
    init_args: make_init_args(5, 2, 4)
});

const continue_addr = poker2_continue_validation.__deploy({
    __type: "mmx.contract.Executable",
    binary: binary,
    init_args: make_init_args(5, 2, 1)
});

const bet_addr = poker2_bet_validation.__deploy({
    __type: "mmx.contract.Executable",
    binary: binary,
    init_args: make_init_args(5, 2, 1)
});

poker2_card_validation.__deploy({
    __type: "mmx.contract.Executable",
    binary: binary,
    init_args: make_init_args(5, 2, 1)
});

const refund_history_addr = poker2_refund_history_validation.__deploy({
    __type: "mmx.contract.Executable",
    binary: binary,
    init_args: make_init_args(5, 2, 1)
});

function test_join_validation()
{
    __test.set_height(0);

    const config = poker2_join_validation.get_config();
    assert(string_bech32(config.currency) == MMX);
    assert(string_bech32(config.dealer) == dealer);
    assert(config.small_blind == 10 && config.min_stack == 50);
    assert(config.min_stack_blinds == null);
    assert(config.max_players == 2 && config.rake_bps == 100);
    assert(config.min_rake == 1);

    poker2_join_validation.join("small", string_hex(alice_key), {
        __test: true, user: alice, deposit: [49, MMX], assert_fail: true
    });
    poker2_join_validation.join("wrong currency", string_hex(alice_key), {
        __test: true, user: alice, deposit: [50, OTHER], assert_fail: true
    });
    poker2_join_validation.join("", string_hex(alice_key), {
        __test: true, user: alice, deposit: [50, MMX], assert_fail: true
    });
    poker2_join_validation.join("dealer", string_hex(dealer_key), {
        __test: true, user: dealer, deposit: [50, MMX], assert_fail: true
    });
    poker2_join_validation.join("wrong key", string_hex(bob_key), {
        __test: true, user: alice, deposit: [50, MMX], assert_fail: true
    });

    poker2_join_validation.join("Alice", string_hex(alice_key), {
        __test: true, user: alice, deposit: [50, MMX]
    });
    poker2_join_validation.join("Alice again", string_hex(alice_key), {
        __test: true, user: alice, deposit: [50, MMX], assert_fail: true
    });
    poker2_join_validation.top_up({
        __test: true, user: alice, deposit: [10, MMX], assert_fail: true
    });
    poker2_join_validation.join("Bob", string_hex(bob_key), {
        __test: true, user: bob, deposit: [50, MMX]
    });
    poker2_join_validation.join("Carol", string_hex(carol_key), {
        __test: true, user: carol, deposit: [50, MMX], assert_fail: true
    });

    assert(poker2_join_validation.get_num_active() == 2);
    assert(poker2_join_validation.get_table_status().player_count == 2);
    assert(__test.get_balance(join_addr, MMX) == 100);

    __test.set_height(5);
    poker2_join_validation.join("Carol", string_hex(carol_key), {
        __test: true, user: carol, deposit: [50, MMX], assert_fail: true
    });
}

function test_lifecycle_validation()
{
    __test.set_height(0);

    poker2_lifecycle_validation.join("Alice", string_hex(alice_key), {
        __test: true, user: alice, deposit: [50, MMX]
    });
    poker2_lifecycle_validation.deactivate({__test: true, user: alice});
    poker2_lifecycle_validation.claim({__test: true, user: alice});
    assert(poker2_lifecycle_validation.get_player_status(alice).withdrawn);

    poker2_lifecycle_validation.top_up({
        __test: true, user: alice, deposit: [5, MMX]
    });
    poker2_lifecycle_validation.activate({
        __test: true, user: alice, assert_fail: true
    });
    poker2_lifecycle_validation.top_up({
        __test: true, user: alice, deposit: [5, MMX]
    });
    poker2_lifecycle_validation.activate({__test: true, user: alice});
    assert(poker2_lifecycle_validation.get_player_status(alice).active);
    poker2_lifecycle_validation.claim({
        __test: true, user: alice, assert_fail: true
    });
    poker2_lifecycle_validation.deactivate({__test: true, user: alice});
    poker2_lifecycle_validation.claim({__test: true, user: alice});
    assert(__test.get_balance(lifecycle_addr, MMX) == 0);
}

function test_waiting_cancel()
{
    __test.set_height(0);

    poker2_waiting_validation.join("Alice", string_hex(alice_key), {
        __test: true, user: alice, deposit: [50, MMX]
    });
    poker2_waiting_validation.join("Bob", string_hex(bob_key), {
        __test: true, user: bob, deposit: [50, MMX]
    });
    __test.set_height(5);
    poker2_waiting_validation.join("Carol", string_hex(carol_key), {
        __test: true, user: carol, deposit: [50, MMX]
    });

    assert(poker2_waiting_validation.get_num_active() == 2);
    assert(poker2_waiting_validation.get_num_waiting() == 1);
    poker2_waiting_validation.claim({
        __test: true, user: carol, assert_fail: true
    });
    poker2_waiting_validation.deactivate({__test: true, user: carol});
    assert(poker2_waiting_validation.get_num_waiting() == 0);
    poker2_waiting_validation.claim({__test: true, user: carol});
    assert(__test.get_balance(waiting_addr, MMX) == 100);
}

function test_settlement_authorization()
{
    __test.set_height(0);

    poker2_settle_validation.join("Alice", string_hex(alice_key), {
        __test: true, user: alice, deposit: [100, MMX]
    });
    poker2_settle_validation.join("Bob", string_hex(bob_key), {
        __test: true, user: bob, deposit: [100, MMX]
    });

    const commitments = [[], []];
    const signatures = [null, null];
    const reveals = [
        [null, null, null, null],
        [null, null, null, null],
    ];
    const betting = [[], [], [], []];
    const timeouts = [[0, 0, 0, 0], [1, 0, 0, 0]];

    poker2_settle_validation.settle(
        commitments, signatures, reveals, betting, [], timeouts, [],
        {__test: true, user: dealer, assert_fail: true}
    );

    __test.set_height(5);
    poker2_settle_validation.settle(
        commitments, signatures, reveals, betting, [], timeouts, [],
        {__test: true, user: alice, assert_fail: true}
    );
    poker2_settle_validation.settle(
        commitments, signatures, reveals, betting, [], [], [],
        {__test: true, user: dealer, assert_fail: true}
    );
    poker2_settle_validation.settle(
        commitments, signatures, reveals, betting, [], timeouts, [],
        {__test: true, user: dealer}
    );

    assert(poker2_settle_validation.get_num_active() == 0);
    assert(poker2_settle_validation.get_player_status(alice).stack == 96);
    assert(poker2_settle_validation.get_player_status(bob).stack == 96);
    assert(poker2_settle_validation.get_table_status().dealer_rake == 8);
    poker2_settle_validation.claim({__test: true, user: alice});
    poker2_settle_validation.claim({__test: true, user: bob});
    assert(__test.get_balance(settle_addr, MMX) == 0);
}

function test_continuation_validation()
{
    __test.set_height(0);

    poker2_continue_validation.join("Alice", string_hex(alice_key), {
        __test: true, user: alice, deposit: [100, MMX]
    });
    poker2_continue_validation.join("Bob", string_hex(bob_key), {
        __test: true, user: bob, deposit: [100, MMX]
    });
    __test.set_height(5);

    var checkpoint = poker2_continue_validation.get_start_checkpoint();
    checkpoint = poker2_continue_validation.checkpoint_step(
        string_hex(checkpoint), 1, 0, 0, alice, 2, 100, null);
    checkpoint = poker2_continue_validation.checkpoint_step(
        string_hex(checkpoint), 1, 0, 0, bob, 2, 100, null);

    const alice_continue = string_hex(__test.ecdsa_sign(
        alice_skey, poker2_continue_validation.get_continue_hash(
            alice, 99, string_hex(checkpoint))));
    const bad_bob_continue = string_hex(__test.ecdsa_sign(
        alice_skey, poker2_continue_validation.get_continue_hash(
            bob, 99, string_hex(checkpoint))));
    const bob_continue = string_hex(__test.ecdsa_sign(
        bob_skey, poker2_continue_validation.get_continue_hash(
            bob, 99, string_hex(checkpoint))));

    const commitments = [[], []];
    const signatures = [null, null];
    const reveals = [
        [null, null, null, null],
        [null, null, null, null],
    ];
    const betting = [[], [], [], []];
    const timeouts = [[0, 0, 0, 0], [1, 0, 0, 0]];

    poker2_continue_validation.settle(
        commitments, signatures, reveals, betting, [], timeouts,
        [[0, alice_continue], [1, bad_bob_continue]],
        {__test: true, user: dealer, assert_fail: true}
    );
    poker2_continue_validation.settle(
        commitments, signatures, reveals, betting, [], timeouts,
        [[0, alice_continue], [1, bob_continue]],
        {__test: true, user: dealer}
    );

    assert(poker2_continue_validation.get_num_active() == 2);
    assert(poker2_continue_validation.get_table_status().hand_id == 1);

    // The old continuation signatures are bound to hand zero and its final
    // checkpoint, so they cannot keep players active after hand one.
    __test.set_height(10);
    poker2_continue_validation.settle(
        commitments, signatures, reveals, betting, [], timeouts,
        [[0, alice_continue], [1, bob_continue]],
        {__test: true, user: dealer, assert_fail: true}
    );
    assert(__test.get_balance(continue_addr, MMX) == 198);
}

function test_seed_and_bet_validation()
{
    __test.set_height(0);

    poker2_bet_validation.join("Alice", string_hex(alice_key), {
        __test: true, user: alice, deposit: [100, MMX]
    });
    poker2_bet_validation.join("Bob", string_hex(bob_key), {
        __test: true, user: bob, deposit: [100, MMX]
    });

    const alice_seeds = [
        sha256("bet_alice_0"), sha256("bet_alice_1"),
        sha256("bet_alice_2"), sha256("bet_alice_3"),
        sha256("bet_alice_pocket"),
    ];
    const bob_seeds = [
        sha256("bet_bob_0"), sha256("bet_bob_1"),
        sha256("bet_bob_2"), sha256("bet_bob_3"),
        sha256("bet_bob_pocket"),
    ];
    const alice_commits = [];
    const bob_commits = [];
    for(var i = 0; i < 5; i++) {
        push(alice_commits, string_hex(poker2_bet_validation.get_seed_commit(
            alice, i, string_hex(alice_seeds[i]))));
        push(bob_commits, string_hex(poker2_bet_validation.get_seed_commit(
            bob, i, string_hex(bob_seeds[i]))));
    }
    const commits = [alice_commits, bob_commits];
    const signatures = [
        string_hex(__test.ecdsa_sign(alice_skey,
            poker2_bet_validation.get_commit_hash(alice, alice_commits))),
        string_hex(__test.ecdsa_sign(bob_skey,
            poker2_bet_validation.get_commit_hash(bob, bob_commits))),
    ];

    __test.set_height(5);
    const reveals = [
        [string_hex(alice_seeds[0]), null, null, null],
        [string_hex(bob_seeds[0]), null, null, null],
    ];
    const bad_reveals = [
        [string_hex(sha256("wrong reveal")), null, null, null],
        reveals[1],
    ];
    poker2_bet_validation.settle(
        commits, signatures, bad_reveals, [[], [], [], []], [], [], [],
        {__test: true, user: dealer, assert_fail: true}
    );

    var checkpoint = poker2_bet_validation.get_start_checkpoint();
    checkpoint = poker2_bet_validation.checkpoint_step(
        string_hex(checkpoint), 1, 0, 0, alice, 1, 100,
        string_hex(poker2_bet_validation.get_commit_hash(alice, alice_commits)));
    checkpoint = poker2_bet_validation.checkpoint_step(
        string_hex(checkpoint), 1, 0, 0, bob, 1, 100,
        string_hex(poker2_bet_validation.get_commit_hash(bob, bob_commits)));
    checkpoint = poker2_bet_validation.checkpoint_step(
        string_hex(checkpoint), 2, 0, 0, alice, 1, 10,
        string_hex(sha256(alice_seeds[0])));
    checkpoint = poker2_bet_validation.checkpoint_step(
        string_hex(checkpoint), 2, 0, 0, bob, 1, 10,
        string_hex(sha256(bob_seeds[0])));

    const epoch_0 = [
        [0, 1, 100, string_hex(__test.ecdsa_sign(
            alice_skey, poker2_bet_validation.get_action_hash(
                alice, 0, 0, 1, 100, string_hex(checkpoint))))],
        [1, 1, 20, string_hex(__test.ecdsa_sign(
            bob_skey, poker2_bet_validation.get_action_hash(
                bob, 0, 0, 1, 20, string_hex(checkpoint))))],
    ];
    checkpoint = poker2_bet_validation.checkpoint_step(
        string_hex(checkpoint), 3, 0, 0, alice, 11, 100, null);
    checkpoint = poker2_bet_validation.checkpoint_step(
        string_hex(checkpoint), 3, 0, 0, bob, 11, 20, null);

    // Bob's cumulative bet increases by a full small blind, but 30 neither
    // matches the current 100 nor doubles his existing 20.
    const epoch_1 = [[1, 1, 30, string_hex(__test.ecdsa_sign(
        bob_skey, poker2_bet_validation.get_action_hash(
            bob, 0, 1, 1, 30, string_hex(checkpoint))))]];
    poker2_bet_validation.settle(
        commits, signatures, reveals, [[epoch_0, epoch_1], [], [], []],
        [], [], [],
        {__test: true, user: dealer, assert_fail: true}
    );
    assert(__test.get_balance(bet_addr, MMX) == 200);
}

function test_card_validation()
{
    const board = [
        ["2", "H"], ["3", "D"], ["4", "C"], ["5", "S"], ["6", "H"]
    ];
    const pocket = [["2", "H"], ["A", "S"]];

    poker2_card_validation.select_hand(
        board, pocket, [0, 1, 2, 3, 5],
        {__test: true, assert_fail: true}
    );
    const selected = poker2_card_validation.select_hand(
        board, pocket, [0, 1, 2, 3, 6]);
    assert(size(selected) == 5 && selected[4][0] == "A");
    poker2_card_validation.select_hand(
        board, pocket, [0, 1, 2, 3, 3],
        {__test: true, assert_fail: true}
    );

    const seed = sha256("duplicate draw seed");
    const seed_hex = string_hex(seed);
    const cards = poker2_card_validation.deal_cards(
        [seed_hex, seed_hex, seed_hex, seed_hex, seed_hex]);
    for(var i = 0; i < size(cards); i++) {
        for(var j = i + 1; j < size(cards); j++) {
            assert(!equals(cards[i], cards[j]));
        }
    }
}

function test_inactive_emergency_refund()
{
    __test.set_height(0);

    // Carol remains registered with a balance but is absent from both bounded
    // rosters when Alice and Bob start the hand.
    poker2_refund_history_validation.join("Carol", string_hex(carol_key), {
        __test: true, user: carol, deposit: [50, MMX]
    });
    poker2_refund_history_validation.deactivate({__test: true, user: carol});
    poker2_refund_history_validation.join("Alice", string_hex(alice_key), {
        __test: true, user: alice, deposit: [50, MMX]
    });
    poker2_refund_history_validation.join("Bob", string_hex(bob_key), {
        __test: true, user: bob, deposit: [50, MMX]
    });
    assert(poker2_refund_history_validation.get_num_active() == 2);
    assert(poker2_refund_history_validation.get_table_status().player_count == 3);

    __test.set_height(105);
    poker2_refund_history_validation.refund({__test: true, user: alice});
    assert(!poker2_refund_history_validation.get_table_status().table_open);
    poker2_refund_history_validation.refund({__test: true, user: carol});
    poker2_refund_history_validation.refund({__test: true, user: bob});
    assert(__test.get_balance(refund_history_addr, MMX) == 0);
}

test_join_validation();
test_lifecycle_validation();
test_waiting_cancel();
test_settlement_authorization();
test_continuation_validation();
test_seed_and_bet_validation();
test_card_validation();
test_inactive_emergency_refund();
