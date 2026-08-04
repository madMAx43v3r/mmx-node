interface __test;
interface poker2;

const MMX = string_bech32(bech32());
const binary = __test.compile("src/contract/poker2.js");

const dealer_skey = sha256("poker2_dealer_skey");
const dealer_key = __test.get_public_key(dealer_skey);
const dealer = string_bech32(sha256(dealer_key));

const alice_skey = sha256("poker2_alice_skey");
const bob_skey = sha256("poker2_bob_skey");
const carol_skey = sha256("poker2_carol_skey");

const alice_key = __test.get_public_key(alice_skey);
const bob_key = __test.get_public_key(bob_skey);
const carol_key = __test.get_public_key(carol_skey);

const alice = string_bech32(sha256(alice_key));
const bob = string_bech32(sha256(bob_key));
const carol = string_bech32(sha256(carol_key));

const table_addr = poker2.__deploy({
    __type: "mmx.contract.Executable",
    binary: binary,
    init_args: [MMX, dealer, 10, 5, 3, 5, 100, 100]
});

function make_seeds(name)
{
    return [
        sha256(concat(name, "_board_0")),
        sha256(concat(name, "_board_1")),
        sha256(concat(name, "_board_2")),
        sha256(concat(name, "_board_3")),
        sha256(concat(name, "_pocket")),
    ];
}

function make_commits(address, seeds)
{
    const result = [];
    for(var i = 0; i < 5; i++) {
        push(result, string_hex(poker2.get_seed_commit(address, i, string_hex(seeds[i]))));
    }
    return result;
}

function sign_commit(skey, address, commits)
{
    return string_hex(__test.ecdsa_sign(
        skey, poker2.get_commit_hash(address, commits)));
}

function sign_action(skey, address, round, epoch, action, amount, checkpoint)
{
    return string_hex(__test.ecdsa_sign(skey,
        poker2.get_action_hash(address, round, epoch, action, amount,
                               string_hex(checkpoint))));
}

function main()
{
    __test.set_height(0);

    poker2.join("Alice", string_hex(alice_key), {
        __test: true, user: alice, deposit: [100, MMX]
    });
    poker2.join("Bob", string_hex(bob_key), {
        __test: true, user: bob, deposit: [60, MMX]
    });
    poker2.join("Carol", string_hex(carol_key), {
        __test: true, user: carol, deposit: [200, MMX]
    });

    assert(__test.get_balance(table_addr, MMX) == 360);
    assert(!poker2.is_started());
    assert(poker2.get_table_status()[2] == 5);
    assert(poker2.get_table_status()[3] == 105);

    const alice_seeds = make_seeds("alice");
    const bob_seeds = make_seeds("bob");
    const carol_seeds = make_seeds("carol");

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
        [string_hex(alice_seeds[0]), string_hex(alice_seeds[1]), string_hex(alice_seeds[2]), string_hex(alice_seeds[3])],
        [string_hex(bob_seeds[0]), string_hex(bob_seeds[1]), string_hex(bob_seeds[2]), string_hex(bob_seeds[3])],
        [string_hex(carol_seeds[0]), string_hex(carol_seeds[1]), string_hex(carol_seeds[2]), string_hex(carol_seeds[3])],
    ];

    __test.set_height(5);
    assert(poker2.is_started());

    // Reproduce the checkpoint at the beginning of the first betting epoch.
    var checkpoint = poker2.get_start_checkpoint();
    checkpoint = poker2.checkpoint_step(string_hex(checkpoint), 1, 0, 0,
                                        alice, 1, 100,
                                        string_hex(poker2.get_commit_hash(alice, alice_commits)));
    checkpoint = poker2.checkpoint_step(string_hex(checkpoint), 1, 0, 0,
                                        bob, 1, 60,
                                        string_hex(poker2.get_commit_hash(bob, bob_commits)));
    checkpoint = poker2.checkpoint_step(string_hex(checkpoint), 1, 0, 0,
                                        carol, 1, 200,
                                        string_hex(poker2.get_commit_hash(carol, carol_commits)));
    checkpoint = poker2.checkpoint_step(string_hex(checkpoint), 2, 0, 0,
                                        alice, 1, 10, string_hex(sha256(alice_seeds[0])));
    checkpoint = poker2.checkpoint_step(string_hex(checkpoint), 2, 0, 0,
                                        bob, 1, 10, string_hex(sha256(bob_seeds[0])));
    checkpoint = poker2.checkpoint_step(string_hex(checkpoint), 2, 0, 0,
                                        carol, 1, 10, string_hex(sha256(carol_seeds[0])));

    // All three actions are concurrent and therefore sign the same checkpoint.
    // Alice and Bob go all-in at different levels. Carol's highest 100 is
    // uncalled and must be returned rather than raked.
    const first_epoch = [
        [0, 1, 100, sign_action(alice_skey, alice, 0, 0, 1, 100, checkpoint)],
        [1, 1, 60, sign_action(bob_skey, bob, 0, 0, 1, 60, checkpoint)],
        [2, 1, 200, sign_action(carol_skey, carol, 0, 0, 1, 200, checkpoint)],
    ];

    const betting = [[first_epoch], [], [], []];
    const shows = [
        [0, string_hex(alice_seeds[4]), [0, 1, 2, 3, 4]],
        [1, string_hex(bob_seeds[4]), [0, 1, 2, 3, 4]],
        [2, string_hex(carol_seeds[4]), [0, 1, 2, 3, 4]],
    ];

    poker2.settle(commits, commit_signatures, reveals, betting,
                  shows, [], {
        __test: true, user: dealer
    });

    assert(size(poker2.get_board()) == 5);
    assert(poker2.get_num_active() == 3);

    const alice_status = poker2.get_player_status(alice);
    const bob_status = poker2.get_player_status(bob);
    const carol_status = poker2.get_player_status(carol);

    assert(alice_status[0] == 100 && alice_status[1] == 100);
    assert(bob_status[0] == 60 && bob_status[1] == 60);
    assert(carol_status[0] == 200 && carol_status[1] == 200);

    // Matched pots: 180 main + 80 side. The 1% rake is 2. The first
    // roster entries receive deterministic split remainders.
    assert(alice_status[3] == 100);
    assert(bob_status[3] == 60);
    assert(carol_status[3] == 198);
    assert(poker2.get_table_status()[4] == 2);

    poker2.claim({__test: true, user: alice});
    poker2.claim({__test: true, user: bob});
    poker2.claim({__test: true, user: carol});

    assert(__test.get_balance(alice, MMX) == 100);
    assert(__test.get_balance(bob, MMX) == 60);
    assert(__test.get_balance(carol, MMX) == 198);
    assert(__test.get_balance(dealer, MMX) == 2);
    assert(__test.get_balance(table_addr, MMX) == 0);
}

main();
