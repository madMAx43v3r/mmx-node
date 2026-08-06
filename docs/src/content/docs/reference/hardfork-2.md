---
title: Hardfork 2
description: Consensus and VM changes activated by MMX hardfork 2.
---

Hardfork 2 activates at the block height configured by `ChainParams.hardfork2_height`. On mainnet this is block
**6,000,000**, projected around 12 December 2026 (UTC). This is a block height, not a VDF height.

Nodes participating at or beyond the activation height need hardfork-2-compatible software. The new rules change block
header hashing, challenge calculation, and smart contract argument handling.

## Summary

Hardfork 2 introduces:

- A cumulative proof chain that is committed to each block header.
- Proof-chain infusion into challenge and space-fork calculations.
- A maximum space-fork interval, after which an infusion is forced.
- Correct map and object key handling when values cross a contract boundary.
- The `SUPPORT_HARDFORK2` block support flag (`0x2`).

## Proof chain

The block header has a new `proof_chain` field. Let `H` be `hardfork2_height`, `P[h]` the block's `proof_hash`, and
`C[h]` its `proof_chain`.

In the formulas below, `hash()` is SHA-256 and `||` denotes raw byte concatenation.

At the activation block:

```text
C[H] = P[H]
```

For every later block:

```text
C[h] = hash("proof_chain" || C[h - 1] || P[h])
```

The activation block is the anchor: its previous block is below the hardfork height, so its proof chain is initialized
to its own proof hash. Starting with the next block, every value commits to the previous proof chain and the current
proof hash.

For blocks at height `H` and later:

- `proof_chain` is included in the block header hash.
- The node verifies `proof_chain` against the formula above.
- Challenge infusion uses `proof_chain` instead of the current block's `proof_hash`.

For blocks below `H`, `proof_chain` is not included in the block header hash and the previous consensus behavior is
preserved.

## Challenge and space-fork calculation

The candidate challenge is first advanced once for each VDF point in the block:

```text
candidate = previous_challenge
repeat block.vdf_count times:
    candidate = hash("next_challenge" || candidate)
```

Below hardfork 2, the infusion value is the block's `proof_hash`. Starting at hardfork 2, it is the block's
`proof_chain`.

A normal space fork occurs when:

```text
hash("proof_infusion_check" || candidate || infusion_value) mod challenge_interval == 0
```

When a space fork occurs, the final challenge is:

```text
hash("challenge_infusion" || candidate || infusion_value)
```

Otherwise, the candidate becomes the next challenge without an infusion.

### Forced infusion

Hardfork 2 also bounds the time between space forks. An infusion is forced when:

```text
previous.space_fork_len + block.vdf_count > max_space_fork_len * challenge_interval
```

The comparison is strictly greater than. On mainnet, `max_space_fork_len` is `8` and `challenge_interval` is `256`,
so an infusion is forced when the accumulated length exceeds 2,048 VDF points.

A normal or forced space fork resets `space_fork_len` to the current block's `vdf_count` and updates space difficulty
using the existing space-fork adjustment rules.

## Smart contract map arguments

MMX VM map keys are address-backed. Before hardfork 2, transaction arguments and remote-call arguments were copied
into the callee engine before the callee binary constants were loaded. String keys in map or object arguments could
therefore receive different internal addresses from identical keys used by the contract. This could make fields appear
missing or prevent values from being persisted under the expected global map entries.

Starting at hardfork 2, contract execution uses this order:

1. Load the callee binary and initialize its constant keys.
2. Materialize the deposit and call arguments in the callee engine.
3. Execute the method.

The new order applies to contract deployment, direct transaction calls, and nested remote calls. Below the activation
height, nodes retain the original setup-before-load order to preserve consensus compatibility.

The `mmx_compile` test harness always uses the corrected load-before-arguments order; it does not emulate the pre-fork
behavior.

## Block format and support flag

Hardfork-2-capable nodes set `BlockHeader.SUPPORT_HARDFORK2` (`0x2`) in `support_flags` when producing blocks.
Activation is determined by `hardfork2_height`; the support flag does not replace the height gate.

Block header hashing now requires chain parameters so it can decide whether `proof_chain` is part of the committed
header. Accordingly, the internal `BlockHeader::calc_hash()`, `BlockHeader::is_valid()`, and `Block::is_valid()` APIs
receive `ChainParams`.

## Compatibility

The height gates preserve the original block hash, challenge calculation, and VM argument order for historical blocks.
No historical block rewrite is required.

At the activation height, all validating and block-producing nodes must apply the new rules. Existing contracts do not
need to be redeployed, but calls that pass maps or objects across a contract boundary will begin using the corrected key
semantics.
