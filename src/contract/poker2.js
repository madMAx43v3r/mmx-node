import {equals, sort, reverse, compare} from "std";

// Dealer-settled, reusable parallel poker table.
//
// A player deposits their complete stack in join(). One small blind is treated
// as committed by every joined player when the game is settled. The remainder
// of the stack stays escrowed but is not part of a pot unless covered by a
// signed cumulative bet. After each settlement, signed continuations keep the
// resulting balances at the table for another hand. Other players become
// inactive and may withdraw manually.
//
// settle() arguments are arrays in player_list order unless noted otherwise.
// Players who join after a hand starts are outside its frozen
// hand_player_count prefix and wait for the following hand:
//
// commitments[player]     [] on commit timeout, otherwise five 32-byte hashes:
//                         four board-seed commitments and one pocket commitment
// commit_signatures       null on timeout, otherwise one signature over the
//                         player's complete commitment bundle
// reveals[player][round]  four board seeds, with null for unavailable reveals
// betting[round][epoch]   action entries [player, action, cumulative_bet, sig]
// shows                    sparse entries [player, pocket_seed, five indices]
// timeouts                [player, phase, round, epoch]
// continuations            sparse entries [player, signature]
//
// Timeout phases:
//   0 - commitment, 1 - board reveal, 2 - betting action, 3 - showdown
//
// Signed action types:
//   0 - check, 1 - bet / call / raise, 2 - fold
//
// Checkpoint phases and events:
//   phase 0 roster: event 0 inactive, 1 active
//   phase 1 commit: event 1 valid, 2 timeout, 3 inactive
//   phase 2 reveal: event 1 valid, 2 timeout, 3 already folded
//   phase 3 action: event 10 check, 11 bet, 12 fold,
//                   20 timeout-check, 21 timeout-fold,
//                   30 already folded, 31 all-in
//   phase 4 show:   event 1 valid, 2 timeout, 3 already folded, 4 mucked

var currency;               // bech32 address of the currency used for deposits and payouts
var dealer;                 // bech32 address of the dealer, who receives the rake
var small_blind;            // currency units, must be > 0
var min_stack;              // initial buy-in minimum, in currency units
var max_players;            // maximum number of players, must be between 2 and 10 inclusive
var start_delay;            // blocks after second player joins before game starts
var game_timeout;           // blocks after start before emergency refund is allowed
var rake_bps;               // 100 bps = 1%

var table_open = true;
var start_height = null;
var refund_height = null;
var hand_id = 0;
var hand_player_count = 0;  // frozen player_list prefix for the current hand

var player_map = {};
var player_list = [];

var board = null;
var transcript_hash = null;
var dealer_rake = 0;

function init(currency_, dealer_, small_blind_, min_stack_blinds_, max_players_,
              start_delay_, game_timeout_, rake_bps_ = 100)
{
    currency = bech32(currency_);
    dealer = bech32(dealer_);
    small_blind = uint(small_blind_);
    max_players = uint(max_players_);
    start_delay = uint(start_delay_);
    game_timeout = uint(game_timeout_);
    rake_bps = uint(rake_bps_);

    assert(dealer != bech32(), "invalid dealer");
    assert(small_blind > 0, "invalid small blind");
    assert(uint(min_stack_blinds_) >= 2, "invalid minimum stack");
    min_stack = uint(min_stack_blinds_) * small_blind;
    assert(min_stack / small_blind == uint(min_stack_blinds_), "minimum stack overflow");
    assert(max_players >= 2 && max_players <= 10, "invalid player limit");
    assert(start_delay > 0, "invalid start delay");
    assert(game_timeout > 0, "invalid game timeout");
    assert(rake_bps <= 10000, "invalid rake");
}

// Registers a new player at any time. Before a hand starts they join its active
// roster immediately; during a hand they wait for the following hand so the
// signed current-hand transcript remains unchanged.

function join(name, public_key) public payable
{
    assert(table_open, "table is closed");
    assert(this.user, "missing user");
    assert(this.user != dealer, "dealer cannot play");
    assert(this.deposit.currency == currency, "invalid currency");
    assert(this.deposit.amount >= min_stack, "stack below minimum");
    assert(player_map[this.user] == null, "already joined");
    assert(is_string(name) && size(name) > 0 && size(name) <= 24, "invalid name");

    const waiting = is_started();
    if(waiting) {
        assert(get_num_active() + get_num_waiting() < max_players,
               "next table full");
    } else {
        assert(get_num_active() < max_players, "table full");
    }

    public_key = binary_hex(public_key);
    assert(size(public_key) == 33, "invalid public key");
    assert(sha256(public_key) == this.user, "public key does not match user");

    const player = {
        name: name,
        address: this.user,
        public_key: public_key,
        stack: this.deposit.amount,
        active: !waiting,
        waiting: waiting,
        in_hand: false,
        bet: 0,
        folded: false,
        payout: 0,
        withdrawn: false,
    };

    player_map[this.user] = size(player_list);
    push(player_list, player);

    if(!waiting) {
        hand_player_count = size(player_list);
        if(get_num_active() == 2) {
            schedule_start();
        }
    }
}

