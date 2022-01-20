# mmx-node

MMX is a blockchain written from scratch using Chia's Proof Of Space and a SHA256 VDF similar to Solana.

It's main features are as follows:
- High performance (1000 transactions per second or more)
- Variable supply (block reward scales with netspace, but also is capped by TX fees)
- Consistent block times (every 10 seconds a block is created)
- Native token support (swaps are possible with standard transactions)
- Energy saving Proof of Space (same as Chia)
- Standard ECDSA signatures for seamless integration (same as Bitcoin)

MMX is desiged to be a blockchain that can actually be used as a currency.

For example the variable supply will stabilize the price, one of the key properties of any currency.

Furthermore, thanks to an efficient implementation and the avoidance of using a virtual machine, it will provide low transaction fees even at high throughput.

Tokens can either be traditionally issued by the owner, or they can be owner-less and created by staking another token over time, in a decentralized manner governed by the blockchain.

In the future it is planned that anybody can create their own token on MMX using a simple web interface.

The first application for MMX will be a hybrid decentralized exchange where users can trade MMX and tokens, as well as possibly a decentralized storage platform.

The variable reward function is as follows: \
`reward = max(difficulty * reward_factor, min_reward)` \
`final_reward = max(1 - TX_fees / (2 * reward), 0) * reward + TX_fees` \
Where `min_reward` is fixed at launch and `reward_factor` will decrease by 15% per year to compensate for reduced farming cost.

A mainnet launch is planned in ~6 months or so.
Currently we are running _testnet4_, so the coins farmed right now are _not worth anything_.

See `#mmx-news` and `#mmx-general` on discord: https://discord.gg/pQwkebKnPB

## Installation / Setup / Usage

Please take a look at the Wiki:

https://github.com/madMAx43v3r/mmx-node/wiki/Installation
https://github.com/madMAx43v3r/mmx-node/wiki/Getting-Started
https://github.com/madMAx43v3r/mmx-node/wiki/CLI-Commands
https://github.com/madMAx43v3r/mmx-node/wiki/Frequently-Asked-Questions
