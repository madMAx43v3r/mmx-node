---
title: MMX Node FAQ
description: Frequently asked questions about using MMX Node.
---
## MMX Node

### _How do I get started? Where do I download the client?_
https://docs.mmx.network/guides/installation/

https://docs.mmx.network/guides/getting-started/

### _I started MMX-node in Windows but getting error message "Faulting module path: C:\Program Files\MMX Node\gui\cefsharp\libcef.dll" or something like that. What's wrong?_
Are you sure you have read the installation wiki linked above? It says to update and install the latest Microsoft Visual C++ Redistributable.

### _Okay I have synced up to the latest blockchain height and I still see high CPU usage?_
Check that VDF verification is done via GPU. Disable Timelord when enabled by accident.

On Linux make sure to be in the `video` and or `render` group (depends on distribution) to be able to access a GPU:
```
sudo adduser $USER video
sudo adduser $USER render
```

### _The timelord is taking a huge chunk of my CPU usage, how do I disable it?_
You can disable it in GUI settings. (It's disabled by default now)

Alternatively, create a file named `timelord` (all lower case) in `config/local/`. Open the file in text editor and type `false`. Or simply run `echo false > config/local/timelord` and then restart the node.

For Windows, put the file in `C:\Users\name\.mmx\config\local\`

**Do not edit any files in `Program Files`**

### _I tried to run one of the `mmx ....` commands and I'm getting `[Proxy] WARN: connect() failed with: 10061 (localhost:11331)` error. What's wrong?_
Before running a `mmx ...` command in a new terminal, you need to `source ./activate.sh` inside the `mmx-node` directory first, and the node needs to be running (except for `mmx wallet create`).
Alternatively, you can open terminal window from MMX-node folder/shortcut from the `Start Menu` with source pre-activated.

### _Where can I find a list of CLI commands for MMX node?_
https://docs.mmx.network/software/cli-commands/

### _How is network difficulty calculated?_

Difficulty is adjusted by a defined (and enforced via consensus) exponential low-pass filter, such that the average proof score is 8192, which will yield 8 proofs per block on average.

### _I left my node running for several hours and when I came back, it's saying that I have forked from the network and there is no way of recovering/syncing to the current height. How can I fix this?_

You can revert to a lower height in the GUI settings page now. The height should be about 1000 blocks before the node gets stuck.

Alternatively, run `mmx node revert [height]` while the node is running. 

### _I copied the `block_chain.dat` file from another machine and now my node is saying I have forked from the network and there is no way of recovering/syncing to the current height. Or I suspect my database is corrupted. How can I fix this?_
Stop the node, then delete the `db` folder in `testnetX` and restart the node. Note that this will re-build the database and take some time.

### _I have tried replay_height and deleting the `db` folder and my node still won't sync. What do I do?_
Stop the node, then delete the `testnetX` folder and restart node. This will sync from scratch and might take some time.

### _I've just started plotting. Can I farm my plotted space right away? How do I add my plots so the harvester reads my plots properly?_
Adding plot directories is now possible in the GUI settings page.

Alternatively, open `config/local/Harvester.json` with a text editor. Follow the syntax below:
```
{
  "reload_interval": 3600,
  "farm_virtual_plots": true,
  "recursive_search": false,
  "plot_dirs": [
    "E:/MMXPlots",
    "E:/Test Folder/",
    "E:/Folder/Subfolder",
    "/mnt/Seagate13/MMXPlots/",
    "/mnt/Seagate42/MMXPlots/"
  ]
}
```
You may also put everything on 1 line if it helps you to see all the plot drives/folders:
```
"plot_dirs": ["D:/", "E:/", "F:/", "G:/", "H:/", "I:/", "J:/", "K:/", "L:/"]
```
Backward slash "`\`" is not supported, so forward slash "`/`" must be used.
Note that the last entry is not followed by a `,`

### _My node returned `[Router] INFO: Broadcasting proof for height 65105 with score 25761`. Why did a plot with lower score eventually win that block?_
The score is actually an indication of how close your plot has proofs for the challenge. The lower the score, the better the proof is.

### _Do I need to open some ports to allow MMX to communicate with other peers?_
You don't have to. But if you have fast internet connection and feel like helping out the network by allowing incoming connections, you can enable port forwarding on TCP 11337 for mainnet.

UPnP automatic port forwarding is now enabled by default, however it can disabled in GUI settings.

### _My setup is a complicated mix of several computers, where I run a full node on one computer and a few remote harvesters. How do I set them up?_
https://docs.mmx.network/guides/remote-services/
