---
title: Web Wallet API
description: API to interact with the web wallet (extension).
---

The MMX Web Wallet can be accessed from any webpage via `window.mmx_wallet`.

Even though the wallet can have multiple accounts or addresses, only one is active at a time, to simplify the API.

### get_address()

Returns the (currently active) wallet address in bech32 format.

Keep in mind this address can be spoofed by the wallet.
To make sure the user actually owns this address you need to have them sign a random message via `sign_message(msg)` and verify the signature. 

### get_public_key()

Returns the (currently active) wallet public key in hex string format (upper case).

### get_network()

Returns `MMX/mainnet` for mainnet.

### sign_message(msg)

Signs a string message with the prefix `MMX/sign_message/` using SHA2-256. Used to prove ownership of the wallet (address).

Example:
```
wallet.sign_message("test123") // -> Sign(SHA256("MMX/sign_message/test123"))
```

Returns an object with the signature and public key in hex string format (upper case):
```
{"signature": "123ABC...", "public_key": "345345FDD"}
```

Returns `null` in case the user doesn't approve.

### sign_transaction(tx)

Signs a given transaction if the user approves it.

The `tx` format is the same as specified in `interface/Transaction.vni`.

If `tx.id` is not specified, the user is free to modify the given transaction, such as expiration height or fee level.

Returns the signed transaction in full. Returns `null` in case the user doesn't approve.
