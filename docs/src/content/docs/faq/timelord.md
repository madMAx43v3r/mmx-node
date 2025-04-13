---
title: TimeLord FAQ
description: Frequently asked questions about MMX timelord.
---

### Do I need to run a timelord?

No. Running a timelord is optional (default is disabled).

Not needed to run a fully functional node. Blockchain, as a whole, only need one active timelord to move forward. A few more, spread around, is preferred for redundancy and security.

You will not get any more or less block rewards by running a timelord on your farming node.

If you want to contribute by running one, check [requirements](../../articles/timelord/timelord-optimizations/#requirements) in this [article](../../articles/timelord/timelord-optimizations/). Enable timelord in WebGUI, or set `true` in `config/local/timelord` file. Check that running, and speed, in NODE / LOG / TIMELORD tab in WebGUI. Probably lower than NODE / VDF Speed, unless you are the fastest timelord.

### Rewards for running a timelord?

Yes, if you are the fastest.

Only fastest timelord, at any time, produces VDF for block being created. An on-chain reward of 0.2 MMX is given every 20 blocks to fastest timelord. Potentially 86.4 MMX per day.

More info about [logic](../../articles/timelord/timelord-optimizations/#logic) and how to know you are [fastest timelord](../../articles/timelord/timelord-optimizations/#fastest-timelord) in this [article](../../articles/timelord/timelord-optimizations/).

### I want to be the fastest timelord

Read following articles, [TimeLord Optimizations](../../articles/timelord/timelord-optimizations/) and [TimeLord Predictions](../../articles/timelord/timelord-predictions/).

Join the [`#mmx-timelord`](https://discord.com/channels/852982773963161650/1026219599311675493) channel on Discord.

### Why not a GPU/FPGA/ASIC timelord?

Timelord logic will only use CPU, not GPU.

GPU is great for parallel SHA256 calculations, beating CPU in both speed and efficiency. GPU is used for verify VDF operation on a node, if available.

For a single SHA256 calculation, CPU's SHA extensions will beat GPU on speed. As timelord SHA256 workload is not parallelizable, CPU wins the serial SHA256 race.

Feedback welcome on other contenders. As of now, nothing observed beating a high-GHz CPU with SHA extensions (optimized silicon circuits inside CPU). Too low speed (GHz) on FPGA, work not parallelizable. Prohibitive cost to produce a high-GHz ASIC that beats Intel/AMD optimized silicon.