// An inactive player with a sufficient balance may request an active seat at
// any time. Before a hand starts this changes its roster directly. During a
// hand it queues the player for the following hand without changing the
// current transcript.

function activate() public
{
    assert(table_open, "table is closed");

    const player = get_player(this.user);
    assert(!player.active, "player already active");
    assert(!player.waiting, "activation already requested");
    assert(player.stack >= small_blind, "stack below small blind");
    player.withdrawn = false;

    if(is_started()) {
        assert(get_num_active() + get_num_waiting() < max_players,
               "next table full");
        player.waiting = true;
    } else {
        assert(get_num_active() < max_players, "table full");
        player.active = true;
        if(get_num_active() == 2) {
            schedule_start();
        }
    }
}

// An active player may back out before the next hand starts. The sole active
// player may always stand down because a hand cannot proceed with one player.
// This explicitly marks them inactive, after which claim() becomes available.

function deactivate() public
{
    assert(table_open, "table is closed");

    const player = get_player(this.user);
    if(player.waiting) {
        player.waiting = false;
    } else {
        assert(player.active, "player already inactive");
        assert(!is_started() || get_num_active() == 1,
               "current hand already started");
        player.active = false;
    }

    if(get_num_active() < 2) {
        start_height = null;
        refund_height = null;
    }
}

// A player who withdrew or fell below one small blind can replenish their
// inactive table balance without registering a new identity.

function top_up() public payable
{
    assert(table_open, "table is closed");
    assert(this.deposit.currency == currency, "invalid currency");
    assert(this.deposit.amount > 0, "invalid deposit amount");

    const player = get_player(this.user);
    assert(!player.active, "active stack is locked");

    const next_stack = player.stack + this.deposit.amount;
    assert(next_stack > player.stack, "stack overflow");
    player.stack = next_stack;
    player.withdrawn = false;
}

function schedule_start()
{
    assert(get_num_active() >= 2, "not enough active players");
    hand_player_count = size(player_list);
    start_height = this.height + start_delay;
    assert(start_height > this.height, "start height overflow");
    refund_height = start_height + game_timeout;
    assert(refund_height > start_height, "refund height overflow");
}

function is_started() const public
{
    if(start_height) {
        return this.height >= start_height;
    }
    return false;
}

function is_expired() const public
{
    if(refund_height) {
        return this.height >= refund_height;
    }
    return false;
}

function get_table_status() const public
{
    return {
        table_open: bool(table_open),
        player_count: size(player_list),
        start_height: start_height,
        refund_height: refund_height,
        dealer_rake: dealer_rake,
        transcript_hash: transcript_hash,
        hand_id: hand_id,
        active_count: get_num_active(),
        waiting_count: get_num_waiting(),
        hand_player_count: hand_player_count,
    };
}

function get_config() const public
{
    return {
        currency: currency,
        dealer: dealer,
        small_blind: small_blind,
        min_stack: min_stack,
        max_players: max_players,
        start_delay: start_delay,
        game_timeout: game_timeout,
        rake_bps: rake_bps,
    };
}

function get_player_info(index) const public
{
    index = uint(index);
    assert(index < size(player_list), "invalid player index");
    const player = player_list[index];
    return {
        name: player.name,
        address: player.address,
        public_key: player.public_key,
        stack: player.stack,
    };
}

function get_player_status(address) const public
{
    const player = get_player(bech32(address));
    return {
        stack: player.stack,
        bet: player.bet,
        folded: bool(player.folded),
        payout: player.payout,
        active: bool(player.active),
        withdrawn: bool(player.withdrawn),
        waiting: bool(player.waiting),
    };
}

function get_board() const public
{
    return board;
}

// Commitment and signature helpers. These functions define the canonical
// messages used by the off-chain dealer and player clients.

function get_seed_commit(address, seed_round, seed) const public
{
    address = bech32(address);
    seed_round = uint(seed_round);
    seed = binary_hex(seed);

    assert(seed_round < 5, "invalid seed round");
    assert(size(seed) == 32, "seed must be 32 bytes");

    return sha256(concat(
        "MMX_PARALLEL_POKER_SEED_V1/",
        string_bech32(this.address), "/",
        string(hand_id), "/",
        string_bech32(address), "/",
        string(seed_round), "/",
        string_hex(seed)
    ));
}

function get_commit_hash(address, commitments) const public
{
    address = bech32(address);
    assert(is_array(commitments) && size(commitments) == 5,
           "invalid commitment bundle");

    var message = concat(
        "MMX_PARALLEL_POKER_COMMIT_V1/",
        string_bech32(this.address), "/",
        string(hand_id), "/",
        string_bech32(address)
    );
    for(const value_ of commitments) {
        const value = binary_hex(value_);
        assert(size(value) == 32, "invalid seed commitment");
        message = concat(message, "/", string_hex(value));
    }
    return sha256(message);
}

