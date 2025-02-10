---
title: Plotting FAQ
description: Frequently asked questions about plotting.
---

## Plotting
### _How do I make the plotter log into a text file for each plot created?_
At the end of the command, add `2>&1 | tee /home/user/desired_path/$(uuid).log;`

Example:
```
./mmx_cuda_plot -f [insert your public farmer key here] -p [insert your pool key here]  -t /mnt/NVME/Temp/ -2 /mnt/ramdisk/ -d /mnt/NVME/Temp/ -C 3 -S 2  -n 18 2>&1 | tee /home/user/desired_path/(filename).log;
```

### _What is the difference between HDD and SSD plots?_
HDD plots have an extra (huge) table that stores the hash output, so looking up the X values to compute quality, is not needed. As such, HDD plot size is much bigger than SSD (about 2.5x bigger), but SSD plots require much more IOPS and compute to farm effectively.

### _What size plots are supported? If I start plotting now for testnet11, can I use the same plots to farm on mainnet?_
For mainnet, k29 to k32 are supported.

### *What k size and C level should I plot?*

That depends on what kind of hardware you already have. The minimum k size of k29 requires 64GB to plot all in RAM. If you only have 32GB RAM and huge/fast NVMe, you can try plotting in partial RAM. Or you can disk plot k32s.
If you have 128GB or 256GB RAM, you should try to plot the largest k size possible (k30 and k31 all in RAM, respectively).

With maximum compression level currently supported by the MMX plotter, there is roughly 3% difference between non-compressed and C15. So it's not really worth the extra compute for just a few GB difference. Personally, I think C1-C5 is where it matters. I recommend C3 if you're harvesting with a Pi4 and have farm size < 300TB. If you run dual Xeon v3/v4, you can try C5 or C6. 

You can run the [benchmark](/guides/mmx-plotter/#benchmark) tool to see what compression level your hardware can handle or read the full [Plotting Guide](/guides/mmx-plotter/).

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

| K-Size | Size (GiB) | Full RAM (GiB) | Partial RAM (GiB) | Disk with Partial RAM (GiB) |
|:-------|:----------:|:--------------:|:-----------------:|:---------------:|
| K29  |  37    |      61    |       39      |      22     |
| K30  |  75    |     116    |       68      |      46     |
| K31  |  155   |     226    |      126      |      88     |
| K32  |  320   |     446    |      246      |     177     |

**Note: The numbers above are just an approximation.**

### _Can I make pooling plots?_
Yes, you can create poolable plots using a plotnft contract. You first have to create the plotnft contract which requires about 0.1 MMX. You can then use the plotnft contract address when creating the plots using the `-c <contract_address>` option. By default the plotnft will be set to solo. There are currently no public pools operating on mainnet.
