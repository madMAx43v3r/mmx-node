---
title: CLI Commands
description: MMX Node CLI Command Reference.
---

When compiled from source:
```bash frame="none"
cd mmx-node
source ./activate.sh
```
With a binary package install, just open a new terminal. On Windows search for `MMX CMD`.

To run any `mmx` commands (except `mmx wallet create`), the node needs to be running. See [Getting Started](../../guides/getting-started/) to read on how to start it.

## Node CLI

To check on the node: `mmx node info`

To check on the peers: `mmx node peers`

To check on a transaction: `mmx node tx <txid>`

To show current node height: `mmx node get height`

To dump a transaction: `mmx node get tx <txid>`

To dump a contract: `mmx node get contract <address>`

To get balance for an address: `mmx node get balance <address> -x <currency>`

To get raw balance for an address: `mmx node get amount <address> -x <currency>`

To dump a block: `mmx node get block <height>`

To dump a block header: `mmx node get header <height>`

To show connected peers: `mmx node get peers`

To show estimated netspace: `mmx node get netspace`

To show circulating coin supply: `mmx node get supply`

To call a smart contract const function: `mmx node call`

To show smart contract state variables: `mmx node read`

To dump all storage of a smart contract: `mmx node dump`

To dump assembly code of a smart contract: `mmx node dump_code`

To fetch a block from a peer: `mmx node fetch block <peer> <height>`

To fetch a block header from a peer: `mmx node fetch header <peer> <height>`

To check the balance of an address: `mmx node balance <address>`

To check the history of an address since a particlar block height: `mmx node history <address> <block_height>`

To show all offers: `mmx node offers [open | closed]`

To force a re-sync: `mmx node sync`

To replay/revert to an earlier block height: `mmx node revert <height>`

## Wallet CLI

To show everything in a wallet: `mmx wallet show`

To show wallet balances: `mmx wallet show balance`

To show wallet contracts: `mmx wallet show contracts`

To show wallet offers: `mmx wallet show offers`

To get a specific wallet address: `mmx wallet get address`

To get a specific wallet balance: `mmx wallet get balance`

To get a specific raw wallet balance: `mmx wallet get amount`

To get a list of all contract addresses: `mmx wallet get contracts`

To get the mnemonic seed words of a wallet: `mmx wallet get seed`

To show entire wallet activity : `mmx wallet log`

To show recent wallet activity : `mmx wallet log -N <limit>`

To transfer funds: `mmx wallet send <options>`
```
  -a <amount to send, 1.23>
  -r <tx fee multiplier>
  -t <destination address>
  -x <currency>
```
To withdraw funds from a contract: `mmx wallet send_from -s <address>`

To transfer an NFT, same as sending with one satoshi: `mmx wallet transfer`

To create an offer on the chain: `mmx wallet offer`
```
  (-x / -z default MMX)
  -a <bid amount> -b <ask amount>
  -x <bid currency> -z <ask currency>
```
To accept an offer: `mmx wallet accept <address>`

To mint tokens: `mmx wallet mint`
```
  -a <amount>
  -t <destination>
  -x <currency>
```
To deploy a contract: `mmx wallet deploy <file>`

To execute a smart contract function: `mmx wallet exec <function> <args> -x <contract>`

To deposit funds to a smart contract: `mmx wallet deposit <function> <args>`
```
  -a <amount to send, 1.23>
  -t <contract address>
  -x <currency>
```

To create a new wallet (offline): `mmx wallet create -f [file_name] [--with-passphrase]`

To create a new wallet (online): `mmx wallet new [name] [--with-passphrase]`

To restore a wallet from a seed hash: `mmx wallet create --with-seed`

To restore a wallet from a set of 24 mnemonic words: `mmx wallet create --with-mnemonic`

To show all wallets and their index: `mmx wallet accounts`

To get farmer / pool keys for plotting: `mmx wallet keys`

To lock a wallet if passphase enabled: `mmx wallet lock`

To unlock a wallet with passphrase: `mmx wallet unlock`

