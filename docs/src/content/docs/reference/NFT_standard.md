---
title: NFT Standard
---

NFTs on MMX are instances of the [nft.js](https://github.com/madMAx43v3r/mmx-node/blob/master/src/contract/nft.js) contract with binary `mmx1hzz9tgs2dz9366t3p4ep8trmaejx7tk9al9ah3md2u37pkesa3qqfyepyw`.

Every contract inherits from [TokenBase](https://github.com/madMAx43v3r/mmx-node/blob/master/interface/contract/TokenBase.vni), as such each NFT already has the following (read-only) fields:
- `name`: String, max length 64
- `meta_data`: Object

The standard for `meta_data` is as follows:
- `description`: A human-readable description of the item. Markdown is supported.
- `image`: Image URL. Use `mmx://mmx1...` for on-chain data.
- `external_url`: Link to external site to view NFT.
- `attributes`: Object of custom attributes (not standardized).
