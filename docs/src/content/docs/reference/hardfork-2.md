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
- Correct 256-bit multiplication overflow detection in the VM.
- Deterministic ordering for implicit multi-currency deployment outputs.
- Canonical type tags for variants in transaction hashes.
- Canonical transaction solutions and stricter multi-signature validation.
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

## Multiplication overflow detection

Before hardfork 2, the VM detected unsigned 256-bit multiplication overflow by comparing the wrapped result with both
operands. This misses some overflows, including `(2^255 + 1) * 2`, whose wrapped result is `2`.

Starting at hardfork 2, multiplication instructions with overflow checking enabled reject the operation when the left
operand is non-zero and the right operand exceeds `MAX_UINT256 / left`. Multiplication without overflow checking
continues to wrap modulo `2^256`. The legacy comparison remains active below the hardfork height so historical contract
execution is unchanged. The `mmx_compile` execution harness uses the corrected behavior.

## Implicit deployment output ordering

A deployment can leave input balances in multiple currencies that are implicitly deposited into the new contract.
Before hardfork 2, these outputs were emitted by iterating an unordered map, so their order could depend on the standard
library and platform.

Starting at hardfork 2, leftover currencies are ordered by address before their implicit outputs are appended. Below the
hardfork height, the original unordered iteration is retained for historical transaction-result compatibility.

## Transaction hash version 1

Before hardfork 2, boolean, signed-integer, and unsigned-integer variants were written to transaction hashes without a
type tag. Adjacent argument values could therefore have the same byte encoding despite having different VM semantics.
For example, `[true, uint64(0)]` and `[uint64(1), false]` produced the same operation hash.

Starting at hardfork 2, transactions must use version 1. Version-1 hashing prefixes boolean, signed 64-bit integer,
and unsigned 64-bit integer variants with distinct type tags. The version is propagated recursively through arrays and
objects and into operation and deployment hashes. Transactions below the activation height must use version 0, which
preserves all historical transaction IDs.

Wallets select the transaction version for the next block height. A version-0 transaction that has not been included
before activation is no longer eligible for inclusion after hardfork 2.

## Canonical transaction solutions

Starting with transaction version 1, every top-level solution must be referenced by the sender, an input, or an
authorized contract call. Top-level solutions must also have unique hashes. This prevents an otherwise unused or
duplicate signature from being added while changing only non-cryptographic solution indexes. A single solution can
still authorize multiple uses by sharing its index.

For a version-1 multi-signature solution, every map entry must belong to an owner of the multi-signature contract and
must contain a public-key signature for that owner. Entries from non-owners and entries of other solution types are
rejected instead of ignored, and the solution's required-signature count must match the contract. Additional valid
signatures from owners remain allowed. Version-0 transactions retain the legacy behavior for historical validation.

## Block format and support flag

Hardfork-2-capable nodes set `BlockHeader.SUPPORT_HARDFORK2` (`0x2`) in `support_flags` when producing blocks.
Activation is determined by `hardfork2_height`; the support flag does not replace the height gate.

Block header hashing now requires chain parameters so it can decide whether `proof_chain` is part of the committed
header. Accordingly, the internal `BlockHeader::calc_hash()`, `BlockHeader::is_valid()`, `Block::finalize()`, and
`Block::is_valid()` APIs receive `ChainParams`.

## Compatibility

The height gates preserve the original block hash, challenge calculation, and VM argument order for historical blocks.
No historical block rewrite is required.

At the activation height, all validating and block-producing nodes must apply the new rules. Existing contracts do not
need to be redeployed, but calls that pass maps or objects across a contract boundary will begin using the corrected key
semantics.
