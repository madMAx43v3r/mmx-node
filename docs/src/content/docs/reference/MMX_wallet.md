---
title: MMX Wallet
description: Important information for developing a wallet for MMX.
---
## MMX Wallet / Address Format

Notes for 3rd party development:
- When converting mnemonic words to a wallet seed, the byte order needs to be reversed, compared to BIP-39.
- The key derivation function for the passphrase is non-standard:
	- There is no chain XORing (not needed for sha512)
	- There is no index being hashed in the first iteration
- The byte order for bech32 addresses is reversed:
	- When converting a public key to bech32, first the sha256 hash is computed, then the byte order is reversed and converted with standard Bech32m.
	- When converting a bech32 address to a hash, the bytes from the resulting standard Bech32m decode need to be reversed.