**To use a non-default wallet, specify `-j <index>` in combination with the above commands. See `mmx wallet accounts`.**

## Farmer CLI

To check on the farm: `mmx farm info`

To get total space in bytes: `mmx farm get space`

To show plot directories: `mmx farm get dirs`

To add plot directories: `mmx farm add <dir>`

To remove plot directories: `mmx farm remove <dir>`

To reload plots: `mmx farm reload`

## Harvester CLI

To check on the harvester: `mmx harvester info`

To get harvester space in bytes: `mmx harvester get space`

To show plot directories: `mmx harvester get dirs`

To add plot directories: `mmx harvester add <dir>`

To remove plot directories: `mmx harvester remove <dir>`

To reload plots: `mmx harvester reload`

## Pooling

To use pooling first create a plot NFT, then create plots for it and finally join a pool.

### Create Plot NFT

```bash frame="none"
mmx wallet plotnft create <name>
```

`<name>` can be any string without whitespace.

After creation the plot NFT is in solo farming mode, which means block rewards will directly go to your Farmer reward address.
See below how to join a pool.

:::note[Note]
Need to wait for the transaction to confirm before it will show up.\
This command takes the usual `-j <index>` argument to select a different wallet.
:::

### Show Plot NFTs

```bash frame="none"
mmx wallet plotnft show
```

The address shown in `[...]` is the plot NFT contract address, which needs to be used for plotting.

:::note[Note]
This command takes the usual `-j <index>` argument to select a different wallet.
:::

Example:
```bash frame="none"
mmx wallet plotnft show -j 1
```
```
[mmx1wknv8xxvzjafrswsrwr3l85y6d8nms2dz22dgxl2qpcyjp64amtsjqjna5]
  Name: test1
  Locked: true
  Server URL: http://localhost:8080
```

### Join a Pool

First you need to obtain the pool server URL for the pool, via their website or discord.

```bash frame="none"
mmx wallet plotnft join <pool_url> -x <plot_nft_address>
```

`<plot_nft_address>` is the same as used for plotting, see `mmx wallet plotnft show`.

:::note[Note]
This command does not need any `-j` to select a wallet.
:::

Example:
```bash frame="none"
mmx wallet plotnft join http://localhost:8080 -x mmx1wknv8xxvzjafrswsrwr3l85y6d8nms2dz22dgxl2qpcyjp64amtsjqjna5
```

### Leave Pool

```bash frame="none"
mmx wallet plotnft unlock -x <plot_nft_address>
```

This will take 256 blocks to complete, to avoid cheating.
Once complete the plot NFT is in solo farming mode, which means block rewards will directly go to your Farmer reward address.

:::note[Note]
This command does not need any `-j` to select a wallet.
:::

Example:
```bash frame="none"
mmx wallet plotnft unlock -x mmx1wknv8xxvzjafrswsrwr3l85y6d8nms2dz22dgxl2qpcyjp64amtsjqjna5
```

### Switch Pool

First leave the current pool, wait 256 blocks for the plot NFT to unlock, then join the new pool as shown above.

### Show Info

To see pool account: `mmx pool info`

To see partials info: `mmx farm info`

Example:
```bash frame="none"
mmx pool info
```
```
Pool [http://localhost:8080]
  Balance: 1.5 MMX
  Total Paid: 123.456 MMX
  Difficulty: 1
  Pool Share: 33 %
  Partial Rate: 33.5417 per hour
  Blocks Found: 42
  Estimated Space: 13.3789 TB
```

```bash frame="none"
mmx farm info
```
```
Plot NFT [mmx1wknv8xxvzjafrswsrwr3l85y6d8nms2dz22dgxl2qpcyjp64amtsjqjna5]
  Name: test1
  Server URL: http://localhost:8080
  Target Address: mmx1uj2dth7r9tcn3vas42f0hzz74dkz8ygv59mpx44n7px7j7yhvv4sfmkf0d
  Plot Count: 100
  Points: 309 OK / 0 FAIL
  Difficulty: 1
  Avg. Response: 0.515676 sec
  Last Partial: 2024-10-13 23:25:44
```
