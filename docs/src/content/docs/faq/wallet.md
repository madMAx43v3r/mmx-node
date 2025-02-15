---
title: Wallet FAQ
description: Frequently asked questions about using MMX Wallet.
---
# Wallet

### _I start my node and it showed warnings that it cannot find my wallet. How do I create a wallet?_
You can create a wallet in the GUI now. Otherwise see below:

Make sure that your wallet file is named `wallet.dat` and not `wallet_xxxxx.dat` and that it's placed in the right folder.

For Linux, it's `~/mmx-node/`

For Windows, it's `C:\Users\name\.mmx\`

If the file does not exist yet, you may create it by running `mmx wallet create -f [filename.dat]`

If you did not specify a filename, the default name `wallet.dat` will be used.

If you have to create multiple wallets, be sure to edit your `config/local/Wallet.json` file and enter your `wallet_name.dat` file under `"key_files+": []`

**`mmx wallet create` is the only MMX command that does not need the node running in the background.**
