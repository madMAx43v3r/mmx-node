---
title: Plotting FAQ
description: Frequently asked questions about MMX plotting.
---

### What plot sizes are supported?

From k29 to k32. Average sizes below, with C0 compression (0%).

| | `k29` | `k30` | `k31` | `k32` |
| :--- | :--- | :--- | :--- | :--- |
| `hdd` | 36.2 GiB | 75.3 GiB | 155.4 GiB | 320.3 GiB |
| `sdd` | 14.7 GiB | 30.3 GiB |  62.3 GiB | 128.3 GiB |

[Article](../../../articles/plotting/plot-format/) with an intro on MMX plot format.

### Difference between HDD and SSD-plots?

Both are just `.plot` files you store somewhere and farm. Per-default named:
```bash frame="none"
plot-mmx-hdd-kxx-cx-<plotid>.plot
plot-mmx-ssd-kxx-cx-<plotid>.plot
```

Difference is that SSD-plots are a type of pre-compressed plots, missing hashes. Reconstructed on-the-fly when farming, but requires very high disk IOPS. Storing them on regular spinning HDD will not work. It balances cost/reward potential between SSD and HDD storage (2.5x SSD advantage, but more expensive media).

Given identical k-size, SSD-plots has same effective space on network as HDD-plots.

Recommended to use C0 for compression (0%) on SSD-plots, because of the high IOPS.

### What k-size and C-compression should I plot?

That depends on your hardware. Minimum k-size of k29 requires 64GB to plot all in RAM. If you only have 32GB RAM and large/fast NVMe, you can try plotting with partial RAM. Or you can disk plot k32. If you have 128GB or 256GB RAM, you should try to plot the largest k-size possible (k30, k31, all in RAM).

With maximum supported compression level C15, there is about 4% difference between C0 non-compressed and C15. You will have to evaluate yourself if those few extra GB are worth the extra farming compute. Those 1-4% are a long way from the +200% with other Proof of Space. Some think C1-C5 is where it matters. Others use C0 as a principle, or peace of mind. Your harvesting CPU power and farm size can also be a guide. Maybe ~C3 with low-power CPU, and ~try C5/C6 with high-power CPU.

You can run the [benchmark](../../../guides/mmx-plotter/#benchmark) tool to see what compression level your hardware can handle or read the full [Plotting Guide](../../../guides/mmx-plotter/).

### Can I create plots for pooling?

Yes, you can create poolable plots using a PlotNFT contract. You first have to create the PlotNFT contract, which requires about 0.1 MMX. If you do not have 0.1 MMX currently, ask in Discord. Somebody will probably be nice. You then use the PlotNFT contract address when creating the plots with the `-c <contract address>` option. By default the PlotNFT will be set to solo.

Read more about pooling under [Creating Plots](../../../guides/mmx-plotter/#creating-plots) in Plotting Guide, and [Pooling](../../../software/cli-commands/#pooling) in CLI Commands.

### Plot speed, temp space and other stats?

Here are a few resources for some stats.

**Final plot sizes for k29/k32 and C0/C15:**\
Miscellaneous tab of [this Google spreadsheet](https://docs.google.com/spreadsheets/d/1EROCHCRJczwlohun0Wx0d2mzCzDrOMoMm-OEB-3voFE/).

**Temp space and disk writes needed for plotting:**\
Plotting section of [this article](../../../articles/plotting/plot-format/#plotting).\
Miscellaneous tab of [this Google spreadsheet](https://docs.google.com/spreadsheets/d/1EROCHCRJczwlohun0Wx0d2mzCzDrOMoMm-OEB-3voFE/).

**How fast are others plotting:**\
Look in tabs of [this Google spreadsheet](https://docs.google.com/spreadsheets/d/1EROCHCRJczwlohun0Wx0d2mzCzDrOMoMm-OEB-3voFE/).

### Benefits of plotting and farming higher k-size?

A single k31 plot has roughly the size of two k30 plots. It is also twice as likely to find a proof. However, as seen above, plotting a higher k-size also requires higher allocation of resources, and longer plotting time. For a small farm with a total number of plots less than 10000, plotting k30 is fine. For a large farm, it is better to plot higher k-size plots to minimize the lookup time for finding proofs at every block height.

Lookup times of < 1 sec is considered good, < 3 sec is acceptable, whereas higher than 5 sec risks losing block rewards.

### Plotting stuck in Phase 1, [P1] Table 1?

Maybe not, be patient. Phase 1, table 1 is very compute heavy and takes more than 50% of plot time (if full RAM plotting). In most cases you are limited by the compute capabilities of your GPU. If no error returned, wait at least 5 minutes before assuming it is stuck.

### Can I use RAMdisk to make plots?

Plotter uses the RAM directly, no need to involve a RAMdisk.

### How much temp space is needed?

| | Size | Full RAM | Partial RAM | Disk/Partial RAM |
| :--- | :--- | :--- | :--- | :--- |
| k29 | 37 GiB | 61 GiB | 39 GiB | 22 GiB |
| k30 | 75 GiB | 116 GiB | 68 GiB | 46 GiB |
| k31 | 155 GiB | 226 GiB | 126 GiB | 88 GiB |
| k32 | 320 GiB | 446 GiB | 246 GiB | 177 GiB |

:::note[Note]
Numbers above are just an approximation.
:::

### How to make plotter log each plot created?

At the end of the command, add:\
 `2>&1 | tee /home/user/desired_path/filename.log`

Example (Linux):
```bash frame="none"
./mmx_cuda_plot <options> 2>&1 | tee /home/user/desired_path/filename.log
```
