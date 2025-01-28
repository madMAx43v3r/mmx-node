---
title: Plotting FAQ
description: Frequently asked questions about plotting.
---

## Plotting

### _How do I get started? How do I make/create MMX plots?_

To download the plotter, go to https://github.com/madMAx43v3r/mmx-binaries/tree/master/mmx-cuda-plotter

Before creating a plot, you need to have a farmer key by creating a wallet first. (See below how to create a wallet)

In order to get -f [farmerkey], see output of `mmx wallet keys`.

To plot for testnet11 and later, you don't need to input -p [poolkey] as it has been disabled. To plot for pool plots, you'll need to replot for mainnet later, by specifying -c [contract address] instead of -p [poolkey].

A new plot format has been introduced for testnet11. Farmers who had plotted from testnet8/9/10 had to replot in order to participate in farming for testnet11. New farmer key is also required (66 characters instead of old 96 characters). Old wallet.dat will be reuseable, as well as the 24 seed mnemonic words.

```
Usage:
  mmx_cuda_plot [OPTION...]

  -C, --level arg      Compression level (0 to 15)
      --ssd            Make SSD plots
  -n, --count arg      Number of plots to create (default = 1, unlimited =
                       -1)
  -g, --device arg     CUDA device (default = 0)
  -r, --ndevices arg   Number of CUDA devices (default = 1)
  -t, --tmpdir arg     Temporary directories for plot storage (default =
                       $PWD)
  -2, --tmpdir2 arg    Temporary directory 2 for partial RAM / disk mode
                       (default = @RAM)
  -3, --tmpdir3 arg    Temporary directory 3 for disk mode (default = @RAM)
  -d, --finaldir arg   Final destinations (default = <tmpdir>, remote =
                       @HOST)
  -z, --dstport arg    Destination port for remote copy (default = 1337)
  -w, --waitforcopy    Wait for copy to start next plot
  -c, --contract arg   Pool Contract Address (62 chars)
  -f, --farmerkey arg  Farmer Public Key (33 bytes)
  -S, --streams arg    Number of parallel streams (default = 3, must be >= 2)
  -B, --chunksize arg  Bucket chunk size in MiB (default = 16, 1 to 256)
  -Q, --maxtmp arg     Max number of plots to cache in tmpdir (default = -1)
  -A, --copylimit arg  Max number of parallel copies in total (default = -1)
  -W, --maxcopy arg    Max number of parallel copies to same HDD (default =
                       1, unlimited = -1)
  -M, --memory arg     Max shared / pinned memory in GiB (default =
                       unlimited)
      --version        Print version
  -h, --help           Print help
```


In case of -n [count] != 1, you may press Ctrl-C for graceful termination after current plot is finished,
or double press Ctrl-C to terminate immediately.


Example:
```
./mmx_cuda_plot -f [insert your public farmer key here] -p [insert your pool key here]  -t /mnt/NVME/Temp/ -2 /mnt/ramdisk/ -d /mnt/NVME/Temp/ -C 3 -S 2  -n 18
```

### _How do I make the plotter log into a text file for each plot created?_
At the end of the command, add `2>&1 | tee /home/user/desired_path/$(uuid).log;`

Example:
```
./mmx_cuda_plot -f [insert your public farmer key here] -p [insert your pool key here]  -t /mnt/NVME/Temp/ -2 /mnt/ramdisk/ -d /mnt/NVME/Temp/ -C 3 -S 2  -n 18 2>&1 | tee /home/user/desired_path/(filename).log;
```

### _What is the difference between HDD and SSD plots?_
HDD plots have an extra (huge) table that stores the hash output, so looking up the X values to compute quality, is not needed. As such, HDD plot size is much bigger than SSD (about 2.5x bigger), but SSD plots require much more IOPS and compute to farm effectively.

### _What size plots are supported? If I start plotting now for testnet11, can I use the same plots to farm on mainnet?_
For testnet, k26 and higher. For mainnet, k29 to k32 are supported.

### *What k size and C level should I plot?*

That depends on what kind of hardware you already have. The minimum k size of k29 requires 64GB to plot all in RAM. If you only have 32GB RAM and huge/fast NVMe, you can try plotting in partial RAM. Or you can disk plot k32s.
If you have 128GB or 256GB RAM, you should try to plot the largest k size possible (k30 and k31 all in RAM, respectively).

With the latest new MMX plot format, there is probably 3% difference in non-compressed and C15 (maximum compression). So it's not really worth the extra compute for just a few GB difference. Personally, I think C1-C5 is where it matters. I recommend C3 if you're harvesting with a Pi4 and have farm size < 300TB. If you run dual Xeon v3/v4, you can try C5 or C6. 

### _What are the final plot sizes for k29 to k32, C0 to C15 ?_
Please refer to this spreadsheet, switch to the "Miscellaneous" tab.
https://docs.google.com/spreadsheets/d/1EROCHCRJczwlohun0Wx0d2mzCzDrOMoMm-OEB-3voFE/

### _How much temp space do I need for this MMX new plot format?_
Please refer to this table
https://github.com/voidxno/mmx-node-notes/wiki/New-plot-format-testnet11#plotting

and also this spreadsheet, switch to the "Miscellaneous" tab.
https://docs.google.com/spreadsheets/d/1EROCHCRJczwlohun0Wx0d2mzCzDrOMoMm-OEB-3voFE/

### _I have GPU xxx and I plot kxx plot Cxx in xxx seconds. How fast are others plotting?_
Check out this spreadsheet:
https://docs.google.com/spreadsheets/d/1EROCHCRJczwlohun0Wx0d2mzCzDrOMoMm-OEB-3voFE/

### _I started plotting but got stuck in Phase 1, table 1._
Phase 1, table 1 is very compute heavy and now takes more than 50% of the plot time (if you're plotting all in RAM). In most cases, you'd be limited by the compute capabilities of your GPU. If it doesn't return an error, keep waiting for at least 5 minutes before assuming it's stuck.

### _What are the benefits of plotting and farming higher k sizes?_
A single k31 plot has roughly the size of two k30 plots. It is also twice as likely to find a proof. However, as seen above, plotting a higher k size also requires higher allocation of resources (temp space) and longer plotting time. For a small farm with a total number of plots less than 10000, plotting k30 plots is fine. For a large farm, it is better to plot higher k size plot to minimize the look-up time for finding proofs at every block height.

Lookup times of < 1 sec is considered good and  < 3 sec is acceptable, whereas anything higher than 5 seconds risk losing block rewards.

### _Can I use RAMdisk to make plots? How much temp space is needed to create HDD plots?_
Current plotter use the RAM natively, there is no need to assign RAM disk or tmpfs.

| K-Size | Partial RAM | All in RAM | Final Plot size (GiB) |
| :--- | :---: | :---: | :---: |
| k29 | 33 | 63 | 37.0 |
| k30 | 64 | 117 | 75.3 |
| k31 | 124 | 216 | 155.1 |
| k32 | 240 | 408 | 320.3 |

**Note: The numbers above are just an approximation.**

### _Can I start making NFT plots now for MMX?_
Yes, you can make NFT plots, but you cannot farm them yet. Any NFT plot created now, will not be valid for mainnet, because they will be on a different blockchain and therefore, different contract pool key.