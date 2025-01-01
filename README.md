# mmx-node

_“ MMX &ndash; fast, low cost, feature rich, decentralized &ndash; with tokenomics of an actual currency ”_

A blockchain written from scratch, doing most things differently.

Design
- Variable token supply governed by consensus (1 block = 1 vote)
- High throughput L1 with consistent block interval (500 TPS, 10 sec)
- Novel Smart Contract VM for ease of development and high performance
- Energy efficient Proof of Space, with optional Proof of Stake (limited to Tx fees)
- Block reward is adjusted to stabilize price, a key property of any currency
- Minimum transaction fee to allow large block size without spam
- Limited external library usage to keep codebase clean and secure
- No pre-mine, no ICO, no investors
- Account based model

Elements
- High performance C++ code (can handle over 1000 TPS easily)
  - Transactions are executed in parallel when possible
- Custom high-level VM for Smart Contracts
  - Native support for variants, arrays, maps, and objects
  - Unified memory with automatic persistence and state updates
  - A restricted subset of JavaScript is compiled into bytecode
  - Average of two machine instructions per line of code
- Native token support (no "approvals" needed, NFT = 1 mojo)
- Smart contract offer based trading (fixed price, OTC)
- Liquidity pool swap based trading (AMM, multi-fee tiers, similar to Uniswap v3)
- ECDSA signatures for seamless integration (same as Bitcoin)
- Custom blockchain database engine (much faster than RocksDB or LevelDB overall)
- Adaptive SHA256 VDF to govern block interval
- Feature rich Node with built-in Block Explorer, Wallet, Market, Swap, RPC, etc

Roadmap

| Release | Date | Description |
| :--- | :--- | :--- |
| testnet7 | Sep 2022 | Finished. <sup>[1]</sup> |
| testnet8 | Oct 2022 | Finished. Incentivized testnet, height 25k-425k. <sup>[2]</sup> |
| testnet9 | Dec 2022 | Finished. Incentivized testnet, height 25k-1220k. <sup>[2]</sup> |
| testnet10 | Apr 2023 | Finished. Incentivized testnet, height 40k-3200k. <sup>[2]</sup> |
| testnet11 | May 2024 | Finished. New plot format, compression resistant. |
| testnet12 | June 2024 | Active. Incentivized testnet, height 25k-tbd. <sup>[2]</sup> |
| mainnet | _tbd_ | Planned Q4 2024. |

_<sup>[1]</sup> Coins farmed on testnets are not worth anything, now or later._\
_<sup>[2]</sup> A fixed reward of 0.5 MMX per block win on incentivized testnets will be given on mainnet genesis._

See `#mmx-news` and `#mmx-general` on Discord: https://discord.gg/BswFhNkMzY

## Installation / Setup / Usage

Please take a look at the Wiki:

- [Installation](https://github.com/madMAx43v3r/mmx-node/wiki/Installation)
- [Getting Started](https://github.com/madMAx43v3r/mmx-node/wiki/Getting-Started)
- [CLI Commands](https://github.com/madMAx43v3r/mmx-node/wiki/CLI-Commands)
- [Frequently Asked Questions](https://github.com/madMAx43v3r/mmx-node/wiki/Frequently-Asked-Questions)

## WebGUI

To access WebGUI, go to: http://localhost:11380/gui/

It's only available on localhost by default. \
The login password is auto-generated at first launch, located in `mmx-node/PASSWD` file (`$MMX_HOME/PASSWD`).

## Release Notes

### Testnet9

- New `Swap` contract, to trade via liquidity pools (similar to _UniSwap_, anyone can provide liquidity and earn fees)
- Improved `Offer` contract: partial trades and extending offers are now possible
- Re-designed `Virtual Plots`: withdrawal is now possible with a 10% fee (no lock duration, fee is burned), also there is no more expiration
- Network traffic is now compressed via `deflate` level 1
- Second best farmer will now create an empty block (just to get the reward), in case the first farmer fails or is too slow

### Testnet10

- Decentralized Timelord rewards (fixed 0.0025 MMX per block, paid by TX fees when possible (but never more than 1/8))
- New AMM Swap with multi-fee tier system (you can chose between 4 fee levels for your liquidity)
- Account activation TX fee (0.01 MMX when transferring to a new address with zero balance)
- Increased min block reward to 0.5 MMX
- Full transition to smart contracts with compiled JS like code
- Harvester improvements for compressed plots (graceful degradation)
- New dev fee system, based only on TX fees, with a fixed amount plus 1% (but never more than 1/4 of TX fees)
- Plot filter has been decreased to 256 now, as it is planned for mainnet
- Many improvements to the code base

### Testnet11

- New plot format:
  - k29 to k32 max
  - 9 tables
  - very low compression only
  - CPU farming only
  - SSD and HDD plot types
  - Old format no longer valid
- Transaction output memo support
- Block reward voting support
- Swap algorithm improved
- Swap fee levels changed to: 0.05%, 0.25%, 1% and 5%
- Using zstd compression for network traffic
- NFT plot support (testing only)

### Testnet12

- Fixed Virtual Plots, they now win blocks at the expected rate. On TN11 it was ~20 times less.
- Fixed block reward formula, average tx fee is subtracted from minimum reward again.

### Mainnet-RC

- New VDF scheme:
  - Single VDF stream with infused Timelord reward
  - No longer possible to steal or disable Timelord reward
  - Proof challenges are now deterministic, apart from random infusions every 256 blocks on average
  - If a proof's hash passes a filter, it will change future challenges and update difficutly
  - Timelord rewards are now paid out every 50 blocks in bulk to fastest TL
- VDF segment count is now dynamic, depending on TL speed:
  - This means VDF verify time wont increase unless the GPU is at it's compute limit
  - Previously VDF verify time would increase even if the GPU is not maxed out, due to fixed parallel work
  - CPU verify is unchanged
- All proofs found are now included in blocks
  - Yields accurate netspace estimation
  - Supports extra security (see below)
- Recent blocks are further secured via a new voting scheme:
  - Up to 21 farmers who found a lesser proof (didn't make a block) recently can act as a validator
  - These validators vote on the first block received per height
  - Only blocks made with the best known proof are voted for
  - This prevents reverting recent blocks via double signing in most cases
  - Previously it was always possible to replace the current peak via double signing, this is now impossible
  - Previously if a farmer got lucky to find multiple blocks in a row:
    - He could also replace them at will until another farmer found the next block
    - This is now also impossible, unless the farmer has close to 50% or more netspace
- 50% TX fee burn (to avoid farmer spam attack + allow supply contraction)
  - Project fee is taken from burned amount
- Virtual Plots have been removed due to an attack vector
  - Together with the un-bounded block reward voting it was possible to generate a heavier chain with minimal real netspace
- Improved block reward voting: Majority vote out of 8640 blocks wins, 1% change up/down per day, >50% participation required.
- Offer contract now supports price update (at most every 1080 blocks)
- Pooling support + Reference Pool implementation
- Improved difficulty adjustment algorithm (targets 4 proofs per block)
- VM improvements to reduce transaction costs
- Send amount can now be up to 128-bit (was limited to 64-bit before)
- Mint amount can now be up to 80-bit (was limited to 64-bit before)
- Added block timestamps
- Smart Contract unit test framework