function get_action_hash(address, betting_round, epoch, action,
                         cumulative_bet, checkpoint) const public
{
    address = bech32(address);
    betting_round = uint(betting_round);
    epoch = uint(epoch);
    action = uint(action);
    cumulative_bet = uint(cumulative_bet);
    checkpoint = binary_hex(checkpoint);

    assert(betting_round < 4, "invalid betting round");
    assert(action < 3, "invalid action");
    assert(size(checkpoint) == 32, "invalid checkpoint");

    return sha256(concat(
        "MMX_PARALLEL_POKER_ACTION_V1/",
        string_bech32(this.address), "/",
        string(hand_id), "/",
        string_bech32(address), "/",
        string(betting_round), "/",
        string(epoch), "/",
        string(action), "/",
        string(cumulative_bet), "/",
        string_hex(checkpoint)
    ));
}

// Generic checkpoint transition used by both settle() and off-chain clients.
// phase and event are protocol integers; data_hash may be null when the event
// has no associated seed or commitment digest.

function checkpoint_step(checkpoint, phase, event_round, epoch, address,
                         event, amount, data_hash = null) const public
{
    checkpoint = binary_hex(checkpoint);
    phase = uint(phase);
    event_round = uint(event_round);
    epoch = uint(epoch);
    address = bech32(address);
    event = uint(event);
    amount = uint(amount);

    assert(size(checkpoint) == 32, "invalid checkpoint");

    var data = "";
    if(data_hash != null) {
        data_hash = binary_hex(data_hash);
        assert(size(data_hash) == 32, "invalid checkpoint data");
        data = string_hex(data_hash);
    }

    return sha256(concat(
        "MMX_PARALLEL_POKER_CHECKPOINT_V1/",
        string_bech32(this.address), "/",
        string(hand_id), "/",
        string_hex(checkpoint), "/",
        string(phase), "/",
        string(event_round), "/",
        string(epoch), "/",
        string_bech32(address), "/",
        string(event), "/",
        string(amount), "/",
        data
    ));
}

function get_start_checkpoint() const public
{
    assert(start_height != null, "game not scheduled");
    assert(hand_player_count <= size(player_list), "invalid hand player count");

    var checkpoint = sha256(concat(
        "MMX_PARALLEL_POKER_START_V1/",
        string_bech32(this.address), "/",
        string(hand_id), "/",
        string(start_height)
    ));
    for(var i = 0; i < hand_player_count; i++) {
        const player = player_list[i];
        var checkpoint_stack = 0;
        if(player.active) {
            checkpoint_stack = player.stack;
        }
        checkpoint = checkpoint_step(checkpoint, 0, 0, 0,
                                     player.address, uint(player.active), checkpoint_stack);
    }
    return checkpoint;
}

function get_continue_hash(address, result_stack, checkpoint) const public
{
    address = bech32(address);
    result_stack = uint(result_stack);
    checkpoint = binary_hex(checkpoint);
    assert(size(checkpoint) == 32, "invalid checkpoint");

    return sha256(concat(
        "MMX_PARALLEL_POKER_CONTINUE_V1/",
        string_bech32(this.address), "/",
        string(hand_id), "/",
        string_bech32(address), "/",
        string(result_stack), "/",
        string_hex(checkpoint)
    ));
}

