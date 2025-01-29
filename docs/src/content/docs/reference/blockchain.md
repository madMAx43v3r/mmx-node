---
title: Blockchain
description: Introduction to the MMX Blockchain.
---
A blockchain written from scratch, doing most things differently.

Design
- Variable token supply governed by consensus (1 block = 1 vote)
- High throughput L1 with consistent block interval (500 TPS, 10 sec)
- Novel Smart Contract VM for ease of development and high performance
- Energy efficient Proof of Space
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

MMX is designed to be a blockchain that can be used as an actual currency.

Variable supply will stabilize the price, one of the key properties of any currency.

Thanks to an efficient implementation, it will provide low transaction fees, even at high throughput.

Mainnet started on 17th Jan 2025.

See `#mmx-news` and `#mmx-general` on discord: https://discord.gg/BswFhNkMzY