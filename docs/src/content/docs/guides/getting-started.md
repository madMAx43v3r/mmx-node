---
title: Getting Started
description: Getting started with MMX.
---

**[Install](../installation/) MMX-Node.**

## GUI

The native GUI can be opened by searching for `MMX Node` (when installed via binary package).

In case of compiling from source:
```bash frame="none"
./run_node.sh --gui      # includes wallet and farmer
./run_wallet.sh --gui    # remote wallet (connect to another node)
```

### WebGUI

The WebGUI is available at: http://localhost:11380/gui/

See `$MMX_HOME/PASSWD` or `mmx-node/PASSWD` for the login password, it is auto generated at compile time.

In case of a binary package install, the password will be in `~/.mmx/PASSWD`. By default, the GUI is only available on localhost.

## CLI

When compiled from source:
```bash frame="none"
cd mmx-node
source ./activate.sh
```
With a binary package install, just open a new terminal. On Windows search for `MMX CMD`.

:::note[Note]
A new wallet can also be created in the GUI.
:::

### Creating a Wallet (offline)

```bash frame="none"
mmx wallet create [-f filename] [--with-passphrase]
```

The file name argument is optional. By default it is `wallet.dat`, which will be loaded automatically.

Additional wallets need to be added to `key_files` array in `config/local/Wallet.json`.

To use a passphrase, specify `--with-passphrase` without the actual passphrase:
```bash frame="none"
mmx wallet create --with-passphrase
Passphrase: <type here>
```

To create a wallet with a known mnemonic seed, specify `--with-mnemonic` without the actual words:
```bash frame="none"
mmx wallet create --with-mnemonic
Mnemonic: word1 word2 ... <enter>
```

To get the mnemonic seed from a wallet (with Node / Wallet already running):
```bash frame="none"
mmx wallet get seed [-j index]
```

:::note[Note]
A Node / Wallet restart is needed to pick up a new wallet.
:::

### Creating a Wallet (online)

With a running Node / Wallet:
```bash frame="none"
mmx wallet new [name] [--with-mnemonic] [--with-passphrase] [-N <num_addresses>]
```
All parameters are optional. The new wallet can be seen with `mmx wallet accounts` (last entry).

To use a known mnemonic seed, specify `--with-mnemonic` without the actual words. Then input the words when promted, with a space inbetween them.

To use a passphrase, specify `--with-passphrase` without the actual passphrase. Then input the passphrase when promted.

### Running a Node

First perform the installation and setup steps, then:

```bash frame="none"
./run_node.sh
```
You can enable port forwarding on TCP port 11337, if you want to help out the network and accept incoming connections.

### Configuration

:::note[Note]
Capitalization of configuration files names matters.\
Any config changes require a node restart to become effective.
:::

#### Custom Farmer Reward Address

Create / Edit file `config/local/Farmer.json`:
```json
{
  "reward_addr": "mmx1..."
}
```
By default the first address of the first wallet is used.

#### Custom Timelord Reward Address

Create / Edit file `config/local/TimeLord.json`:
```json
{
  "reward_addr": "mmx1..."
}
```

#### Fixed Node Peers

Create / Edit file `config/local/Router.json`:
```json
{
  "fixed_peers": ["192.168.0.123", "more"]
}
```

#### Enable Timelord

```bash frame="none"
echo true > config/local/timelord
```

#### CUDA Settings

Create / Edit `config/local/cuda.json`:
```json
{
  "enable": true,
  "devices": [0, 1, ...]
}
```
Empty device list = use all devices. Device indices start at 0. CUDA is enabled by default for all devices.

#### Custom home directory

To set a custom storage path for the blockchain DB, wallet files, etc:
```bash frame="none"
export MMX_HOME=/your/path/
```
Wallet files will end up in `MMX_HOME`, everything else in `mainnet` subfolder.
When compiling from source, `MMX_HOME` is not set, so it defaults to the current directory.
When installing a binary package `MMX_HOME` defaults to `~/.mmx/`.

:::note[Note]
A trailing `/` in the path is required.
:::

#### Custom data directory

To store the DB in a custom directory you can set environment variable `MMX_DATA` (for example):
```bash frame="none"
export MMX_DATA=/mnt/mmx_data/
```
A node restart is required. Optionally the `mainnet` folder can be copied to the new `MMX_DATA` path (after stopping the node), to avoid having to sync from scratch again.

:::note[Note]
A trailing `/` in the path is required.
:::

#### Reducing network traffic

If you have a slow internet connection or want to reduce traffic in general you can lower the number of connections in `config/local/Router.json`.
For example to run at the bare recommended minimum:
```json
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
```bash frame="none"
screen -S node    # start node as above
# <Ctrl+A> + D    # to detach
screen -r node    # to attach again
```

### Recover from forking

To re-sync starting from a specific height: `mmx node revert <height>`.
This is needed if for some reason the node forked from the network. Just subtract 1000 blocks or more from the current height you are stuck at.

## Plotting

**For an in depth guide on plotting, see** [Plotting Guide](../mmx-plotter/).

To get the farmer key for plotting:
```bash frame="none"
mmx wallet keys [-j index]
```
The node needs to be running for this command to work. (`-j` to specify the index of a non-default wallet)

Via GUI the farmer key can be found in Wallet > Info section, see `farmer_public_key`:

![image](https://github.com/madMAx43v3r/mmx-node/assets/951738/7ebc8eaa-d0f9-43fd-bb87-0788a59b138a)

:::note[Note]
During plotting, the node does not need to be running (the plotter doesn't even need internet connection).
:::

Download CUDA plotter here: [mmx-binaries/mmx-cuda-plotter](https://github.com/madMAx43v3r/mmx-binaries/tree/master/mmx-cuda-plotter)

There is no CPU plotter anymore for the new format, because it would be too inefficient. Any old Nvidia GPU will do, Maxwell or newer.

```bash frame="none"
# Example Full RAM:
./mmx_cuda_plot_k30 -C 5 -n -1 -t /mnt/tmp_ssd/ -d <dst> -f <farmer_key>
# Example Partial RAM:
./mmx_cuda_plot_k30 -C 5 -n -1 -2 /mnt/tmp_ssd/ -d <dst> -f <farmer_key>
# Example Disk Mode:
./mmx_cuda_plot_k30 -C 5 -n -1 -3 /mnt/tmp_ssd/ -d <dst> -f <farmer_key>
```

If you would like to use compressed plots, it is recommended that you run the mmx_posbench tool to benchmark your hardware.

To create SSD plots (for farming on SSDs) add `--ssd` to the command and use `-C 0`. SSD plots are 250% more efficient but cannot be farmed on HDDs. They have higher CPU load to farm, hence it's recommended to plot uncompressed.

The minimum k-size for mainnet is k29, the maximum k-size is k32.

To add a plot directory add the path to `plot_dirs` array in `config/local/Harvester.json`, for example:
```json
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