function settle(commitments, commit_signatures, reveals, betting,
                shows, timeouts, continuations) public
{
    assert(table_open, "table is closed");
    assert(this.user == dealer, "only dealer can settle");
    assert(is_started(), "game not started");
    assert(!is_expired(), "game expired");
    assert(get_num_active() >= 2, "not enough active players");

    const count = hand_player_count;
    assert(count >= 2, "not enough players");
    assert(count <= size(player_list), "invalid hand player count");
    assert(is_array(commitments) && size(commitments) == count,
           "invalid commitments");
    assert(is_array(commit_signatures) && size(commit_signatures) == count,
           "invalid commitment signatures");
    assert(is_array(reveals) && size(reveals) == count, "invalid reveals");
    assert(is_array(betting) && size(betting) == 4, "invalid betting transcript");
    assert(is_array(shows), "invalid shows");
    assert(is_array(timeouts), "invalid timeouts");
    assert(is_array(continuations), "invalid continuations");

    const timeout_used = validate_timeouts(timeouts, count);
    const show_used = validate_shows(shows, count);
    const continuation_used = validate_continuations(
        continuations, size(player_list));
    const commit_values = [];
    const ranks = [];

    board = null;
    dealer_rake = 0;

    for(var i = 0; i < count; i++) {
        const player = player_list[i];
        player.in_hand = bool(player.active);
        if(player.in_hand) {
            assert(player.stack >= small_blind, "active stack below small blind");
            player.bet = small_blind;
            player.folded = false;
            player.payout = 0;
        } else {
            player.bet = 0;
            player.folded = true;
            player.payout = player.stack;
        }
        push(ranks, null);

        assert(is_array(reveals[i]) && size(reveals[i]) == 4,
               "invalid player reveals");
    }

    var checkpoint = get_start_checkpoint();

    // Every valid participant signs all five commitments as one bundle before
    // any reveal is released. A commit timeout immediately folds the player.
    for(var i = 0; i < count; i++) {
        const player = player_list[i];
        const values = commitments[i];
        assert(is_array(values), "invalid player commitments");

        if(player.in_hand) {
            if(size(values) == 5) {
                const decoded = [];
                for(const value_ of values) {
                    const value = binary_hex(value_);
                    assert(size(value) == 32, "invalid seed commitment");
                    push(decoded, value);
                }
                const commit_hash = get_commit_hash(player.address, decoded);
                const signature = binary_hex(commit_signatures[i]);
                assert(size(signature) == 64, "invalid commitment signature");
                assert(ecdsa_verify(commit_hash, player.public_key, signature),
                       "commitment signature verification failed");
                push(commit_values, decoded);
                checkpoint = checkpoint_step(checkpoint, 1, 0, 0,
                                             player.address, 1, player.stack,
                                             commit_hash);
            } else {
                assert(size(values) == 0, "invalid player commitments");
                assert(commit_signatures[i] == null,
                       "signature without commitments");
                assert(use_timeout(timeouts, timeout_used, i, 0, 0, 0),
                       "missing commitment timeout");
                player.folded = true;
                push(commit_values, []);
                checkpoint = checkpoint_step(checkpoint, 1, 0, 0,
                                             player.address, 2, player.stack);
            }
        } else {
            assert(size(values) == 0, "invalid player commitments");
            assert(commit_signatures[i] == null, "inactive commitment signature");
            push(commit_values, []);
            checkpoint = checkpoint_step(checkpoint, 1, 0, 0,
                                         player.address, 3, 0);
        }
    }

    const sources = [];
    var rounds_processed = 0;

    for(var round = 0; round < 4; round++) {
        if(get_num_hand_active() > 1) {

            var seed_data = binary();

        // Reveals are processed in roster order, after the dealer has collected
        // the full batch or recorded the corresponding timeouts.
        for(var i = 0; i < count; i++) {
            const player = player_list[i];
            const seed_ = reveals[i][round];

            if(!player.folded) {
                if(seed_ != null) {
                    const seed = binary_hex(seed_);
                    assert(size(seed) == 32, "seed must be 32 bytes");
                    assert(get_seed_commit(player.address, round, seed)
                           == commit_values[i][round], "invalid seed reveal");
                    seed_data = concat(seed_data, seed);
                    checkpoint = checkpoint_step(checkpoint, 2, round, 0,
                                                 player.address, 1, player.bet,
                                                 sha256(seed));
                } else {
                    assert(use_timeout(timeouts, timeout_used, i, 1, round, 0),
                           "missing reveal timeout");
                    player.folded = true;
                    checkpoint = checkpoint_step(checkpoint, 2, round, 0,
                                                 player.address, 2, player.bet);
                }
            } else {
                assert(seed_ == null, "reveal from folded player");
                checkpoint = checkpoint_step(checkpoint, 2, round, 0,
                                             player.address, 3, player.bet);
            }
        }

        var source = sha256(seed_data);
        if(round > 0) {
            source = sha256(concat(sources[round - 1], source));
        }
        push(sources, source);

            checkpoint = process_betting_round(round, betting[round], checkpoint,
                                               timeouts, timeout_used);
            rounds_processed++;
        }
    }

    validate_unused_rounds(rounds_processed, reveals, betting);

    // A showdown is only needed when at least two players survive all four
    // reveal and betting rounds.
    if(get_num_hand_active() > 1) {
        assert(rounds_processed == 4 && size(sources) == 4,
               "incomplete board transcript");
        board = deal_cards([
            sha256(concat(binary_hex("F1"), sources[1])),
            sha256(concat(binary_hex("F2"), sources[1])),
            sha256(concat(binary_hex("F3"), sources[1])),
            sources[2],
            sources[3]
        ]);

        const global_seed = sources[0];
        for(var i = 0; i < count; i++) {
            const player = player_list[i];
            const show_index = find_show(shows, i);

            if(!player.folded) {
                if(show_index != null) {
                    const show = shows[show_index];
                    show_used[show_index] = true;

                    const private_seed = binary_hex(show[1]);
                    const hand = show[2];
                    assert(size(private_seed) == 32,
                           "private seed must be 32 bytes");
                    assert(get_seed_commit(player.address, 4, private_seed)
                           == commit_values[i][4], "invalid private seed");
                    assert(is_array(hand) && size(hand) == 5, "invalid hand");

                    const source = sha256(concat(global_seed, private_seed));
                    const pocket = deal_cards([
                        sha256(concat(binary_hex("A1"), source)),
                        sha256(concat(binary_hex("A2"), source))
                    ]);
                    ranks[i] = get_rank(select_hand(board, pocket, hand));
                    checkpoint = checkpoint_step(checkpoint, 4, 4, 0,
                                                 player.address, 1, player.bet,
                                                 sha256(private_seed));
                } else {
                    player.folded = true;
                    if(use_timeout(timeouts, timeout_used, i, 3, 4, 0)) {
                        checkpoint = checkpoint_step(checkpoint, 4, 4, 0,
                                                     player.address, 2, player.bet);
                    } else {
                        checkpoint = checkpoint_step(checkpoint, 4, 4, 0,
                                                     player.address, 4, player.bet);
                    }
                }
            } else {
                assert(show_index == null, "show from folded player");
                checkpoint = checkpoint_step(checkpoint, 4, 4, 0,
                                             player.address, 3, player.bet);
            }
        }
    } else {
        assert(size(shows) == 0, "unexpected show");
    }

    assert_all_shows_used(show_used);
    assert_all_timeouts_used(timeout_used);

    transcript_hash = checkpoint;
    allocate_payouts(ranks);

    var active_count = 0;
    for(var i = 0; i < size(player_list); i++) {
        const player = player_list[i];
        const continuation_index = find_continuation(continuations, i);

        if(player.in_hand) {
            assert(!player.waiting, "active player queued activation");
            if(continuation_index != null) {
                const signature = binary_hex(continuations[continuation_index][1]);
                assert(size(signature) == 64, "invalid continuation signature");
                assert(ecdsa_verify(
                    get_continue_hash(player.address, player.payout, transcript_hash),
                    player.public_key, signature),
                    "continuation signature verification failed");
                continuation_used[continuation_index] = true;
                if(player.payout >= small_blind) {
                    assert(active_count < max_players, "too many active players");
                    player.active = true;
                    active_count++;
                } else {
                    player.active = false;
                }
            } else {
                player.active = false;
            }
        } else {
            assert(continuation_index == null,
                   "inactive player supplied continuation");
            if(player.waiting && player.payout >= small_blind) {
                assert(active_count < max_players, "too many active players");
                player.active = true;
                active_count++;
            } else {
                player.active = false;
            }
        }
        player.waiting = false;
    }

    // A lone player cannot start another hand. Their signature does not block
    // this settlement; they become inactive and may withdraw with everyone
    // else who chose not to continue.
    if(active_count < 2) {
        for(const player of player_list) {
            player.active = false;
        }
    }
    assert_all_continuations_used(continuation_used);

    for(const player of player_list) {
        player.stack = player.payout;
        player.in_hand = false;
    }
    if(dealer_rake > 0) {
        send(dealer, dealer_rake, currency, "poker_rake");
    }

    const next_hand_id = hand_id + 1;
    assert(next_hand_id > hand_id, "hand id overflow");
    hand_id = next_hand_id;

    if(get_num_active() >= 2) {
        schedule_start();
    } else {
        start_height = null;
        refund_height = null;
    }
}

