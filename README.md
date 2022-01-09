# mmx-node

MMX is a blockchain written from scratch using Chia's Proof Of Space and a SHA256 VDF similar to Solana.

It's main features are as follows:
- High performance (1000 transactions per second or more)
- Variable supply (block reward scales with netspace, but also is capped by TX fees)
- Consistent block times (every 10 seconds a block is created)
- Native token support (swaps are possible with standard transactions)
- Energy saving Proof of Space (same as Chia)
- Standard ECDSA signatures for seamless integration (same as Bitcoin)

MMX is desiged to be a blockchain that can actually be used as a currency.

For example the variable supply will stabilize the price, one of the key properties of any currency.

Furthermore, thanks to an efficient implementation and the avoidance of using a virtual machine, it will provide low transaction fees even at high throughput.

Tokens can either be traditionally issued by the owner, or they can be owner-less and created by staking another token over time, in a decentralized manner governed by the blockchain.

In the future it is planned that anybody can create their own token on MMX using a simple web interface.

In addition the first application for MMX will be a decentralized exchange where users can trade MMX and tokens.

The variable reward function is as follows: \
`reward = max(max(difficulty * const_factor, min_reward), TX fees)`. \
Where `min_reward` and `const_factor` are fixed at launch.

A mainnet launch is planned in ~6 months or so.
Currently we are running _testnet3_, so the coins farmed right now are _not worth anything_.

See `#mmx-news` and `#mmx-general` on discord: https://discord.gg/pQwkebKnPB

## CLI

To use the CLI:
```
cd mmx-node
source ./activate.sh
```

The node needs to be running, see below how to start it.

To check your balance: `mmx wallet show`

To show wallet activity: `mmx wallet log`

To send coins: `mmx wallet send -a 1.234 -t <address>`

To show the first 10 addresses: `mmx wallet show 10`

To get a specific receiving address: `mmx wallet get address <index>`

To show wallet activity since height: `mmx wallet log <since>`

To show wallet activity for last N heights: `mmx wallet log -N`

To get the seed value from a wallet: `mmx wallet get seed`

To use a non-default wallet specify `-j <index>` with above commands (at the end).

To check on the node: `mmx node info`

To check on the peers: `mmx node peers`

To check on a transaction: `mmx node tx <txid>`

To check the balance of an address: `mmx node balance <address>`

To check the history of an address: `mmx node history <address> [since]`

To dump a transaction: `mmx node get tx <txid>`

To dump a block: `mmx node get block <height>`

To dump a block header: `mmx node get header <height>`

To force a re-sync: `mmx node sync`

To get connected peers: `mmx node get peers`

To check on the farm: `mmx farm info`

To get total space in bytes: `mmx farm get space`

To show plot directories: `mmx farm get dirs`

To reload plots: `mmx farm reload`

## Setup

First finish the installation step below.

To continue enter the CLI environment:
```
cd mmx-node
source ./activate.sh
```

### Creating a Wallet

```
mmx wallet create [-f filename]
```

The file name argument is optional, by default it is `wallet.dat`, which is already included in the default configuration.

To use more wallets add the paths to `key_files+` array in `config/local/Wallet.json`.

To create a wallet with a known seed value:
```
mmx wallet create <seed> [-f filename]
```

To get the seed value from a wallet:
```
mmx wallet get seed [-j index]
```

## Running a Node

First perform the installation and setup steps.

To run a node for current `testnet3`
```
./run_node.sh
```

You can enable port forwarding on TCP port 12333 if you want to help out the network and accept incoming connections.

To set a custom storage path for the blockchain, etc:
```
echo /my/path/ > config/local/root_path
```

To set a custom storage path for wallet files create/edit `config/local/Wallet.json`:
```
{
	"storage_path": "/my/path/"
}
```

To disable the `TimeLord` specify `--timelord 0` on the command line.
Alternatively, you can also disable it by default: `echo false > config/local/timelord`.
If you have a slow CPU this is recommended and maybe even needed to stay in sync.

Any config changes require a node restart to become effective.

### Light Node

The node can be run in a "light mode" which filters out any transactions which don't concern your addresses, in addition to disabling message relaying, VDF verification and transaction verification.
A light node relies on other full nodes to validate transactions and only checks that transactions of interest were included in blocks with valid proof (aka SPV, Simplified Payment Verification). 

To run a light node:
```
./run_light_node.sh
```
Light node data will end up in `testnet3/light_node/` by default, there is no conflict with data from a full node.

Note: You cannot add a new wallet afterwards (or increase the number of addresses), if you do so you have to re-sync from scratch.

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
This is needed if for some reason you forked from the network. Just subtract 500 or 1000 blocks from the current height you are stuck at.
To re-sync from scratch delete `block_chain.dat`.

### Switching to latest testnet

After stopping the node:
```
./update.sh
rm config/local/mmx_node.json
rm block_chain.dat known_peers.dat NETWORK
./run_node.sh
```
`block_chain.dat`, `known_peers.dat` and `logs` are now stored in `testnet3` folder by default.

### Remote Farmer

To run a remote farmer with it's own wallet and harvester:
```
./run_farmer.sh -n node.ip:11330
```
Alternatively to set the node address permanently: `echo node.ip:11330 > config/local/node`

To disable the built-in farmer in the node: `echo false > config/local/farmer`

### Remote Harvester

To run a remote harvester, while connecting to a node:
```
./run_harvester.sh -n node.ip:11330
```
Alternatively to set the node address permanently: `echo node.ip:11330 > config/local/node`

