---
title: TimeLord Optimizations
description: How to optimize MMX timelord speed
prev: false
next: false

articlePublished: 2023-07-11
articleUpdated: 2025-03-17

authorName: voidxno
authorPicture: https://avatars.githubusercontent.com/u/53347890?s=200
authorURL: https://github.com/voidxno
---

Running an MMX TimeLord is optional (default is disabled). Not needed to run a fully functional node.

Blockchain, as a whole, only need one active timelord to move forward. A few more, spread around, is preferred for redundancy and security.

If you want to contribute by running one, check [requirements](#requirements) below. Enable timelord in WebGUI, or set `true` in `config/local/timelord` file. Check that running, and speed, in NODE / LOG / TIMELORD tab in WebGUI. Probably lower than NODE / VDF Speed, unless you are the fastest timelord.

No more is needed. Standard Linux compile and Windows binaries gives good performance for a timelord. Unless you want to optimize for fastest timelord, or the fun of it.

_**IMPORTANT:**_ _Trying to be fastest TimeLord is not easy. YOU need to decide if worth it. **Overclock at your own risk, CPU can degrade**. Information shared here, is to **best of my knowledge as of Mar2025**._

## TLDR;

I want to run [fastest timelord](../../../articles/timelord/timelord-predictions/):
* Intel Ultra 200S series, E-core (15th-gen, Arrow Lake)
* Use [Linux](#linux-or-windows) and [GCC](#optimize-compiler)
* Compile with [AVX](#optimize-compiler-options-avx-vs-sse42)
* Compile with [15th-gen E-core optimized code](#optimize-source-code-architecture)
* [Isolate](#optimize-isolate-thread) 1x CPU core to timelord process thread
* Have a GPU/iGPU verify VDF
* [Clock 1x CPU core](#optimize-cpu-speed) as high as possible

_NOTE: Optimize and **overclock at your own risk**._

## Logic

A timelord performs a very simple mathematical operation (SHA256, Secure Hash Algorithm). It is performed recursively and cannot be parallelized. Previous result is needed, before repeating, as fast as possible. Like a very specific mathematical single-threaded benchmark.

Timelord runs 1x of these operations, creating a VDF (verifiable delay function) stream.

Only fastest timelord, at any time, produces VDF for block being created. A reward of 0.2 MMX is given every 20 blocks to fastest timelord. Potentially 86.4 MMX per day. Be aware of [overtaking caveat](#fastest-timelord) for fastest timelord.

## Requirements

**CPU:** Intel or AMD (w/ SHA extensions).<br>
**Model:** Intel 11th-gen (Rocket Lake), AMD Zen, or later (a few exceptions).<br>
**GPU/iGPU:** Any compatible (offload verify VDF).

**Linux:** `grep -o 'sha_ni' /proc/cpuinfo`, empty if not available.<br>
**Windows:** CPU-Z (Instructions) or HWiNFO64 (Features), look for `SHA`.

You can run timelord on a CPU without SHA extensions. Will fallback to AVX2. In reality SHA extensions are needed. SHA256 calculations are ~5-10x faster with SHA vs AVX2.

## Why not GPU/FPGA/ASIC

Timelord logic will only use CPU, not GPU.

GPU is great for parallel SHA256 calculations, beating CPU in both speed and efficiency. GPU is used for verify VDF operation on a node, if available.

For a single SHA256 calculation, CPU's SHA extensions will beat GPU on speed. As timelord SHA256 workload is not parallelizable, CPU wins the serial SHA256 race.

Feedback welcome on other contenders. As of now, nothing observed beating a high-GHz CPU with SHA extensions (optimized silicon circuits inside CPU). Too low speed (GHz) on FPGA, work not parallelizable. Prohibitive cost to produce a high-GHz ASIC that beats Intel/AMD optimized silicon.

## Optimize TimeLord

Standard Linux compile and Windows binaries gives good performance for a timelord. Still important to tune surrounding environment. Either aiming for fastest timelord rewards, or just the challenge.

Information and numbers in this article might be superseded. Sections below are known info at date of publish. To help get started or give ideas. Not the absolute answer. Probably angles not discovered yet, and other ways to go about it.

You do not need to complicate it like below. Try to run a timelord. Measure speed. Try out a tip. Measure if faster.

Discuss and share in [`#mmx-timelord`](https://discord.com/channels/852982773963161650/1026219599311675493) channel on Discord. No requirement to divulge all your secrets. But a good place to get tips, or kickstart new ideas.

_NOTE: Optimize and overclock at your own risk._

### Test Environment

**CPU:** Intel Ultra 200S series, 265K (15th-gen, Arrow Lake)<br>
**GPU:** Nvidia RTX 4060 Ti 8GB (AD106)<br>
**OS:** Ubuntu 24.10 (Oracular Oriole)<br>
**OS:** Windows 10 (22H2)<br>
**mmx-node:** v1.1.7 (+ [AVX](#optimize-compiler-options-avx-vs-sse42), + [optimized code](#optimize-source-code-architecture))

Most instructions below are transferable to AMD.

### Measuring Speed

Timelord speed is measured in MH/s (million hashes per second).

Current blockchain network speed, fastest timelord, is found in NODE / VDF Speed in WebGUI. Your own timelord speed is found under NODE / LOG / TIMELORD tab in WebGUI.

To make it easier to measure own improvements, baseline numbers, compare with others. It is recommended to measure MH/s speed per 0.1 GHz (**MH/s/0.1GHz**). Yes, absolute speed is the end goal. But timelord speed, at least observed for now, is linear given CPU GHz. A 2-step process is recommended:
1. Optimize for best possible MH/s/0.1GHz
2. Clock 1x CPU core as high as possible

In this case the Intel 15th-gen has had an E-core locked to 3.3 GHz (could be lower/higher, not important). We already know from [TimeLord Predictions](../../../articles/timelord/timelord-predictions/) that E-core is the best option. Not always been so. Previous generations had P-core as best, with hyperthreading and E-cores disabled (more on that [later](#optimize-cpu-speed)). Goal is to make optimization measurements easier and controlled.

Intel 15th-gen (v1.1.7, + [AVX](#optimize-compiler-options-avx-vs-sse42), + [optimized code](#optimize-source-code-architecture)):

| Environment | Measured | Locked Speed | Measured Per Unit |
| :--- | :--- | :--- | :--- |
| Ubuntu/gcc14 | 44.60 MH/s | /33 (3.3 GHz) | **1.351** MH/s/0.1GHz |
| Ubuntu/Clang19 | 44.28 MH/s | /33 (3.3 GHz) | **1.342** MH/s/0.1GHz |
| Windows/VC++ | 41.49 MH/s | /33 (3.3 GHz) | **1.257** MH/s/0.1GHz |

These numbers represent absolute speed per 0.1 GHz, given the environment and tuning. Easy to compare against yourself or others. Top speed after that is dependent on how high you can clock 1x CPU core (more on that [later](#optimize-cpu-speed)). In this case, 1x E-core stable at **4.6** GHz, would give a timelord speed of ~**62.2** MH/s.

As a sidenote. P-core numbers for 12/13/14th-gen Intel gives exact same performance per 0.1 GHz. Basically, no IPC (instructions per clock) uplift for SHA extensions between them (for this specific use-case). But 14th-gen have potential to clock highest.

Testing of AMD Zen4-core (7000-series), and Zen5-core (9000-series) gives **0.755** and **0.715** MH/s/0.1GHz. Yes, a degradation in efficiency. Probably an architectural decision.

Intel's 15th-gen E-cores efficiency increase was a surprise (1.352, vs 0.975 before). Making it the best known choice for now. Previous generations had Intel/AMD much closer (for this specific use-case). In the end, an overclocking race.

Fastest timelord speeds observed on testnets (as of **Mar2025**):

| Continuous | Peak |
| :--- | :--- |
| ~**74.5** MH/s | ~**75.2** MH/s |

### Linux or Windows

Numbers above, in previous releases and CPU generations, have switched between Linux or Windows binary being fastest. With current source code, Linux (gcc14) looks to have the edge.

Instructions shown in sections below are done on Linux. But most aspects are applicable to Windows too.

Linux distribution and kernel often have an effect on different types of workloads. When it comes to timelord logic, not much observed. Logic for creation of a VDF stream is very small. A few instructions repeated in a CPU core.

### Optimize: Establish defaults

Follow [mmx-node](../../../guides/installation/#building-from-source) build from source installation for Linux (in this case Ubuntu, with default compiler gcc14). Get mmx-node up and running. Enable timelord in WebGUI, or set `true` in `config/local/timelord` file. Find and [lock timelord process thread](#optimize-isolate-thread) to an E-core (`taskset`).

Let it run for a while. Check average speed in NODE / LOG / TIMELORD tab in WebGUI. With this setup:

| Environment | Measured | Locked Speed | Measured Per Unit |
| :--- | :--- | :--- | :--- |
| Ubuntu/gcc14 | 43.16 MH/s | /33 (3.3 GHz) | **1.308** MH/s/0.1GHz |

### Optimize: Compiler

Compiler has an effect on how good source code is translated to binary objects. Default for Ubuntu 24.10 is GCC (GNU Compiler Collection), or gcc14. An alternative is Clang (LLVM), or Clang19. There are others. Has varied if Clang or gcc do a better job.

One way to install, enable and compile with Clang19:

```bash frame="none"
sudo apt install clang lld libomp-dev
```
```bash frame="none"
export CC=/usr/bin/clang-19
export CPP=/usr/bin/clang-cpp-19
export CXX=/usr/bin/clang++-19
export LD=/usr/bin/ld.lld-19
```
```bash frame="none"
./clean_all.sh
./make_devel.sh
```

_NOTE: You need to perform `export` statements in terminal environment before compile, or gcc14 will be used._<br>
_NOTE: When you switch compiler, or compiler options. Always do `./clean_all.sh` before new compile._<br>
_NOTE: You will get a lot of unused `-fmax-errors=1` warnings. Just ignore, or remove from compiler options._

Default Clang19 compile (`./make_devel.sh`):

| Environment | Measured | Locked Speed | Measured Per Unit |
| :--- | :--- | :--- | :--- |
| Ubuntu/Clang19 | 42.95 MH/s | /33 (3.3 GHz) | **1.301** MH/s/0.1GHz |

Small, but noticeable degrade from gcc14's **1.308** MH/s/0.1GHz. We'll switch back to gcc14 going forward.

### Optimize: Compiler options

Compiler options can have a big effect on how source code is transformed to a binary object. Often focus is on speed vs size. Several options have an effect on timelord logic. Much have been tried with both gcc and Clang.

For now, gcc14 with default options in `./make_devel.sh` gives best performance.

Some elements to experiment with (`./make_devel.sh`):
* Switch between `Release` and `RelWithDebInfo` (`-DCMAKE_BUILD_TYPE`)
* Remove `-fno-omit-frame-pointer` (`-DCMAKE_CXX_FLAGS`)
* Add `-march=native` (`-DCMAKE_CXX_FLAGS`)
* Variants of `-O` optimization option (`-DCMAKE_CXX_FLAGS`)

There are others. Look up optimization in relevant compiler documentation.

_NOTE: When you switch compiler, or compiler options. Always do `./clean_all.sh` before new compile._<br>

### Optimize: Compiler options (AVX vs SSE4.2)

Current default compile combines the usage of SHA extensions and SSE4.2 instructions. Raising the SSE4.2 baseline to AVX instructions gives about ~1% boost on 15th-gen Intel. Has to do with compiler using identical AVX versions of certain SSE4.2 instructions. Though, on 11th-gen Intel this looks to degrade performance (better with SSE4.2).

To implement AVX vs SSE4.2 baseline (`./CMakeLists.txt`):

Change `-msse4.2` to `-mavx` on two lines (Linux compile part):
```cmake
set_source_files_properties(src/sha256_ni.cpp PROPERTIES COMPILE_FLAGS "-mavx -msha")
set_source_files_properties(src/sha256_ni_rec.cpp PROPERTIES COMPILE_FLAGS "-mavx -msha")
```

Add two lines (Windows compile part):
```cmake
set_source_files_properties(src/sha256_ni.cpp PROPERTIES COMPILE_FLAGS "/arch:AVX")
set_source_files_properties(src/sha256_ni_rec.cpp PROPERTIES COMPILE_FLAGS "/arch:AVX")
```

Easier to see location in closed [PR#210](https://github.com/madMAx43v3r/mmx-node/pull/210/files) request.

Small, but real jump from gcc14's **1.308** MH/s/0.1GHz:

| Environment | Measured | Locked Speed | Measured Per Unit |
| :--- | :--- | :--- | :--- |
| Ubuntu/gcc14 | 43.58 MH/s | /33 (3.3 GHz) | **1.320** MH/s/0.1GHz |

_NOTE: For now, official releases will keep having SSE4.2 as baseline._

### Optimize: Source code

One thing to be aware of is that we want to optimize a tiny part of whole mmx-node. Even a tiny subset of whole timelord logic. The calculation of a VDF stream. Performed through `hash_t TimeLord::compute(...)` (`/src/TimeLord.cpp`) calling `recursive_sha256_ni(...)` (`/src/sha256_ni_rec.cpp`). We do not care about the rest. As long as this part goes as fast as possible. Unless surrounding elements has an effect. Not observed for now.

That part of source code is already written to be fast when translated to binary objects by compiler (inline, intrinsics, asm).

Several iterations were made for it to [end up like that](https://github.com/voidxno/fast-recursive-sha256/blob/main/OPTIMIZE.md). Still, this is the place to adjust source code if you think there is a way to optimize it even more.

To illustrate, the following file [sha256_ni_rec_sample.cpp](/resources/code/timelord/optimize/sample/sha256_ni_rec_sample.cpp) contains 5x optimization samples related to static 32 bytes nature of implementation. Intel 15th-gen E-core architecture edition below implements `SAMPLE02` and `SAMPLE04`. If all 5x had been used, reduced effect. Though, would then be possible to remove data for index `8-15` in `K64` array. Independent of that, possible to remove, rearrange variables and order of lines. While still maintaining algorithm.

End result is what counts. However you would like the code to look, and what you think works. You are a slave to compiler optimizing code presented. Unless you really want to dig into source code, use architecture edition below.

### Optimize: Source code (architecture)

Although current [sha256_ni_rec.cpp](https://raw.githubusercontent.com/madMAx43v3r/mmx-node/refs/heads/master/src/sha256_ni_rec.cpp) is very fast. Not impossible certain CPU architectures could be faster with code organized other ways. Found that out for Intel 15th-gen E-core. Such specific code will not be implemented directly in mmx-node. Making it public here for others to use.

| Architecture | File |
| :--- | :--- |
| Intel 15th-gen E-core | [sha256_ni_rec.cpp](/resources/code/timelord/architecture/Intel_15th-gen_E-core/sha256_ni_rec.cpp) |

Download 15th-gen E-core file, overwrite current in mmx-node installation. Update file timestamp, and compile:

```bash frame="none"
touch ./src/sha256_ni_rec.cpp
./make_devel.sh
```

Real jump from standard optimized **1.320** MH/s/0.1GHz:

| Environment | Measured | Locked Speed | Measured Per Unit |
| :--- | :--- | :--- | :--- |
| Ubuntu/gcc14 | 44.60 MH/s | /33 (3.3 GHz) | **1.351** MH/s/0.1GHz |

### Optimize: Isolate thread

Timelord logic has 1x process thread. It wants 100% of 1x CPU core to calculate VDF stream. Goal is to create an environment that makes this 1x process thread run with high GHz continuously.

One way, and valid strategy, is to let the OS process scheduler do its job (Linux or Windows). Distribute and use resources as best possible, depending on requirements and state of system. Maybe tune some aspects of OS, together with BIOS adjustments to clock CPU as high as possible. Gives great results. All modern CPUs have logic to boost individual CPU cores in combination with OS scheduler, power management and other logic.

Another, more manual way, is to dedicate a specific CPU core to the 1x timelord process thread. Locking OS and other processes away from it. In this case an Intel CPU with 8x P-cores, numbered 0 to 7. With E-cores following after that. Going to dedicate E-core 8 to timelord process thread. One way to achieve it (Linux, Ubuntu):

* Force OS process scheduler to not use core 8. Add `isolcpus=8` to `GRUB_CMDLINE_LINUX` (`/etc/default/grub`). Easily observed through `htop` and CPU core utilization.
* When timelord is up and running, you should have 1x process thread at 99/100% with command name of 'TimeLord':<br>
`ps -A -T -o tid,comm,pcpu | grep 'TimeLord'`<br>
You'll get two entries. Last one is the 99/100% creating VDF stream. Can also find it with `htop`. Let's say it have pid(tid) 5122. Assign it to the isolated CPU core:<br>
`taskset -cp 8 5122`<br>
Check result through `htop`. Should have core 8 at 100% all the time through creation of VDF stream.

Also possible to isolate several E-cores and assign the process to all of them. Let's say we want a whole E-core cluster, 4x E-cores.
Syntax becomes `isolcpus=8,9,10,11` and `taskset -cp 8-11 5122`. How OS process scheduler moves, or not, the process around those E-cores. Not sure. Need to experiment yourself.

### Optimize: CPU speed

At this stage we know what to expect for each 0.1 GHz, **1.351** MH/s. All testing we have observed have given linear increase, given CPU GHz. Now it is time to clock CPU core as high as possible.

_NOTE: Optimize and **overclock at your own risk**._

First a boring observation. Many elements surrounding raw GHz of CPU core have been tested:

* RAM type/speed/latency/bandwidth
* HyperThreading on/off
* Virtualization (VT-d)
* Mitigations (Spectre/Meltdown)
* CPU cache/ring ratio
* CPU L1/L2/L3 cache size
* CPU core-to-core latency

Nothing looks to affect timelord speed, except CPU core clock (GHz). Remember, timelord logic for creation of VDF stream is very small. Not much outside a few instructions repeated in a CPU core.

In previous CPU generations we disabled P-core hyperthreading and the E-cores themselves. Was no penalty observed on timelord speed. Less complications, more overclocking potential. In this case, if motherboard supports it. Manually clock P-cores lower (GHz), and as high as possible for E-core(s).

Now it is a game of getting highest possible GHz, while keeping CPU cool and stable.

### Optimize: CPU speed (Intel 15th-gen)

Currently it looks like E-cores on Intel's 15th-gen are able to overlock to between 5.3 to 5.5 GHz. Depending on luck of draw of CPU quality. Tough, are **strong indications of rapid silicon degradation** at those speeds.

YOU need to decide if worth it. **Overclock at your own risk, CPU can degrade**.

## Fastest TimeLord

On-chain timelord rewards was introduced with testnet10, and further refined with mainnet-rc (release candidate). Now part of blockchain logic. Before that, a temporary centralized solution existed.

How do you know if you are fastest timelord. Ultimate indicator is very easy. You have a wallet address set up as 'TimeLord Reward Address' in SETTINGS in WebGUI. Timelord rewards will show up as `0.2 MMX` of type `VDF_REWARD`, every 20th block (...00,20,40,60,80).

Overtaking as fastest timelord is not instant. Unless current fastest timelord outright stops, crashes, or new one is faster by a good margin. Your timelord starts behind because of network and verify VDF latency. Not easy to quantify given internet itself, other nodes and decentralized logic. Will help having a fast VDF verify (GPU). Is where your timelord starts calculating its VDF stream from.

Still early days. We do not know yet how people will evaluate risk, cost and rewards for running fastest timelord. Run fast, crash often and degrade CPU. Or at the edge, still stable, taking over when fastest crash.  Will pattern stabilize, or be random.

### Optimize: VDF verify (GPU)

This article is mostly about optimizing your local timelord speed, VDF create on CPU. From whitepaper (quote):
> _In the best case, the latency to receive a block is around 100 ms. While the fastest VDF verification is currently around 200 ms, using 40 series NVIDIA GPUs with high clocks. That means a timelord trying to gain the lead would have to be around 3% faster._

As told, many variables in play. Network latency, node reputation, so on. **Another aspect you can optimize locally** is **latency of VDF verify**. Get it as low as possible. For a reference, my RTX 4060 Ti, with fastest timelord at ~72.5 MH/s. Have ~0.257 sec VDF verify.

## Feedback

Please contradict findings above, or tell of new ones. Use [`#mmx-timelord`](https://discord.com/channels/852982773963161650/1026219599311675493) channel on Discord.

## Donation

If you find information useful, donations are welcome:

```
BTC: bc1qtl00g8lctmuud72rv5eqr6kkpt85ws0t2u9s8d
ETH: 0x5fA8c257b502947A65D399906999D4FC373510B5
MMX: mmx1pk95pv4lj5k3y9cwxzuuyznjsgdkqsu7wkxz029nqnenjathtv7suf9qgc
XCH: xch1rk473wu3yqlxyyap4f4fhs8knzf4jt6aagtzka0g24hjgskmlv7qcme9gt
KAS: kaspa:qqjrwh00du33v4f78re4x3u50420fcvemuu3ye3wy2dhllxtjlhagf04g97hj
```