function process_betting_round(round, epochs, checkpoint,
                               timeouts, timeout_used)
{
    assert(is_array(epochs), "invalid betting round");

    if(get_num_hand_active() <= 1 || get_num_actors() == 0) {
        assert(size(epochs) == 0, "unexpected betting epoch");
        return checkpoint;
    }

    var done = false;

    for(var epoch = 0; epoch < size(epochs); epoch++) {
        assert(!done, "betting continued after completion");
        const entries = epochs[epoch];
        assert(is_array(entries), "invalid betting epoch");

        const entry_used = [];
        for(var e = 0; e < size(entries); e++) {
            const entry = entries[e];
            assert(is_array(entry) && size(entry) == 4, "invalid action entry");
            assert(is_uint(entry[0]) && entry[0] < hand_player_count,
                   "invalid action player");
            assert(is_uint(entry[1]) && entry[1] < 3, "invalid action type");
            assert(is_uint(entry[2]), "invalid action amount");
            push(entry_used, false);
        }

        const epoch_checkpoint = checkpoint;
        const target = get_current_bet();

        for(var i = 0; i < hand_player_count; i++) {
            const player = player_list[i];
            const entry_index = find_action(entries, i);
            var event = 30;     // already folded / inactive

            if(!player.folded && player.bet < player.stack) {
                if(entry_index != null) {
                    const entry = entries[entry_index];
                    entry_used[entry_index] = true;

                    const action = entry[1];
                    const amount = entry[2];
                    const signature = binary_hex(entry[3]);
                    assert(size(signature) == 64, "invalid action signature");
                    assert(ecdsa_verify(
                        get_action_hash(player.address, round, epoch,
                                        action, amount, epoch_checkpoint),
                        player.public_key, signature),
                        "action signature verification failed");

                    if(action == 0) {
                        assert(amount == player.bet, "check changes bet");
                        assert(player.bet == target, "cannot check facing a bet");
                        event = 10;
                    } else if(action == 1) {
                        assert(amount > player.bet, "bet did not increase");
                        assert(amount <= player.stack, "bet exceeds stack");
                        assert(amount - player.bet >= small_blind
                               || amount == player.stack,
                               "bet increment below small blind");
                        assert(amount == target
                               || amount / 2 >= player.bet
                               || amount == player.stack,
                               "bet must match, double, or go all-in");
                        player.bet = amount;
                        event = 11;
                    } else {
                        assert(amount == player.bet, "fold changes bet");
                        player.folded = true;
                        event = 12;
                    }
                } else {
                    assert(use_timeout(timeouts, timeout_used, i, 2,
                                       round, epoch), "missing action timeout");
                    if(player.bet == target) {
                        event = 20;     // dealer-attested timeout check
                    } else {
                        player.folded = true;
                        event = 21;     // dealer-attested timeout fold
                    }
                }
            } else {
                assert(entry_index == null, "action from inactive player");
                if(!player.folded) {
                    event = 31;         // all-in, no action required
                }
            }

            checkpoint = checkpoint_step(checkpoint, 3, round, epoch,
                                         player.address, event, player.bet);
        }

        for(var e = 0; e < size(entry_used); e++) {
            assert(entry_used[e], "unused action entry");
        }

        const next_target = get_current_bet();
        done = true;
        for(const player of player_list) {
            if(player.in_hand && !player.folded && player.bet < player.stack
               && player.bet < next_target) {
                done = false;
            }
        }
        if(get_num_hand_active() <= 1) {
            done = true;
        }
    }

    assert(done, "incomplete betting round");
    return checkpoint;
}

