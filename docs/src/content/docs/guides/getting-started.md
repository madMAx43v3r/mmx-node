---
title: Getting Started
description: Getting started with MMX.
i18nReady: true
---

## GUI
The native GUI can be opened by searching for `MMX Node`.

In case of compiling from source:
```
./run_node.sh --gui
./run_wallet.sh --gui
```
The WebGUI is available at: http://localhost:11380/gui/

See `$MMX_HOME/PASSWD` or `mmx-node/PASSWD` for the login password, it is auto generated at compile time.

In case of a binary package install, the password will be in `~/.mmx/PASSWD`. By default, the GUI is only available on localhost.

## CLI Setup

When compiled from source:
```
cd mmx-node
source ./activate.sh
```
With a binary package install, just open a new terminal. On Windows search for `MMX CMD`.

Note: A new wallet can also be created in the GUI.

## Creating a Wallet (offline)

```
mmx wallet create [-f filename] [--with-passphrase]
```

The file name argument is optional. By default it is `wallet.dat`, which will be loaded automatically.

Additional wallets need to be added to `key_files` array in `config/local/Wallet.json`.

To use a passphrase, specify `--with-passphrase` without the actual passphrase:
```
$ mmx wallet create --with-passphrase
Passphrase: <type here>
```
It will be queried interactively, to avoid it being stored in terminal history.

To create a wallet with a known mnemonic seed:
```
mmx wallet create --mnemonic word1 word2 ... [-f filename] [--with-passphrase]
```

To create a wallet with a known seed hash (legacy):
```
mmx wallet create <seed_hash> [-f filename] [--with-passphrase]
```

To get the mnemonic seed from a wallet (with Node / Wallet already running):
```
mmx wallet get seed [-j index]
```

Note: A Node / Wallet restart is needed to pick up a new wallet.

## Creating a Wallet (online)

With a running Node / Wallet:
```
mmx wallet new [name] [--with-passphrase] [-N <num_addresses>]
```
All parameters are optional. The new wallet can be seen with `mmx wallet accounts` (last entry).

To use a passphrase, specify `--with-passphrase` without the actual passphrase. It will be queried interactively, to avoid it being stored in terminal history.


## Configuration
Note: Capitalization of configuration files matters. \
Note: Any config changes require a node restart to become effective.

#### Custom Farmer Reward Address

Create / Edit file `config/local/Farmer.json`:
```
{
	"reward_addr": "mmx1..."
}
```
By default the first address of the first wallet is used.

#### Custom Timelord Reward Address

Create / Edit file `config/local/TimeLord.json`:
```
{
	"reward_addr": "mmx1..."
}
```

#### Fixed Node Peers

Create / Edit file `config/local/Router.json`:
```
{
	"fixed_peers": ["192.168.0.123", "more"]
}
```

#### Enable Timelord
```
echo true > config/local/timelord
```

#### Custom data directory

To have the blockchain and DB stored in a custom directory you can set environment variable `MMX_DATA` (for example):
```
export MMX_DATA=/mnt/mmx_data/
```
A node restart is required. Optionally the previous `testnetX` folder can be copied to the new `MMX_DATA` path (after stopping the node), to avoid having to sync from scratch again.

## Plotting

To get the farmer key for plotting:
```
mmx wallet keys [-j index]
```
The node needs to be running for this command to work. (`-j` to specify the index of a non-default wallet)

Via GUI the farmer key can be found in Wallet > Info section, see `farmer_public_key`: 

![image](https://github.com/madMAx43v3r/mmx-node/assets/951738/7ebc8eaa-d0f9-43fd-bb87-0788a59b138a)

Note: During plotting, the node does not need to be running (the plotter doesn't even need internet connection).

Download CUDA plotter here: https://github.com/madMAx43v3r/mmx-binaries/tree/master/mmx-cuda-plotter

There is no CPU plotter anymore for the new format, because it would be too inefficient. Any old Nvidia GPU will do, Maxwell or newer.

Example Full RAM: `./mmx_cuda_plot_k30 -C 5 -n -1 -t /mnt/tmp_ssd/ -d <dst> -f <farmer_key>`

Example Partial RAM: `./mmx_cuda_plot_k30 -C 5 -n -1 -2 /mnt/tmp_ssd/ -d <dst> -f <farmer_key>`

Example Disk Mode: `./mmx_cuda_plot_k30 -C 5 -n -1 -3 /mnt/tmp_ssd/ -d <dst> -f <farmer_key>`

Usage is the same as for Gigahorse (https://github.com/madMAx43v3r/chia-gigahorse/tree/master/cuda-plotter), just without `-p` pool key.

If you have a fast CPU ([passmark benchmark](https://www.cpubenchmark.net) > 5000 points) you can use `-C 10` for HDD plots.

To create SSD plots (for farming on SSDs) add `--ssd` to the command and use `-C 0`. SSD plots are 250% more efficient but cannot be farmed on HDDs. They have higher CPU load to farm, hence it's recommended to plot uncompressed.

The minimum k-size for mainnet will be k29, for testnet it is k26. The plots from testnet11 and onwards can be reused for mainnet later.

To add a plot directory add the path to `plot_dirs` array in `config/local/Harvester.json`, for example:
```
{
	"plot_dirs": [
		"/mnt/drive1/plots/",
		"/mnt/drive2/plots/",
		"C:/windows/path/example/"
	]
}
```
Directories are searched recursively by default. To disable recursive search you can set `recursive_search` to `false` in `Harvester.json`.

For the above reason, avoid adding a root directory (e.g. `H:\`), unless your drive only contains plots. Instead, make a folder and place all your plots in there (e.g. `H:\MMX Plots\`).

## Running a Node

First perform the installation and setup steps.

To run a node for the current testnet:
```
./run_node.sh
```

You can enable port forwarding on TCP port 12341, if you want to help out the network and accept incoming connections.

To set a custom storage path for the blockchain, wallet files, etc:
```
export MMX_HOME=/your/path/
```
Wallet files will end up in `MMX_HOME`, everything else in a `testnetX` subfolder. By default `MMX_HOME` is not set, so it's the current directory.

### Reducing network traffic

If you have a slow internet connection or want to reduce traffic in general you can lower the number of connections in `config/local/Router.json`.
For example to run at the bare recommended minimum:
```
{
	"num_peers_out": 4,
	"max_connections": 4
}
```
`num_peers_out` is the maximum number of outgoing connections to synced peers. `max_connections` is the maximum total number of connections.
Keep in mind this will increase your chances of losing sync.

Another more drastic measure is to disable relaying messages to other nodes, by setting `do_relay` to `false` in `config/local/Router.json`.
However this will hurt the network, so please only disable it if absolutely necessary.

### Running in background

To run a node in the background you can enter a `screen` session:
```
screen -S node
(start node as above)
<Ctrl+A> + D (to detach)
screen -r node (to attach again)
```

### Recover from forking

To re-sync starting from a specific height: `./run_node.sh --Node.replay_height <height>`.
Or while running: `mmx node revert <height>`.
This is needed if for some reason you forked from the network. Just subtract 500 or 1000 blocks from the current height you are stuck at.
To re-sync from scratch delete `block_chain.dat` and `db` folder in `testnetX`, or run `mmx node revert 0`.

### Switching to latest testnet

After stopping the node:
```
rm NETWORK
./update.sh
./run_node.sh
```
Blockchain data are now stored in `testnetX` folder by default.
