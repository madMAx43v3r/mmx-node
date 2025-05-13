---
title: Remote Services
description: How to setup and use remote mmx services.
---

The following steps are provided for running multiple harvesters with a single node, or for separate node / farmer / wallet setups.

To enable remote access to a Node or Farmer:
```bash frame="none"
echo true > config/local/allow_remote
```
Alternatively, see "Allow remote service access" in GUI Settings.

Remote harvesters need access to port `11333`, while remote farmer, timelord and wallet need port `11330`.

### Remote Harvester

To run a remote harvester:
```bash frame="none"
./run_harvester.sh -n node.ip
```
Alternatively to set the node address permanently: `echo node.ip > config/local/node`

:::note[Note]
To connect to a remote farmer, replace `node.ip` with the farmer IP.
:::

To disable the built-in harvester in the node: `echo false > config/local/harvester`

### Remote Farmer

To run a remote farmer with it's own wallet and harvester:
```bash frame="none"
./run_farmer.sh -n node.ip
```
Alternatively to set the node address permanently: `echo node.ip > config/local/node`

To disable the built-in farmer in the node: `echo false > config/local/farmer`\
To disable the built-in wallet in the node or farmer: `echo false > config/local/wallet`\
To disable the built-in harvester in the farmer: `echo false > config/local/harvester`

### Remote Timelord

To run a remote timelord:
```bash frame="none"
./run_timelord.sh -n node.ip
```
Alternatively to set the node address permanently: `echo node.ip > config/local/node`

To disable the built-in timelord in the node: `echo false > config/local/timelord`

### Remote Wallet

To run a remote wallet:
```bash frame="none"
./run_wallet.sh -n node.ip
```
Alternatively to set the node address permanently: `echo node.ip > config/local/node`

To disable the built-in wallet in the node: `echo false > config/local/wallet`

### Remote connections over public networks

To use the remote services over a public network such the internet you should use an SSH tunnel, instead of opening port `11330` or `11333` to the world (which would hurt security).

To run an SSH tunnel to connect to a node from another machine (such as from a remote farmer):
```bash frame="none"
ssh -N -L 11330:localhost:11330 user@node.ip
```
This will forward local port `11330` to port `11330` on the node's machine.