function validate_unused_rounds(processed, reveals, betting) const
{
    for(var round = processed; round < 4; round++) {
        assert(size(betting[round]) == 0, "unexpected betting transcript");
        for(var i = 0; i < hand_player_count; i++) {
            assert(reveals[i][round] == null, "unexpected seed reveal");
        }
    }
}

function find_action(entries, player_index) const
{
    var result = null;
    for(var i = 0; i < size(entries); i++) {
        if(entries[i][0] == player_index) {
            assert(result == null, "duplicate player action");
            result = i;
        }
    }
    return result;
}

function validate_shows(shows, count) const
{
    const used = [];
    for(const show of shows) {
        assert(is_array(show) && size(show) == 3, "invalid show entry");
        assert(is_uint(show[0]) && show[0] < count, "invalid show player");
        assert(is_array(show[2]), "invalid show hand");
        push(used, false);
    }
    return used;
}

function find_show(shows, player_index) const
{
    var result = null;
    for(var i = 0; i < size(shows); i++) {
        if(shows[i][0] == player_index) {
            assert(result == null, "duplicate player show");
            result = i;
        }
    }
    return result;
}

function assert_all_shows_used(used) const
{
    for(const value of used) {
        assert(value, "unused show entry");
    }
}

function validate_continuations(continuations, count) const
{
    const used = [];
    for(const continuation of continuations) {
        assert(is_array(continuation) && size(continuation) == 2,
               "invalid continuation entry");
        assert(is_uint(continuation[0]) && continuation[0] < count,
               "invalid continuation player");
        push(used, false);
    }
    return used;
}

function find_continuation(continuations, player_index) const
{
    var result = null;
    for(var i = 0; i < size(continuations); i++) {
        if(continuations[i][0] == player_index) {
            assert(result == null, "duplicate player continuation");
            result = i;
        }
    }
    return result;
}

function assert_all_continuations_used(used) const
{
    for(const value of used) {
        assert(value, "unused continuation entry");
    }
}

function validate_timeouts(timeouts, count) const
{
    const used = [];
    for(const record of timeouts) {
        assert(is_array(record) && size(record) == 4, "invalid timeout record");
        assert(is_uint(record[0]) && record[0] < count, "invalid timeout player");
        assert(is_uint(record[1]) && record[1] < 4, "invalid timeout phase");
        assert(is_uint(record[2]) && record[2] <= 4, "invalid timeout round");
        assert(is_uint(record[3]), "invalid timeout epoch");
        push(used, false);
    }
    return used;
}

function use_timeout(timeouts, used, player, phase, round, epoch)
{
    var result = null;
    for(var i = 0; i < size(timeouts); i++) {
        const record = timeouts[i];
        if(record[0] == player && record[1] == phase
           && record[2] == round && record[3] == epoch) {
            assert(result == null, "duplicate timeout record");
            result = i;
        }
    }
    if(result != null) {
        assert(!used[result], "timeout already used");
        used[result] = true;
        return true;
    }
    return false;
}

function assert_all_timeouts_used(used) const
{
    for(const value of used) {
        assert(value, "unused timeout record");
    }
}

function get_current_bet() const
{
    var amount = 0;
    for(const player of player_list) {
        if(player.in_hand && !player.folded && player.bet > amount) {
            amount = player.bet;
        }
    }
    return amount;
}

// Returns the number of players enrolled in the next hand. During settlement,
// this is also the fixed roster for the hand being verified. Inactive and
// waiting players are not included.

function get_num_active() const public
{
    var count = 0;
    for(const player of player_list) {
        if(player.active) {
            count++;
        }
    }
    return count;
}

function get_num_waiting() const public
{
    var count = 0;
    for(const player of player_list) {
        if(player.waiting) {
            count++;
        }
    }
    return count;
}

