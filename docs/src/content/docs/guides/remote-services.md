---
title: Using Remote Services
description: How to setup and use remote mmx services.
---

## Remote Services

These steps are provided for farming on many machines on a single local area network, or even over a wide area network.

### Remote Farmer

To run a remote farmer with it's own wallet and harvester:
```bash title="Remote Farmer"
./run_farmer.sh -n node.ip:11330
```
Alternatively to set the node address permanently: `echo node.ip:11330 > config/local/node`

To disable the built-in farmer in the node: `echo false > config/local/farmer`

### Remote Harvester

To run a remote harvester, while connecting to a node:
```bash title="Remote Harvester"
./run_harvester.sh -n node.ip:11330
```
Alternatively to set the node address permanently: `echo node.ip:11330 > config/local/node`

To run a remote harvester, while connecting to a remote farmer:
```bash title="Remote Harvester"
./run_harvester.sh -n farmer.ip:11333
```
Alternatively to set the farmer address permanently: `echo farmer.ip:11333 > config/local/node`

To disable the built-in harvester in the node: `echo false > config/local/harvester`

### Remote Timelord

To run a remote timelord:
```bash title="Remote Timelord"
./run_timelord.sh -n node.ip:11330
```
Alternatively to set the node address permanently: `echo node.ip:11330 > config/local/node`

To disable the built-in timelord in the node: `echo false > config/local/timelord`

### Remote Wallet

To run a remote wallet:
```bash title="Remote Wallet"
./run_wallet.sh -n node.ip:11330
```
Alternatively to set the node address permanently: `echo node.ip:11330 > config/local/node`

To disable the built-in wallet in the node:
```bash title="Disable Wallet"
echo false > config/local/wallet
echo false > config/local/farmer
```

### Remote connections over public networks

To use the remote services over a public network such the internet you should use an SSH tunnel, instead of opening port `11330` to the world.

To run an SSH tunnel to connect to a node from another machine (such as from a remote farmer):
```bash title="SSH Tunnel"
ssh -N -L 11330:localhost:11330 user@node.ip
```
This will forward local port `11330` to port `11330` on the node's machine.