To run a remote harvester, while connecting to a remote farmer:
```
./run_harvester.sh -n farmer.ip:11333
```
Alternatively to set the farmer address permanently: `echo farmer.ip:11333 > config/local/node`

To disable the built-in harvester in the node: `echo false > config/local/harvester`

### Remote Timelord

To run a remote timelord:
```
./run_timelord.sh -n node.ip:11330
```
Alternatively to set the node address permanently: `echo node.ip:11330 > config/local/node`

To disable the built-in timelord in the node: `echo false > config/local/timelord`

### Remote Wallet

To run a remote wallet:
```
./run_wallet.sh -n node.ip:11330
```
Alternatively to set the node address permanently: `echo node.ip:11330 > config/local/node`

To disable the built-in wallet in the node:
```
echo false > config/local/wallet
echo false > config/local/farmer
```

### Remote connections over public networks

To use the remote services over a public network such the internet you should use an SSH tunnel, instead of opening port `11330` to the world.

To run an SSH tunnel to connect to a node from another machine (such as from a remote farmer):
```
ssh -N -L 11330:localhost:11330 user@node.ip
```
This will forward local port `11330` to port `11330` on the node's machine.

## Plotting

To get the farmer and pool keys for plotting:
```
mmx wallet keys [-j index]
```

The node needs to be running for this command to work. (`-j` to specify the index of a non-default wallet)

Then use the latest version of my plotter with `-x 11337` argument: https://github.com/madMAx43v3r/chia-plotter

It will show the following output at the beginning to confirm the new plot format (from testnet3 onwards):
```
Network Port: 11337 [MMX] (unique)
```
The new plots will have a name starting with "plot-mmx-". Plots created before that version are only valid on testnet1/2.

The minimum K size for mainnet will be k30, for testnets it is k26. The plots from testnet3 and onwards can be reused for mainnet later.
However there will be a time limit for k30 and k31, ~3 years for k30 and ~6 years for k31, to prevent grinding attacks in the future.

To add a plot directory add the path to `plot_dirs` array in `config/local/Harvester.json`, for example:
```
{
	"plot_dirs": ["/mnt/drive1/plots/", "/mnt/drive2/plots/"]
}
```

## Installation

Note: OpenCL packages are optional, ie. `ocl-icd-opencl-dev`, etc.

Ubuntu Linux:
```
sudo apt update
sudo apt install git cmake build-essential libsecp256k1-dev libsodium-dev zlib1g-dev ocl-icd-opencl-dev clinfo screen
```

Arch Linux:
```
sudo pacman -Syu
sudo pacman -S base-devel git cmake zlib libsecp256k1 libsodium opencl-headers ocl-icd clinfo screen
```

Fedora Linux:
```
yum install git cmake clinfo gcc libsecp256k1-devel libsodium ocl-icd-devel opencl-headers opencl-utils ghc-zlib
```

OpenCL provides faster and more efficient VDF verification using an integrated or dedicated GPU.
A standard iGPU found in Intel CPUs with 192 shader cores is plenty fast enough.
If you dont have a fast quad core CPU (>3 GHz) or higher core count CPU, it is required to have a GPU with OpenCL support.

Make sure to be in the `video` and or `render` group (depends on distribution) to be able to access a GPU:
```
sudo adduser $USER video
sudo adduser $USER render
```

### Building

```
git clone https://github.com/madMAx43v3r/mmx-node.git
cd mmx-node
git submodule update --init --recursive
./make_devel.sh
```

To update to latest version:
```
./update.sh
```

### Windows via WSL

To setup Ubuntu 20.04 in WSL on Windows you can follow the tutorial over here: \
https://docs.microsoft.com/en-us/learn/modules/get-started-with-windows-subsystem-for-linux/

Make sure to install Ubuntu in step 2: https://www.microsoft.com/store/p/ubuntu/9nblggh4msv6

Then type "Ubuntu" in the start menu and start it, you will be asked to setup a user and password.
After that you can follow the normal instructions for Ubuntu 20.04.

To get OpenCL working in WSL:
https://devblogs.microsoft.com/commandline/oneapi-l0-openvino-and-opencl-coming-to-the-windows-subsystem-for-linux-for-intel-gpus/

### Using packaged secp256k1

If you don't have a system package for `libsecp256k1`:
```
cd mmx-node/secp256k1
./autogen.sh
./configure
make -j8
cd ..
./make_devel.sh -DWITH_SECP256K1=1
```

### OpenCL for Intel iGPUs

Ubuntu 20.04, 21.04
```
sudo apt install intel-opencl-icd
```

Ubuntu ppa for 18.04, 20.04, 21.04
```
sudo add-apt-repository ppa:intel-opencl/intel-opencl
sudo apt update
sudo apt install intel-opencl-icd
```

For older Intel CPUs like `Ivy Bridge` or `Bay Trail` you need this package:
```
sudo apt install beignet-opencl-icd
```

Make sure your iGPU is not somehow disabled, like here for example: https://community.hetzner.com/tutorials/howto-enable-igpu

### OpenCL for AMD GPUs

Linux: \
https://www.amd.com/en/support/kb/release-notes/rn-amdgpu-unified-linux-21-20 \
https://www.amd.com/en/support/kb/release-notes/rn-amdgpu-unified-linux-21-30

```
./amdgpu-pro-install -y --opencl=pal,legacy
```

Windows: https://google.com/search?q=amd+graphics+driver+download

### OpenCL for Nvidia GPUs

Install CUDA, may the force be with you: \
https://www.google.com/search?q=nvidia+cuda+download

Arch Linux:
```
sudo pacman -S nvidia nvidia-utils opencl-nvidia
```