// Returns the number of current-hand players who have not folded. This
// includes all-in players, who remain eligible to win despite no longer being
// able to take betting actions.

function get_num_hand_active() const
{
    var count = 0;
    for(const player of player_list) {
        if(player.in_hand && !player.folded) {
            count++;
        }
    }
    return count;
}

// Returns the number of current-hand players who may still take a betting
// action: they have not folded and are not all-in. Betting rounds stop when
// fewer than two actors remain.

function get_num_actors() const
{
    var count = 0;
    for(const player of player_list) {
        if(player.in_hand && !player.folded && player.bet < player.stack) {
            count++;
        }
    }
    return count;
}

function get_player(address) const
{
    const index = player_map[address];
    assert(index != null, "not a player");
    return player_list[index];
}

// Initializes every result with unused stack, then distributes standard main
// and side-pot layers. A one-player layer is uncalled and returned. A layer
// with no surviving eligible player is returned contributor-by-contributor.

function allocate_payouts(ranks)
{
    const layer_amounts = [];
    const layer_winners = [];
    var matched_total = 0;
    var total_stack = 0;

    for(const player of player_list) {
        if(player.in_hand) {
            assert(player.bet >= small_blind && player.bet <= player.stack,
                   "invalid final bet");
            player.payout = player.stack - player.bet;
        } else {
            assert(player.bet == 0, "inactive player bet");
            player.payout = player.stack;
        }
        total_stack += player.stack;
    }

    var previous = 0;
    for(var layer_index = 0; layer_index < size(player_list); layer_index++) {
        var level = null;
        for(const player of player_list) {
            if(player.bet > previous) {
                if(level == null) {
                    level = player.bet;
                } else if(player.bet < level) {
                    level = player.bet;
                }
            }
        }
        if(level != null) {
            const delta = level - previous;
            const contributors = [];
            const eligible = [];
            for(var i = 0; i < size(player_list); i++) {
                const player = player_list[i];
                if(player.bet >= level) {
                    push(contributors, i);
                    if(player.in_hand && !player.folded) {
                        push(eligible, i);
                    }
                }
            }

            if(size(contributors) == 1 || size(eligible) == 0) {
                for(const index of contributors) {
                    player_list[index].payout += delta;
                }
            } else {
                const amount = delta * size(contributors);
                const winners = get_layer_winners(eligible, ranks);
                push(layer_amounts, amount);
                push(layer_winners, winners);
                matched_total += amount;
            }
            previous = level;
        }
    }

    dealer_rake = (matched_total * rake_bps) / 10000;

    var matched_seen = 0;
    var rake_seen = 0;
    for(var layer = 0; layer < size(layer_amounts); layer++) {
        const amount = layer_amounts[layer];
        const winners = layer_winners[layer];

        matched_seen += amount;
        const rake_until = dealer_rake * matched_seen / matched_total;
        const layer_rake = rake_until - rake_seen;
        rake_seen = rake_until;

        const net = amount - layer_rake;
        for(var i = 0; i < size(winners); i++) {
            player_list[winners[i]].payout += get_split_amount(net, size(winners), i);
        }
    }

    var total_payout = dealer_rake;
    for(const player of player_list) {
        total_payout += player.payout;
    }
    assert(total_payout == total_stack, "payout accounting mismatch");
}

function get_layer_winners(eligible, ranks) const
{
    if(size(eligible) == 1) {
        return [eligible[0]];
    }

    var winners = [];
    var winning_rank = null;

    for(const index of eligible) {
        const rank = ranks[index];
        assert(rank != null, "missing showdown hand");

        if(winning_rank == null) {
            winning_rank = rank;
            push(winners, index);
        } else {
            const result = compare_rank(rank, winning_rank);
            if(result == "GT") {
                winning_rank = rank;
                winners = [];
                push(winners, index);
            } else if(result == "EQ") {
                push(winners, index);
            }
        }
    }
    return winners;
}

function get_split_amount(total, count, index) const public
{
    total = uint(total);
    count = uint(count);
    index = uint(index);

    assert(count > 0, "invalid split count");
    assert(index < count, "invalid split index");

    var amount = total / count;
    if(index < total % count) {
        amount++;
    }
    return amount;
}

// A player may withdraw their complete table balance after a settlement in
// which they did not sign to continue. Continuing players remain locked into
// the next hand.

function claim() public
{
    assert(table_open, "table is closed");
    const player = get_player(this.user);
    assert(!player.active, "player chose to continue");
    assert(!player.waiting, "player requested activation");
    assert(player.stack > 0, "nothing to claim");

    const amount = player.stack;
    player.stack = 0;
    player.withdrawn = true;
    send(this.user, amount, currency, "poker_payout");
}

// If the dealer fails to settle the current hand, the emergency deadline
// closes the table and lets every player recover their current table balance.

function refund() public
{
    assert(is_expired(), "game not expired");

    if(table_open) {
        table_open = false;
        for(const player of player_list) {
            player.active = false;
            player.waiting = false;
        }
    }
    assert(!table_open, "table is still open");

    const player = get_player(this.user);
    assert(player.stack > 0, "nothing to refund");

    const amount = player.stack;
    player.stack = 0;
    player.withdrawn = true;
    send(this.user, amount, currency, "poker_refund");
}

