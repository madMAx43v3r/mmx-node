import {equals, sort, reverse, compare} from "std";

// Dealer-settled, one-shot parallel poker table.
//
// A player deposits their complete stack in join(). One small blind is treated
// as committed by every joined player when the game is settled. The remainder
// of the stack stays escrowed but is not part of a pot unless covered by a
// signed cumulative bet.
//
// settle() arguments are arrays in player_list order unless noted otherwise:
//
// commitments[player]     [] on commit timeout, otherwise five 32-byte hashes:
//                         four board-seed commitments and one pocket commitment
// commit_signatures       null on timeout, otherwise one signature over the
//                         player's complete commitment bundle
// reveals[player][round]  four board seeds, with null for unavailable reveals
// betting[round][epoch]   action entries [player, action, cumulative_bet, sig]
// private_seeds[player]   pocket seed at showdown, otherwise null
// hands[player]           five indices into board + pocket, otherwise []
// timeouts                [player, phase, round, epoch]
//
// Timeout phases:
//   0 - commitment, 1 - board reveal, 2 - betting action, 3 - showdown
//
// Signed action types:
//   0 - check, 1 - bet / call / raise, 2 - fold
//
// Checkpoint phases and events:
//   phase 0 roster: event 0
//   phase 1 commit: event 1 valid, 2 timeout
//   phase 2 reveal: event 1 valid, 2 timeout, 3 already folded
//   phase 3 action: event 10 check, 11 bet, 12 fold,
//                   20 timeout-check, 21 timeout-fold,
//                   30 already folded, 31 all-in
//   phase 4 show:   event 1 valid, 2 timeout, 3 already folded

var currency;
var dealer;
var small_blind;
var min_stack;
var max_players;
var start_delay;
var game_timeout;
var rake_bps;

var state = 0;              // 0 - open / playing, 1 - settled, 2 - refunding
var start_height = null;
var refund_height = null;

var player_map = {};
var player_list = [];

var board = null;
var transcript_hash = null;
var dealer_rake = 0;
var rake_claimed = false;

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
    assert(uint(min_stack_blinds_) > 0, "invalid minimum stack");
    min_stack = uint(min_stack_blinds_) * small_blind;
    assert(min_stack / small_blind == uint(min_stack_blinds_), "minimum stack overflow");
    assert(max_players >= 2 && max_players <= 10, "invalid player limit");
    assert(start_delay > 0, "invalid start delay");
    assert(game_timeout > 0, "invalid game timeout");
    assert(rake_bps <= 10000, "invalid rake");
}

function join(name, public_key) public payable
{
    assert(state == 0, "table is closed");
    assert(!is_started(), "game already started");
    assert(this.user, "missing user");
    assert(this.user != dealer, "dealer cannot play");
    assert(this.deposit.currency == currency, "invalid currency");
    assert(this.deposit.amount >= min_stack, "stack below minimum");
    assert(size(player_list) < max_players, "table full");
    assert(player_map[this.user] == null, "already joined");
    assert(is_string(name) && size(name) > 0 && size(name) <= 24, "invalid name");

    public_key = binary_hex(public_key);
    assert(size(public_key) == 33, "invalid public key");
    assert(sha256(public_key) == this.user, "public key does not match user");

    const player = {
        name: name,
        address: this.user,
        public_key: public_key,
        stack: this.deposit.amount,
        bet: 0,
        folded: false,
        payout: 0,
        claimed: false,
    };

    player_map[this.user] = size(player_list);
    push(player_list, player);

    if(size(player_list) == 2) {
        start_height = this.height + start_delay;
        assert(start_height > this.height, "start height overflow");
        refund_height = start_height + game_timeout;
        assert(refund_height > start_height, "refund height overflow");
    }
}

// Before a second player joins, the first player may wait indefinitely or
// leave. Once the start countdown exists, all stacks remain locked until
// settlement or the emergency refund height.

