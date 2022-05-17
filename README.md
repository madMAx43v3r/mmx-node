# mmx-node

MMX is a blockchain written from scratch using Chia's Proof Of Space and a SHA256 VDF similar to Solana.

It's main features are as follows:
- High performance (1000 transactions per second or more)
- Custom high-level VM for smart contracts (similar to JavaScript)
- Variable supply (block reward scales with netspace, but also is capped by TX fees)
- Consistent block times (every 10 seconds a block is created)
- Native token and NFT support (atomic swaps with standard transactions)
- Non-interactive offers for off-chain / on-chain trading (like in Chia)
- Energy efficient Proof of Space (same as Chia)
- Standard ECDSA signatures for seamless integration (same as Bitcoin)

MMX is desiged to be a blockchain that can actually be used as a currency.

For example the variable supply will stabilize the price, one of the key properties of any currency.

Furthermore, thanks to an efficient implementation, it will provide low transaction fees even at high throughput.

A mainnet launch is planned in Q4 2022.
Currently we are running _testnet5_, so the coins farmed right now are _not worth anything_.

See `#mmx-news` and `#mmx-general` on discord: https://discord.gg/pQwkebKnPB

## Installation / Setup / Usage

Please take a look at the Wiki:

- [Installation](https://github.com/madMAx43v3r/mmx-node/wiki/Installation)
- [Getting-Started](https://github.com/madMAx43v3r/mmx-node/wiki/Getting-Started)
- [CLI-Commands](https://github.com/madMAx43v3r/mmx-node/wiki/CLI-Commands)
- [Frequently-Asked-Questions](https://github.com/madMAx43v3r/mmx-node/wiki/Frequently-Asked-Questions)

## WebGUI

To access the WebGUI go to: http://localhost:11380/gui/

It's only available on localhost since it allows spending from your wallet.
