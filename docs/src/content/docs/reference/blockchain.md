---
title: Blockchain
description: Introduction to the MMX Blockchain.
---

A blockchain written from scratch, doing most things differently.
1. MMX is designed to be a blockchain that can be used as an actual currency
2. Variable supply will stabilize the price, key property of any currency
3. Efficient implementation provides low transaction fees, at high throughput

:::note[Note]
In-depth technical, read [whitepaper](../../../articles/general/mmx-whitepaper/). Light introduction, read [article](../../../articles/general/mmx-tldr/).
:::

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

Mainnet started on 17th Jan 2025.

See [`#mmx-news`](https://discord.com/channels/852982773963161650/926566475249106974) and [`#mmx-general`](https://discord.com/channels/852982773963161650/925017012235817042) on [Discord](https://discord.gg/BswFhNkMzY).