// Poker hand evaluation ----------------------------------------------------

function get_rank(hand) const public
{
    assert(size(hand) == 5, "hand must be 5 cards");

    const RANK_MAP = {
        "2": 0, "3": 1, "4": 2, "5": 3, "6": 4, "7": 5, "8": 6, "9": 7,
        "10": 8, "T": 8, "J": 9, "Q": 10, "K": 11, "A": 12
    };

    var values = [];
    var suit_map = {"H": 0, "D": 0, "C": 0, "S": 0};

    for(const card of hand) {
        const rank = RANK_MAP[card[0]];
        assert(rank != null, "invalid card number");
        assert(suit_map[card[1]] != null, "invalid card suit");
        push(values, rank);
        suit_map[card[1]]++;
    }
    values = reverse(sort(values));

    var unique_values = [];
    var previous = null;
    for(const value of values) {
        if(value != previous) {
            push(unique_values, value);
            previous = value;
        }
    }

    var straight = false;
    if(size(unique_values) == 5) {
        straight = (unique_values[0] - unique_values[4] == 4);
    }
    const low_straight = equals(unique_values, [12, 3, 2, 1, 0]);

    var flush = false;
    for(const suit of ["H", "D", "C", "S"]) {
        if(suit_map[suit] == 5) {
            flush = true;
        }
    }

    const counts = [];
    for(var i = 0; i <= 12; i++) {
        push(counts, 0);
    }
    for(const value of values) {
        counts[value]++;
    }

    const pairs = [];
    const singles = [];
    var threes = null;
    var fours = null;
    for(const value of unique_values) {
        if(counts[value] == 1) push(singles, value);
        if(counts[value] == 2) push(pairs, value);
        if(counts[value] == 3) threes = value;
        if(counts[value] == 4) fours = value;
    }

    if(flush && straight) return [10, [values[0]]];
    if(flush && low_straight) return [9, [3]];
    if(fours != null) return [8, [fours, singles[0]]];
    if(threes != null && size(pairs)) return [7, [threes, pairs[0]]];
    if(flush) return [6, values];
    if(straight) return [5, [values[0]]];
    if(low_straight) return [4, [3]];
    if(threes != null) return [3, [threes, singles[0], singles[1]]];
    if(size(pairs) == 2) return [2, [pairs[0], pairs[1], singles[0]]];
    if(size(pairs) == 1) return [1, [pairs[0], singles[0], singles[1], singles[2]]];
    return [0, values];
}

function compare_rank(left, right) const public
{
    assert(is_array(left) && size(left) == 2, "invalid hand rank");
    assert(is_array(right) && size(right) == 2, "invalid hand rank");

    const result = compare(left[0], right[0]);
    if(result == "EQ") {
        return compare(left[1], right[1]);
    }
    return result;
}

// Five board cards followed by two pocket cards. A pocket card colliding with
// the board is unavailable and cannot be selected.

function select_hand(board_, pocket, hand) const public
{
    assert(is_array(board_) && size(board_) == 5, "invalid board");
    assert(is_array(pocket) && size(pocket) == 2, "invalid pocket");
    assert(is_array(hand) && size(hand) == 5, "invalid hand");

    const all_cards = concat(board_, pocket);
    const selected = [];
    const index_map = {};

    for(const index of hand) {
        assert(is_uint(index) && index < 7, "invalid card index");
        assert(index_map[index] == null, "duplicate card index");
        index_map[index] = true;

        if(index >= 5) {
            for(const board_card of board_) {
                assert(!equals(all_cards[index], board_card),
                       "pocket card collides with board");
            }
        }
        push(selected, all_cards[index]);
    }
    return selected;
}

function get_card(seed) const public
{
    seed = uint(seed);
    const RANK_MAP = ["2", "3", "4", "5", "6", "7", "8", "9", "T", "J", "Q", "K", "A"];
    const SUIT_MAP = ["H", "D", "C", "S"];
    return [RANK_MAP[seed % 13], SUIT_MAP[(seed / 13) % 4]];
}

function deal_cards(seed_list) const public
{
    assert(is_array(seed_list), "seed list must be an array");

    var num_used = 0;
    const used_map = {};
    const cards = [];

    for(const seed of seed_list) {
        push(cards, draw_card(binary_hex(seed), used_map, num_used));
        num_used++;
    }
    return cards;
}

function draw_card(seed, used_map, num_used) const
{
    assert(is_binary(seed) && size(seed) == 32, "invalid seed");
    assert(num_used < 52, "deck is empty");

    const target = uint(seed) % (52 - num_used);
    var offset = 0;

    for(var index = 0; index < 52; index++) {
        if(used_map[index] == null) {
            if(offset == target) {
                used_map[index] = true;
                return get_card(index);
            }
            offset++;
        }
    }
    assert(false, "failed to draw card");
}