function leave() public
{
    assert(state == 0, "table is closed");
    assert(start_height == null, "start countdown already active");
    assert(size(player_list) == 1, "cannot leave table");

    const player = get_player(this.user);
    send(this.user, player.stack, currency, "poker_refund");

    player_map = {};
    player_list = [];
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

// Returns [state, player_count, start_height, refund_height, dealer_rake,
//          rake_claimed, transcript_hash].

function get_table_status() const public
{
    return [state, size(player_list), start_height, refund_height,
            dealer_rake, rake_claimed, transcript_hash];
}

// Returns [currency, dealer, small_blind, minimum_stack, maximum_players,
//          start_delay, game_timeout, rake_basis_points].

function get_config() const public
{
    return [currency, dealer, small_blind, min_stack, max_players,
            start_delay, game_timeout, rake_bps];
}

// Returns [name, address, public_key, stack].

function get_player_info(index) const public
{
    index = uint(index);
    assert(index < size(player_list), "invalid player index");
    const player = player_list[index];
    return [player.name, player.address, player.public_key, player.stack];
}

// Returns [stack, cumulative_bet, folded, payout, claimed].

function get_player_status(address) const public
{
    const player = get_player(bech32(address));
    return [player.stack, player.bet, bool(player.folded),
            player.payout, bool(player.claimed)];
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

    var checkpoint = sha256(concat(
        "MMX_PARALLEL_POKER_START_V1/",
        string_bech32(this.address), "/",
        string(start_height)
    ));
    for(const player of player_list) {
        checkpoint = checkpoint_step(checkpoint, 0, 0, 0,
                                     player.address, 0, player.stack);
    }
    return checkpoint;
}

function settle(commitments, commit_signatures, reveals, betting,
                private_seeds, hands, timeouts) public
{
    assert(state == 0, "table is closed");
    assert(this.user == dealer, "only dealer can settle");
    assert(is_started(), "game not started");
    assert(!is_expired(), "game expired");

    const count = size(player_list);
    assert(count >= 2, "not enough players");
    assert(is_array(commitments) && size(commitments) == count,
           "invalid commitments");
    assert(is_array(commit_signatures) && size(commit_signatures) == count,
           "invalid commitment signatures");
    assert(is_array(reveals) && size(reveals) == count, "invalid reveals");
    assert(is_array(betting) && size(betting) == 4, "invalid betting transcript");
    assert(is_array(private_seeds) && size(private_seeds) == count,
           "invalid private seeds");
    assert(is_array(hands) && size(hands) == count, "invalid hands");
    assert(is_array(timeouts), "invalid timeouts");

    const timeout_used = validate_timeouts(timeouts, count);
    const commit_values = [];
    const ranks = [];

    for(var i = 0; i < count; i++) {
        const player = player_list[i];
        player.bet = small_blind;
        player.folded = false;
        player.payout = 0;
        player.claimed = false;
        push(ranks, null);

        assert(is_array(reveals[i]) && size(reveals[i]) == 4,
               "invalid player reveals");
        assert(is_array(hands[i]), "invalid player hand");
    }

    var checkpoint = get_start_checkpoint();

    // Every valid participant signs all five commitments as one bundle before
    // any reveal is released. A commit timeout immediately folds the player.
    for(var i = 0; i < count; i++) {
        const player = player_list[i];
        const values = commitments[i];
        assert(is_array(values), "invalid player commitments");

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
    }

    const sources = [];
    var rounds_processed = 0;

    for(var round = 0; round < 4; round++) {
        if(get_num_active() > 1) {

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
    if(get_num_active() > 1) {
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
            const private_seed_ = private_seeds[i];

            if(!player.folded) {
                if(private_seed_ != null) {
                    const private_seed = binary_hex(private_seed_);
                    assert(size(private_seed) == 32,
                           "private seed must be 32 bytes");
                    assert(get_seed_commit(player.address, 4, private_seed)
                           == commit_values[i][4], "invalid private seed");
                    assert(size(hands[i]) == 5, "invalid hand");

                    const source = sha256(concat(global_seed, private_seed));
                    const pocket = deal_cards([
                        sha256(concat(binary_hex("A1"), source)),
                        sha256(concat(binary_hex("A2"), source))
                    ]);
                    ranks[i] = get_rank(select_hand(board, pocket, hands[i]));
                    checkpoint = checkpoint_step(checkpoint, 4, 4, 0,
                                                 player.address, 1, player.bet,
                                                 sha256(private_seed));
                } else {
                    assert(size(hands[i]) == 0, "hand without private seed");
                    assert(use_timeout(timeouts, timeout_used, i, 3, 4, 0),
                           "missing showdown timeout");
                    player.folded = true;
                    checkpoint = checkpoint_step(checkpoint, 4, 4, 0,
                                                 player.address, 2, player.bet);
                }
            } else {
                assert(private_seed_ == null, "show from folded player");
                assert(size(hands[i]) == 0, "hand from folded player");
                checkpoint = checkpoint_step(checkpoint, 4, 4, 0,
                                             player.address, 3, player.bet);
            }
        }
    } else {
        for(var i = 0; i < count; i++) {
            assert(private_seeds[i] == null, "unexpected private seed");
            assert(size(hands[i]) == 0, "unexpected hand");
        }
    }

    assert_all_timeouts_used(timeout_used);

    transcript_hash = checkpoint;
    allocate_payouts(ranks);
    state = 1;
}

function process_betting_round(round, epochs, checkpoint,
                               timeouts, timeout_used)
{
    assert(is_array(epochs), "invalid betting round");

    if(get_num_active() <= 1 || get_num_actors() == 0) {
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
            assert(is_uint(entry[0]) && entry[0] < size(player_list),
                   "invalid action player");
            assert(is_uint(entry[1]) && entry[1] < 3, "invalid action type");
            assert(is_uint(entry[2]), "invalid action amount");
            push(entry_used, false);
        }

        const epoch_checkpoint = checkpoint;
        const target = get_current_bet();

        for(var i = 0; i < size(player_list); i++) {
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
            if(!player.folded && player.bet < player.stack
               && player.bet < next_target) {
                done = false;
            }
        }
        if(get_num_active() <= 1) {
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
        for(var i = 0; i < size(player_list); i++) {
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
        if(!player.folded && player.bet > amount) {
            amount = player.bet;
        }
    }
    return amount;
}

function get_num_active() const public
{
    var count = 0;
    for(const player of player_list) {
        if(!player.folded) {
            count++;
        }
    }
    return count;
}

function get_num_actors() const
{
    var count = 0;
    for(const player of player_list) {
        if(!player.folded && player.bet < player.stack) {
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

// Initializes every claim with unused stack, then distributes standard main
// and side-pot layers. A one-player layer is uncalled and returned. A layer
// with no surviving eligible player is returned contributor-by-contributor.

function allocate_payouts(ranks)
{
    const layer_amounts = [];
    const layer_winners = [];
    var matched_total = 0;
    var total_stack = 0;

    for(const player of player_list) {
        assert(player.bet >= small_blind && player.bet <= player.stack,
               "invalid final bet");
        player.payout = player.stack - player.bet;
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
                    if(!player.folded) {
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

    dealer_rake = matched_total * rake_bps / 10000;

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

function claim() public
{
    assert(state == 1, "game not settled");
    const player = get_player(this.user);
    assert(!player.claimed, "already claimed");

    if(player.payout > 0) {
        send(this.user, player.payout, currency, "poker_win");
    }
    player.claimed = true;
}

function claim_rake() public
{
    assert(state == 1, "game not settled");
    assert(this.user == dealer, "only dealer can claim rake");
    assert(!rake_claimed, "rake already claimed");

    if(dealer_rake > 0) {
        send(dealer, dealer_rake, currency, "poker_rake");
    }
    rake_claimed = true;
}

// If settlement never arrives, every player recovers the complete stack. The
// implicit blind is only applied by settle(), so it is also fully refunded.

function refund() public
{
    assert(state != 1, "game already settled");
    assert(is_expired(), "game not expired");

    if(state == 0) {
        state = 2;
    }
    const player = get_player(this.user);
    assert(!player.claimed, "already refunded");

    send(this.user, player.stack, currency, "poker_refund");
    player.claimed = true;
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
