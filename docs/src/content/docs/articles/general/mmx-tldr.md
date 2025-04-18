---
title: MMX TLDR;
description: What is the MMX blockchain ?
prev: false
next: false

articlePublished: 2025-03-14
articleUpdated: 2025-04-18

authorName: voidxno
authorPicture: https://avatars.githubusercontent.com/u/53347890?s=200
authorURL: https://github.com/voidxno
---

What is the [MMX blockchain](https://mmx.network/) ?

_&#8220;&nbsp;MMX &ndash; fast, low cost, feature rich, decentralized &ndash; with tokenomics of an actual currency&nbsp;&#8221;_

:::note
For more technical information, read [whitepaper](../../../articles/general/mmx-whitepaper/).
:::

## TLDR;

- A **native L1 blockchain**, truly independent and decentralized
  - Mainnet **launched** 17jan2025
  - Designed as a real stable currency, cryptos original goal
- **500 TPS**, 10 sec block time, consistent
  - **Written from scratch** by [Max](https://github.com/madMAx43v3r), in high performance C++ code
  - Easily handle 1000 TPS in future
  - Nodes on regular home hardware and internet connections
- Security by **Proof of Space and Time** (**PoST**)
  - **More energy efficient** than Proof of Work (PoW)
  - **More decentralized** than Proof of Stake (PoS)
  - High-power once, low-power decentralized thereafter
- **Feature rich** compatible **L1 on-chain logic**
  - Enhancing proven logic, and creating new
  - High-level VM for Smart Contracts
  - Smart Contract offer trading, fixed price, OTC
  - Liquidity pool swap trading, AMM, multi-fee tiers
  - Native token support, MMX one of them
  - Account model ECDSA signatures, seamless integration
- **Feature rich** node:
  - Built-in Block Explorer, Wallet, Market, Swap, RPC, and more

## Compare

Native L1 blockchain logic, without any L2 scaling.

| | MMX | Bitcoin | Ethereum | Chia |
| :--- | :--- | :--- | :--- | :--- |
| Consensus | [PoST](## "Proof of Space and Time") | [PoW](## "Proof of Work") | [PoS](## "Proof of Stake") | [PoST](## "Proof of Space and Time") |
| [TPS](## "Transactions per Second, real simplest-TX capacity, ignoring TX-fees") (capacity) _<sup>[1]</sup>_ | 500 _<sup>[(i)](## "250M cost limit / 50K cost simplest-TX / blocktime = ~500")</sup>_ | 12 _<sup>[(i)](## "1 MB / 140 bytes SegWit-TX / blocktime = ~12")</sup>_ | 119 _<sup>[(i)](## "30M gas limit / 21K gas simplest-TX / blocktime = ~119")</sup>_ | 35 _<sup>[(i)](## "Documented as 20-40 TPS, real-life stress tests at 35 = ~35")</sup>_ |
| [Blocktime/TX](## "Average time it takes to create a new block with TX-es") (average) _<sup>[1]</sup>_ | ~10 sec _<sup>[(i)](## "Blocktime 10 sec, ~1.5% non-TX blocks, average TX blocktime = ~10 sec")</sup>_ | ~600 sec _<sup>[(i)](## "Average blocktime statistics = ~600 sec (~10 min)")</sup>_ | ~12 sec _<sup>[(i)](## "Average blocktime statistics = ~12 sec")</sup>_ | ~56 sec _<sup>[(i)](## "Blocktime 18.75 sec, 2/3 non-TX blocks, average TX blocktime = ~56 sec")</sup>_ |
| Smart Contracts (VM) | Yes | No | Yes | Yes |
| Feature Rich (L1) | Yes | No | Basic | Yes |

_<sup>[1]</sup> Look [footnotes](#footnotes) for more details._

Decentralization, or not:

- **Ethereum**: Less decentralized. Staking concentrated, staking requirement for validators.
- **XRP**: Not truly decentralized. Centralized approved validators, token distribution.
- **Solana**: Not truly decentralized. Staking concentrated, token distribution, validator requirements.

If needed, **MMX** is a solid foundation for any L2 solution.

## History

- **Jan2025**: Mainnet live 17jan2025, **MMX**
- **Oct2022**: Testnets 8-12, first incentivized
- **Nov2021**: Testnets 1-7, development [started](https://github.com/madMAx43v3r/mmx-node/commits/master/?since=2021-11-25&until=2021-11-25), **by** [**Max**](https://github.com/madMAx43v3r)

## Tokenomics

- Detailed info in [whitepaper](../../../articles/general/mmx-whitepaper/)
- No pre-mine, no pre-farm, public incentivized testnets
- Transparently written on mainnet genesis, [records](https://github.com/madMAx43v3r/mmx-node/tree/master/data) on GitHub
- Majority to farmers, +99%, not developers, with &lt;1% of netspace

## Resources

- [Homepage](https://mmx.network/)
- [Documentation](https://docs.mmx.network/)
- [GitHub](https://github.com/madMAx43v3r/mmx-node)
- [Explorer](https://explore.mmx.network/)
- [X Feed](https://x.com/MMX_Network_)
- [Discord](https://discord.gg/BswFhNkMzY)
- [MMXplorer](https://mmxplorer.com/)

## Footnotes

<details><summary><b>TPS (capacity)</b></summary>

\
**Premise**: Capacity of blockchain in Transactions per Second, with real simplest-TX size, ignoring TX-fees.

- **MMX**: `250M cost limit / 50K cost simplest-TX / blocktime` = `~500`\
Node coded extremely performant in C++ to handle all aspects of blockchain logic on regular home hardware. Real-life stress tests, on regular user nodes, have handled 2000 TX-es (200 TPS) per block without problems. Block parameters at that time limited it to 2000 TX-es. Now set to potentially 5000 TX-es (500 TPS). Locally node will be able to handle it, and can easily scale to 10000 TX-es (1000 TPS) in future.\
Bottleneck is bandwidth of home internet connections, and latency of internet in general. A limitation MMX works the most out of, while still being truly decentralized. We know 200 TPS works, with 500 TPS potential. Future home internet improvements could raise that number (1000 TPS). All on-chain L1, before any L2 scaling.

- **Bitcoin**: `1 MB / 140 bytes SegWit-TX / blocktime` = `~12`\
Bitcoin is usually referenced with a maximum TPS around 7. Could increase if community had agreed on several design improvements. As of now, with current SegWit-TX structure. Potentially 12 TPS with only SegWit-TX in block.

- **Ethereum**: `30M gas limit / 21K gas simplest-TX / blocktime` = `~119`\
Ethereum is usually listed with anything from 15-50 maximum TPS. Real-life statistics have it working at an average of 15 TPS, with variable complexity of TX-es. The calculation of 119 TPS might not look very realistic. But we are talking capacity potential, ignoring TX-fees. Filling a block with about 1428 simplest-TX, results in a potential TPS of 119. If the blockchain logic and nodes are optimized enough to handle such a scenario, unknown.

- **Chia**: `Documented as 20-40 TPS, real-life stress tests at 35` = `~35`\
Not easy to get hard numbers to calculate Chia's maximum TPS potential. It is documented to be in the 20-40 range. A real-life stress test, with simplest-TX-es, have given about 35 TPS.

_NOTE: An 100% apples to apples comparison is not possible without overly complicating it. Each blockchain has unique properties, real-life behavior, and limitations. This is an attempt to estimate a realistic capacity potential. As fair as possible, in a simple table._

</details>

<details><summary><b>Blocktime/TX (average)</b></summary>

\
**Premise**: Average time it takes to create a new block with TX-es, given frequency of non-TX blocks.

- **MMX**: `Blocktime 10 sec, ~1.5% non-TX blocks, average TX blocktime` = `~10 sec`
- **Bitcoin**: `Average blocktime statistics` = `~600 sec` (`~10 min`)
- **Ethereum**: `Average blocktime statistics` = `~12 sec`
- **Chia**: `Blocktime 18.75 sec, 2/3 non-TX blocks, average TX blocktime` = `~56 sec`

</details>
