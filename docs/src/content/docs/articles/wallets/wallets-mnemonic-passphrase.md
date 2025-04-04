---
title: Wallets, mnemonic and passphrase
description: A look at MMX wallets, mnemonic and passphrase
prev: false
next: false

articlePublished: 2024-07-08
articleUpdated: 2025-01-30

authorName: voidxno
authorPicture: https://avatars.githubusercontent.com/u/53347890?s=200
authorURL: https://github.com/voidxno
---

A look at MMX wallets, mnemonic and passphrase.

_**IMPORTANT:**_ _My own understanding, using mmx-node. I take no responsibility for content. When it comes to crypto and wallets, YOU need to educate yourself. As always, NEVER share your mnemonic or passphrase with others._

## TLDR;

- **BACKUP** (securely) both **24-words mnemonic** AND **passphrase** (if used)
- **NEVER share** your **mnemonic** or **passphrase** with others
- Passphrase is not a global lock password for all your wallets
- Passphrase is local to, and part of the wallet you created
- Passphrase is optional, works like the 25th word of mnemonic
- Identical 24-words mnemonic, with/without passphrase, are different wallets
- **Take security seriously** (look [Security](#security))
- Public farmer key is not affected by passphrase (look [Farming](#farming))
- Evaluate what farmer reward address is set to (look [Farming](#farming))
- If using other addresses than main one for wallet (look [Addresses](#addresses))

Wallet is [ECDSA](https://en.wikipedia.org/wiki/Elliptic_Curve_Digital_Signature_Algorithm) based, with [BIP39](https://github.com/bitcoin/bips/blob/master/bip-0039.mediawiki) functionality. Built with tried and tested elements from Bitcoin wallet.

## Backup

As long as you **backup** (securely), in some way, the **24-words mnemonic** and **passphrase** (if used). You will always be able to recreate wallet. Only information needed.

Also recommended to **note** the **finger\_print** and main **address** of wallet (Index[0]). Easy indicators to make sure you recreated the correct wallet with backup information.


:::caution[Important]
Perform the two steps above, and you will always be able to recreate and verify wallet.
:::

## HowTo

Look [Find elements](#find-elements) on how to find information above.

You can also backup (securely), the `.dat` file, that represents the wallet. Not really needed. The 24-words mnemonic and passphrase (if used), are enough to recreate wallet. Also, if wallet was created with a passphrase. You still need passphrase to access wallet, in addition to `.dat` file.

:::tip
Topics in this article can be tricky to get a grip on. Try asking an LLM chatbot about _"BIP39 wallet standard"_, _"mnemonic"_ and _"passphrase"_. Converse, and you might get answers with angles that works better for you.
:::

## Mnemonic

_NOTE: Ignoring optional passphrase for now (25th word), that is next section._

The 24-words mnemonic is a user friendly way to represent the raw binary hexadecimals seed value for wallet ([BIP39](https://github.com/bitcoin/bips/blob/master/bip-0039.mediawiki)).

Another angle: It is the absolute secret that both creates and gives full access to wallet.

When you create a wallet, you have two options:
- Specify existing 24-words mnemonic, and recreate an existing wallet.
- Don't specify 24-words mnemonic, get a new wallet created, with new random generated 24-words mnemonic.

## Passphrase

This element causes some confusion. It is just MMX utilizing the [BIP39](https://github.com/bitcoin/bips/blob/master/bip-0039.mediawiki) standard in the way it is defined. Nothing more, nothing less.

But the simple logic behind BIP39 gives a few use-cases. Depending on your creativity.

Technically, not complicated:
- Wallet (no passphrase): mnemonic = secret seed (and a unique wallet)
- Wallet (with passphrase): mnemonic + passphrase = secret seed (and a unique wallet)

Two 100% unique wallets above, even if identical 24-words mnemonic. Seed will be different, finger\_print will be different, main address (Index[0]) will be different.

Look at passphrase as the optional 25th word, that you need to type in each time you want to perform actions with wallet.

Most important. If you use a passphrase on one of your wallets. You MUST backup that passphrase, in addition to the 24-words mnemonic. Without you will not be able to recreate that wallet. MMX is not storing that passphrase anywhere. Only thing stored in wallet `.dat` file is the 24-words mnemonic. Which is why passphrase is kind of looked at as password for wallet.

Two consequences of BIP39 simplicity:
- Not possible: Add a passphrase to an existing wallet without passphrase.
- Not possible: Change passphrase of an existing wallet with passphrase.
- Why: Passphrase (25th word) is a static part of wallet secret.
- Solution (both cases): Create a new wallet with existing 24-words mnemonic and new passphrase. This will be a 100% new unique wallet. Then transfer your funds from old to new wallet. Which are two totally independent wallets. Just sharing the same 24-words mnemonic.

Some use-cases:
- Enhancing security with extra layer on top of 24-words mnemonic.
- Have different wallets with same mnemonic, but different passphrases.
- Separate funds, hidden wallets, threat scenario, different passphrases.

## Security

YOUR responsibility. Too broad of a topic. A few facts:
- 24-words mnemonic: Stored in wallet `.dat` file on disk (file itself not encrypted)
- Passphrase: Never stored anywhere on disk (only temporary in memory, when used)

Full access to a wallet:
- Wallet (no passphrase): Only need 24-words mnemonic (i.e., `.dat` file)
- Wallet (with passphrase): Need both 24-words mnemonic and passphrase

Consider:
- Evaluate using passphrase for better security.
- Risk of unauthorized access to wallet `.dat` files.
- Use of cold wallets, not all wallets have to be active.
- Send rewards or other funds to cold wallet addresses.

Ultimately. Your responsibility how you store, use, and arrange wallets.

## Farming

### Farmer Public Key

You create plots with a `farmer_public_key` (66 chars) from a wallet. This wallet needs to be present on mmx-node to farm those plots.

In relation to wallets, `farmer_public_key` can create some confusion. Both statements are true:
- Identical 24-words mnemonic, with/without passphrase, gives **identical** `farmer_public_key`
- Identical 24-words mnemonic, with/without passphrase, are **different wallets**

Logic:
- `farmer_public_key` is created from 24-words mnemonic + a fixed passphrase ("`MMX/farmer_keys`").
- Independent of wallet having a passphrase, or not.

Done this way on purpose. No compromise on security. Makes it possible for mmx-node to start farming, even if wallet has passphrase. No waiting for user input of passphrase on startup. Pragmatic choice to make farming as seamless as possible.

You will never generate identical 24-words mnemonic as another user. Primary purpose of passphrase is extra security, not better randomness.

### Farmer Reward Address

Can argue a farming wallet created without passphrase and getting rewards, is an issue. The `.dat` file of that wallet is enough to access reward funds. Still need access to file though. An operational choice, totally avoidable. If wanted.

Farmer Reward Address (SETTINGS), can be set to any wallet address. Until set, mmx-node defaults to main address (Index[0]) of first wallet created. Which often is your farming wallet, usually without a passphrase.

Make it a non-issue:
- Create a new farming wallet, same 24-words mnemonic, now with a passphrase:
  - This becomes a new unique wallet, with a new main address.
  - Send rewards to new main address, that you need passphrase to access.
  - Plots still farming, because of `farmer_public_key` logic above.
- Or, set reward address yourself to another cold wallet (best practice).

Most important. Double-check that Farmer Reward Address (SETTINGS) is set to what you want and expect. Security and access to wallet controlling that address is up to you.

## Addresses

One wallet can have more than one address. When creating a wallet, default is 1 for Number of Addresses. Was 100 in earlier mmx-node releases. You can still set it to 100, or another number. Totally ok, no problem.

Rationale:
- Easier for most users to operate with one wallet equals one address.
- Memo functionality can eliminate need of multiple addresses in most cases.

When you recreate a wallet, addresses will be identical to first time you create it. They are unique for your wallet, ordered identically. Independent of how many you choose to be shown.

Be aware of ('missing' funds):
- No problem sending funds to other index addresses than main one (Index[0]).
- If wallet recreated with 1 as number of addresses, will not see those funds.

Your wallet is still key to control all those Index[x] addresses. But wallet will only enumerate number of addresses it was specified with.

So, if missing funds on a wallet. Make sure it is not issue above, by recreating wallet with at least number of addresses that have seen activity.

## Account Index

_NOTE: Only an explanation that accounts within a wallet technically exists. Not needed in normal usage._

The 'Account' word can mislead in this context. Each wallet listed in WebGUI are also 'Accounts'. What we are talking about here is 'Account Index' **within a wallet**.

Not to be confused with the `account` index number assigned to a wallet starting at `Wallet #100` (or `#0` if `wallet.dat`). That is just a counter with no significance. Only for mmx-node itself to number the wallets it finds in `Wallet.json`.

Account Index here is the `index` property assigned to `0` (default), on each wallet created by mmx-node. Written as `"index": 0,` on each wallet entry in `Wallet.json`. That property has an effect of generating a unique set of addresses for that wallet. Or more specifically, that wallets account index.

Unless you have a very specific need of this logic. Ignore it, let it stay at `0` (default).

## .dat files

Wallet `.dat` files are stored at following locations:
- Linux: `/mmx-node/` `wallet_<fingerprint>.dat`
- Windows: `C:\Users\<user>\.mmx\` `wallet_<fingerprint>.dat`

Contains the binary version of 24-words mnemonic for each wallet.

Can be named only `wallet.dat` if created through CLI interface, or older version of mmx-node. Will be `Wallet #0`, vs others who starts at `#100`. Used as any other wallet.

It does not hurt to backup them, but you still need passphrase in addition (if used). Look [Backup](#backup) and [HowTo](#howto).

There is also `info_<fingerprint>.dat` files. Cached non-sensitive information about passphrased wallets:
- Linux: `/mmx-node/testnet12/wallet/` `info_<fingerprint>.dat`
- Windows: `C:\Users\<user>\.mmx\testnet12\wallet\` `info_<fingerprint>.dat`

Generated for wallets with passphrase on first unlock. Used by mmx-node to get addresses for a wallet with passphrase. Without unlocking it.

Can check contents of file yourself (Linux):
- `vnxread -f /mmx-node/testnet12/wallet/info_<fingerprint>.dat`

## Find elements

How to find key wallet elements above using WebGUI.

### &#x1F50D; Wallets

Listed under main WALLET tab. No global master wallet logic. A flat list of independent wallets:
- Wallet #xxx: Just a counter, nothing of significance.
- Address of wallet: Main address of wallet (Index[0]).
- Account Name of wallet: If named when created.

Each wallet has tabs for info and operations. Most important here (to the right):
- INFO
- Settings (option wheel symbol)

### &#x1F50D; Mnemonic (24-words)

Navigate to the Settings (option wheel symbol) tab for wallet in question:
- Press SHOW SEED button

Copy/paste or write down the 24-words mnemonic. Remember to get the order of words correct.

Look [Mnemonic](#mnemonic) section for information about what it represents.

### &#x1F50D; Passphrase

Not stored anywhere by mmx-node. Important information you must remember yourself, or store securely.

Look [Passphrase](#passphrase) section for information about what it represents.

### &#x1F50D; finger_print & address

Navigate to the INFO tab for wallet in question:
- address: Main address of wallet (Index[0])
- finger_print: Unique 32-bit fingerprint value for wallet

Either one is a good indicator to know if you have recreated correct wallet.

Look [Backup](#backup) section for why you should also save this information.

## Feedback

More questions, or wrong info above. Use [`#mmx-general`](https://discord.com/channels/852982773963161650/925017012235817042) or [`#mmx-support`](https://discord.com/channels/852982773963161650/853021000135475270) channel on Discord.
