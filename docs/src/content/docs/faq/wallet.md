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

If you have to create multiple wallets, be sure to edit your `/config/local/Wallet.json` file and enter your `wallet_name.dat` file under `"key_files+": []`

**`mmx wallet create` is the only MMX command that does not need the node running in the background.**

### _I lost my wallet.dat file, can I recover it with my 64-character seed hash instead?_
Yes. Run `mmx wallet create [seed hash]`. You might have to cd into the /mmx-node/ folder and run `source ./activate.sh`

For Windows users, run the command from the shortcut placed in the start menu.

### _Is it true that the seed hash will be replaced with 24 mnemonic words instead? How can I get them if I had an old wallet created before testnet7?_
Yes, once your node properly loads your wallet.dat, you can run `mmx wallet get seed` to show your 24 mnemonic seed words.

In the future, you can even generate your wallet.dat file with `mmx wallet create --mnemonic word1 word2 word3 .....`

### _So I have setup everything right, my farmer is working, my plots are passing the filter. I have a large % of the netspace and I have farmed for days but I haven't won any MMX coins. What's wrong?_

**Realize that with the same seed hash, your wallet address has changed since testnet7.**

If you have set your farming reward sent to a cold wallet, you need to check that you still have proper access to the old wallet, or update the reward address to a new one. You can check your farmer reward address in `Farmer.json` file in `/config/local/`.

If you currently have that set to point to `null` address, it's going to send reward to your default/hot wallet.