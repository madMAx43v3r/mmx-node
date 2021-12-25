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

Tokes can either be traditionally issued by the owner, or they can be owner-less and created by staking another token over time, in a decentralized manner governed by the blockchain.

In the future it is planned that anybody can create their own token on MMX using a simple web interface.

In addition the first application for MMX will be a decentralized exchange where users can trade MMX and tokens.

## CLI

To use the CLI:
```
cd mmx-node
. ./activate
```

The node needs to be running, see below how to start it.

To check your balance: `mmx wallet show`

To show the first 10 addresses: `mmx wallet show 10`

To get a specific receiving address: `mmx wallet get address [index]`

To send coins: `mmx wallet send -a <amount> -t <address>`

To use a non-default wallet specify `-j <index>` with above commands.

To check on the node: `mmx node info`

To check on a transaction: `mmx node tx <txid>`

To check the balance of any address: `mmx node balance <address>`

To dump a transaction: `mmx node get tx <txid>`

To dump a block: `mmx node get block <height>`

To dump a block header: `mmx node get header <height>`

To force a re-sync: `mmx node sync`

To show some known peers: `mmx node get peers [count]`

## Running a Node

First perform the installation and setup steps below.

To run a node for `testnet1`
```
mmx_node -c config/test1/ config/local/
```

To run a node in the background you can enter a `screen` session:
```
screen -S node
(start node as above)
<Ctrl+A> + D (to detach)
screen -r node (to attach again)
```

To disable the `TimeLord` specify `--timelord 0` on the command line. If you have a slow CPU this is recommended.

To re-sync starting from a specific height: `--Node.replay_height <height>`

## Setup

First finish the installtion step below.

To continue enter the CLI environment:
```
cd mmx-node
. ./activate
```

### Creating a Wallet

```
mmx wallet create [-f filename]
```

The file name argument is optional, by default it is `wallet.dat`, which is already included in the default configuration.

To add more wallets add the paths to `key_files+` array in `config/local/Wallet.json`.

## Plotting

To get the farmer and pool keys for plotting:
```
mmx wallet keys [-j index]
```

The node needs to be running for this command to work. (`-j` to specify the index of a non-default wallet)

Then use my plotter with `-x 11337` argument: https://github.com/madMAx43v3r/chia-plotter

The minimum K size for mainnet will probably be k30, for testnets it is k26. The plots from testnets can be reused for mainnet later.

To add a plot directory add the path to `plot_dirs` array in `config/local/Harvester.json`.

## Installation

To install the dependencies:
```
sudo apt update
sudo apt install cmake build-essential libsecp256k1-dev libsodium-dev zlib1g-dev ocl-icd-opencl-dev clinfo
```

OpenCL provides faster and more effient VDF verification using an integrated or dedicated GPU.
A standard iGPU found in Intel CPUs with 192 shader cores is plenty fast enough.
If you dont have a fast quad core CPU (>3 GHz) or higher core count CPU, it is required to have a GPU with OpenCL support.

### VNX Middleware

`mmx-node` depends on my middleware `VNX` which is free to use but closed source.
It provides the communication over network, the storage on disk as well as RPC support and publish/subscribe.

Ubuntu 20.04
```
wget https://github.com/automyinc/vnx-base/raw/master/x86_64/vnx-base-1.9.8-x86_64-ubuntu-20.04.deb
sudo dpkg -i vnx-base-1.9.8-x86_64-ubuntu-20.04.deb
```

Ubuntu 18.04
```
wget https://github.com/automyinc/vnx-base/raw/master/x86_64/vnx-base-1.9.8-x86_64-ubuntu-18.04.deb
sudo dpkg -i vnx-base-1.9.8-x86_64-ubuntu-18.04.deb
```

Windows: coming soon, once I have a build machine

### Building

```
git clone https://github.com/madMAx43v3r/mmx-node.git
cd mmx-node
git submodule update --init --recursive
./make_devel.sh
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
