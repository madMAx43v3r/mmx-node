# mmx-node

MMX is a blockchain written from scratch, using proven logic from Chia's Proof of Space and improved SHA256 VDF similar to Solana.

Design
- High transaction capacity, with low and consistent block times
- Energy efficient Proof of Space, future transition to virtual storage
- Rich compatible feature set, built into base blockchain
- Balanced hardware requirements, with nodes in home networks
- Native MMX is designed with the properties and usage of an actual currency
- Variable supply stabilizes the price, a key property of any currency
- Efficient implementation provides low transaction fees, at high throughput

Features
- High performance code (1000 transactions per second or more)
- Custom high-level VM for smart contracts (similar to JavaScript)
- Variable supply (block reward scales with netspace, but also capped by TX fees)
- Consistent block times (created every 10 seconds, governed by VDF)
- Native token and NFT support (atomic swaps with standard transactions)
- Smart contract offers for on-chain trading (bids locked into contract)
- Energy efficient Proof of Space (same as Chia)
- Standard ECDSA signatures for seamless integration (same as Bitcoin)

Roadmap

| Release | Planned | Description |
| :--- | :--- | :--- |
| testnet7 | Sep 2022 | Currently active. <sup>[1]</sup> |
| testnet8 | Oct 2022 | First incentivized testnet. <sup>[2]</sup> |
| testnetX | _tbd_ | As needed. <sup>[2]</sup> |
| mainnet | _tbd_ | Expected Q1 2023. |

_<sup>[1]</sup> Coins farmed on testnet7 and earlier are not worth anything, now or later._\
_<sup>[2]</sup> Incentivized testnet coins, from block rewards only, will carry over to mainnet._

See `#mmx-news` and `#mmx-general` on Discord: https://discord.gg/BswFhNkMzY

## Installation / Setup / Usage

Please take a look at the Wiki:

- [Installation](https://github.com/madMAx43v3r/mmx-node/wiki/Installation)
- [Getting Started](https://github.com/madMAx43v3r/mmx-node/wiki/Getting-Started)
- [CLI Commands](https://github.com/madMAx43v3r/mmx-node/wiki/CLI-Commands)
- [Frequently Asked Questions](https://github.com/madMAx43v3r/mmx-node/wiki/Frequently-Asked-Questions)

## WebGUI

To access WebGUI, go to: http://localhost:11380/gui/

Only available on localhost, because of _full access_ to your wallet.

Login password is auto-generated at first launch, located in $MMX_HOME/PASSWD file.
