---
title: Node FAQ
description: Frequently asked questions about MMX node.
---

### How do I get started with a node

[Download and install](../../../guides/installation/), then [get started](../../../guides/getting-started/).

### "Faulting module" in Windows

Make sure you have latest Microsoft Visual C++ Redist installed:
- [Microsoft Visual C++ Redistributable](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist)
- [Microsoft Visual C++ Redistributable for Visual Studio](https://visualstudio.microsoft.com/downloads/)

### High CPU usage, even synced

I'm getting high CPU usage, even when my node is synced to latest blockchain height.

Make sure you are performing VDF verification [with GPU/iGPU](../../../guides/optimize-vdf/) (more info in [VDF FAQ](../../../faq/vdf/)).

On Linux make sure user has access to GPU/iGPU through  the `video` and or `render` groups:
```bash frame="none"
sudo adduser $USER video
sudo adduser $USER render
```

Check if [timelord](#how-do-i-disable-timelord) has been enabled by accident.

### How do I disable timelord?

Timelord should be [disabled by default](../../../faq/timelord/). You can check if it is enabled by looking under SETTINGS in WebGUI. Or check the `config/local/timelord` file.

### What is VDF height?

I see two different heights for blocks, both in logs and block explorer. Which is right?

Both. Each block in MMX has two heights:
- Block `Height` (always the lower value, Network Height)
- `VDF Height` (always the higher value)

In nearly all cases, block height is what is relevant for normal usage.

A block is prepared by timelord every 10 sec, and given an VDF height.

About 2% of the time a non-TX block is the result, because of no winning proof. In that case VDF height still increases, but block height stands still. Chain will still adjust itself to 8640 blocks each day, with ~10 sec block interval.

Can look at block height as the 'public facing' height, while VDF height is an 'internal clock' height.

One example of VDF height usage is FARMER / PROOF tab. You submit proofs that passed plot filter to an VDF height. Block height is unknown at this point. It will depend on potential non-TX blocks before it becomes a block in chain. If your proof [was best](../../../faq/node/#a-plot-with-lower-score-won-the-block), it will be listed with block height under FARMER / BLOCKS tab. Can be tricky to correlate at times.

[Time Calculator](../../../tools/time-calculator/) can help show the relation, and lookup or estimate time and date for heights.

### Warning when using mmx CLI command

I'm getting a warning when trying one of the `mmx ...` commands:\
`[Proxy] WARN: connect() failed with: xxx (localhost:1133x)`

Make sure node is running. Except `mmx wallet create`, all other commands require node running.

In Windows, start the MMX CMD shortcut from Start Menu.\
If Linux binary package, enough to open a terminal.

If compiled from source on Linux (before using CLI commands):
```bash frame="none"
cd mmx-node
source ./activate.sh
```

### List of mmx CLI commands

Overview of [CLI commands](../../../software/cli-commands/).

### How is network difficulty calculated?

Look Blockhain and Challenge Generation sections of [whitepaper](../../../articles/general/mmx-whitepaper/).

### How do I start farming plots created?

I've just created some plots. How do I start farming them?

Through SETTINGS in WebGUI, you have a Plot Directory input field below the Harvester section. Input the directory with the plots you have created, press ADD DIRECTORY button.

Alternatively you can edit the `config/local/Harvester.json` file:
```json
{
  "num_threads": 32,
  "recursive_search": true,
  "reload_interval": 3600,
  "plot_dirs": [
    "E:/MMXplots/",
    "F:/MMXplots/",
    "/mnt/Harddisk123/MMXplots/",
    "/mnt/Harddisk456/MMXplots/"
  ]
}
```

Only use forward slash `/`, even if Windows. Last entry is not followed by a comma `,`.

### A plot with lower score won the block?

Score indicates how close your plot has proofs for the challenge. The lower the score, the better the proof.

### Do I need to open ports to communicate?

Not really. But you will help the network by allowing incoming connection. If possible, enable port forwarding on `11337` (TCP).

Per-default, automatic UPnP port forwarding is enabled. You can disable it through SETTINGS in WebGUI. Or set `"open_port": false,` in `config/local/Router.json` file.

### How do I set up remote harvesters?

I've got several computers. Full node on one, and a few remote harvesters. How do I set them up?

Look [Remote Services](../../../guides/remote-services/) guide.

### My node says it has forked from network

Should rarely happen with mainnet. If forked, and not automatically getting back to synced. You can try to revert DB to a height about 1000 blocks before node got stuck.

Through SETTINGS in WebGUI, try the `Revert DB to height` option.\
If using CLI, try the `mmx node revert <height>` command.

### I suspect my DB is corrupted

:::note[Note]
Make sure you really want to try syncing from scratch. Will take some time
:::

:::danger[Warning]
Make sure you only delete the DB directory. Not directories with important information like wallets, which you already should have [secure backups](../../../articles/wallets/wallets-mnemonic-passphrase/#backup) of.
:::

To force a 100% sync from scratch. Stop the node, delete following directory:
- Windows: `C:\Users\<user>\.mmx\mainnet\db\`
- Linux: `~/.mmx/mainnet/db/`
- Linux: `~/mmx-node/mainnet/db/`

Start the node again. A clean sync will start from height 0.

### "Warning! UnhandledException" in Windows

If starting MMX Node fails with a `Warning! UnhandledException` dialog box, containing `user.config`, `value 0x00`, `invalid character`, `Line 9`, `position 1`. Try the following:
- Restart Windows, to make sure no hanging MMX Node processes
- Navigate to: `C:\Users\<user>\AppData\Local\madMAx43v3r\MmxGui.exe_Url_xxx\0.0.0.0\`
- Rename `user.config` file to `_backup01_user.config` (just in case)
- Start MMX Node, new `user.config` created

File only contains Windows GUI app specific configuration. If fixed, go through Windows specific options in MMX Node and check that ok. Probable cause for this looks to be unclean shutdown of MMX Node, corrupting file. Maybe a Windows bluescreen.

### "CUDA failed" messages

If you get `CUDA failed` in logs, maybe with `Proof verification failed`. MMX Node has problems interfacing with the Nvidia CUDA library on your OS.

MMX Node will try to use CUDA (Nvidia GPU) for farming and proof verify, if present. For VDF verify it tries to use OpenCL (NVidia/AMD/Intel GPU). Both will fallback to CPU if not present.

If problems with CUDA, try to disable it under SETTINGS in WebGUI (CUDA / Enable CUDA compute). Or set `"enable": false,` in `config/local/cuda.json` file.

You can also try to update or match other versions of CUDA platform on your OS. The interface between compiled programs and CUDA versions are fickle.

In most cases farming and proof verify is light work for a CPU. Unless you have a very large farm, or too high compression on your plots.
