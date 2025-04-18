---
title: General FAQ
description: Frequently asked questions about MMX blockchain.
---

### What is the MMX blockchain?

Technical introduction, read [whitepaper](../../../articles/general/mmx-whitepaper/).\
Light introduction, read [article](../../../articles/general/mmx-tldr/).

### How much MMX per block reward?

Usually around 0.5 MMX + transaction fees.

To get the full picture. Read about Dynamic Supply, Reward Control Loop and Minimum Block Reward in [whitepaper](../../../articles/general/mmx-whitepaper/).

### Can I farm MMX with Chia plots?

No.

MMX was written from scratch by Max. He also reworked plot format to make it orders of magnitude more resistant to compression techniques that hit Chia.

For an introduction to MMX plot format, read this [article](../../../articles/plotting/plot-format/).

### How much MMX for a given TB farm?

Formula (simplified):\
`(farmTB / netspacePB) * 4.32` = MMX (per day)

Example (250TB farm, Netspace 170PB):\
`(250 / 170) * 4.32` = ~6.35 MMX (per day)

Formula (expanded):\
`(farmTB / (netspacePB * 1000)) * (8640 blocks per day * 0.5 MMX)`

:::note[Note]
Find current Netspace in [MMXplorer](https://mmxplorer.com/dashboard). Calculations above gives average blocks won per day, multiplied by 0.5 MMX. That [block reward](#how-much-mmx-per-block-reward) can vary.
:::

### Is there a pre-farm?

No pre-farm, no pre-mine, no VC funded development.

What was were 100% open and communicated incentivized testnets. Starting with testnet8, to reward community providing a realistic environment through initial development.

List of incentivized testnets rewards are all public on GitHub ([testnet8/rewards.txt](https://github.com/madMAx43v3r/mmx-node/blob/master/data/testnet8/rewards.txt), [data](https://github.com/madMAx43v3r/mmx-node/blob/master/data)). Used to create mainnet genesis. No hidden pre-mine written into it, only community incentivized rewards.

As a sidenote, developers had less than 1% of netspace through testnets. All covered in [whitepaper](../../../articles/general/mmx-whitepaper/). Including testnet rewards, written into mainnet genesis. Over 99% of it to community farmers.

:::note[Note]
Definition of pre-farm will always be contentious. If comparing or arguing facts above. Also include context, volume, spread, who owns/controls. It should all be scaled and weighted, to get a fair picture.
:::

### Is there a dev fee?

There is a project funding fee.

It is a 1% fee on *transaction fees*, <ins>not block reward</ins>. To fund continued development of MMX. More nuances to how it works, read Project Funding section of [whitepaper](../../../articles/general/mmx-whitepaper/).

### Where can I find MMX branding assets?
Go to [mmx-assets](https://github.com/madMAx43v3r/mmx-assets) repository. Basic info, logo and assets folders:
- [`/logo/artwork/`](https://github.com/madMAx43v3r/mmx-assets/tree/master/logo/artwork/) _(artwork)_
- [`/logo/assets/`](https://github.com/madMAx43v3r/mmx-assets/tree/master/logo/assets/) _(misc)_
- [`/logo/raster/`](https://github.com/madMAx43v3r/mmx-assets/tree/master/logo/raster/) _(.png)_
- [`/logo/vector/`](https://github.com/madMAx43v3r/mmx-assets/tree/master/logo/vector/) _(.svg)_
