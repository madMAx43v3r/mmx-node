---
title: TimeLord Predictions
description: State and predictions of MMX timelord speeds
prev: false
next: false

articlePublished: 2023-10-05
articleUpdated: 2025-03-17

authorName: voidxno
authorPicture: https://avatars.githubusercontent.com/u/53347890?s=200
authorURL: https://github.com/voidxno
---

Current state and predictions of MMX TimeLord speeds. Fun reading the future, and [failing](https://discord.com/channels/852982773963161650/1026219599311675493/1307377841070932009) spectacularly.

If needed, read [TimeLord Optimizations](../../../articles/timelord/timelord-optimizations/) for technical background and elements of [timelord logic](../../../articles/timelord/timelord-optimizations/#logic).

Running a timelord is optional (default is disabled). Not needed to run a fully functional node. Blockchain, as a whole, only need one active timelord to move forward. A few more, spread around, is preferred for redundancy and security. Do not need to be fastest.

## TLDR;

Fastest timelord predictions (as of **Mar2025**):

- Contenders: Intel/AMD CPUs with SHA extensions
- Observed: ~**74.5** MH/s (continuous)
- Observed: ~**75.2** MH/s (peak)
- Future: ~**71-77** MH/s (2025)

## Contenders

Current contenders for fastest timelord are Intel/AMD CPUs with SHA extensions. Why not GPU/FPGA/ASIC, [read here](../../../articles/timelord/timelord-optimizations/#why-not-gpufpgaasic). Always a possibility of something hidden out there, not observed yet.

Two elements decide speed of timelord:
- CPU core efficiency (MH/s per 0.1 GHz)
- CPU core clock speed (running GHz)

Leaves us with newest Intel/AMD CPUs that can clock 1x core the highest:
- Intel Ultra 200S series (15th-gen, Arrow Lake)
- AMD Ryzen 7000/9000 series (Zen 4/5)

_NOTE: [eBACS](https://bench.cr.yp.to) have [SHA256 efficiency benchmarks](https://bench.cr.yp.to/impl-hashblocks/sha256.html) for different CPU architectures with SHA extensions (dolbeau/amd64-sha)._

## Today (Mar2025)

To illustrate, ignoring overclocking, using max single-core boost from specifications.

Intel (E-core):
| | MH/s/0.1GHz | Max boost | TimeLord | Release |
| :--- | :--- | :--- | :--- | :--- |
| 12900KS (E-core) | ~**0.975** | 4.0 GHz | **39.0** MH/s | Nov2021 |
| 13900KS (E-core) | ~**0.975** | 4.3 GHz | **41.9** MH/s | Sep2022 |
| 14900KS (E-core) | ~**0.975** | 4.5 GHz | **43.9** MH/s | Oct2023 |
| 285K (E-core) | ~**1.350** | 4.6 GHz | **62.1** MH/s | Oct2024 |

Intel (P-core):
| | MH/s/0.1GHz | Max boost | TimeLord | Release |
| :--- | :--- | :--- | :--- | :--- |
| 12900KS (P-core) | ~**0.705** | 5.5 GHz | **38.8** MH/s | Nov2021 |
| 13900KS (P-core) | ~**0.705** | 6.0 GHz | **42.3** MH/s | Sep2022 |
| 14900KS (P-core) | ~**0.705** | 6.2 GHz | **43.7** MH/s | Oct2023 |
| 285K (P-core) | ~**0.905** | 5.7 GHz | **51.5** MH/s | Oct2024 |

AMD (Zen4, Zen5):
| | MH/s/0.1GHz | Max boost | TimeLord | Release |
| :--- | :--- | :--- | :--- | :--- |
| 5950X (Zen4-core) | ~**0.755** | 4.9 GHz | **37.0** MH/s | Nov2020 |
| 7950X (Zen4-core) | ~**0.755** | 5.7 GHz | **43.0** MH/s | Sep2022 |
| 9950X (Zen5-core) | ~**0.715** | 5.7 GHz | **40.8** MH/s | Aug2024 |

In reality, will need to clock 1x core as high as possible (GHz). Keep it there, stable, cool it, 100% load, 24/7.

A lot is up to cooling commitment, together with luck of draw on CPU silicon quality. With segmentation and binning done directly by Intel/AMD these days (product SKUs). Top-end SKUs, like Intel KS editions, do not give much headroom. Except if exotic cooling, like LN2 (liquid nitrogen).

Observed: ~**74.5** MH/s (continuous)

## Future (2025)

Next interesting CPU architecture and node developments:
- Intel: Nova Lake (_tbd_, Desktop)
- AMD: Zen 6 (_tbd_, Desktop)

Both Intel and AMD are cagey about real information on future CPU generations, up-to and including release.

Most relevant for timelord:
- CPU core efficiency (MH/s per 0.1 GHz)
- CPU core clock speed (running GHz)

Previous changes in CPU architecture have shown that efficiency for SHA extensions usually change for the better (SHA256 calculations per 0.1 GHz). Intel's 285K E-core (1.350) efficiency increase compared to 14900KS E-core (0.975) was a [surprise](https://discord.com/channels/852982773963161650/1026219599311675493/1307377841070932009). Existing E-core efficiency (0.975) vs P-cores (0.705) was already best in class.

Increase is not a given though. Measurements of Zen5-core (0.715) SHA extensions efficiency are lower than Zen4-core (0.755). Probably an architectural decision.

Digital circuits have physical limits with trade-offs between CPU cycles needed and max clock frequency. There is a reason why P-cores have higher boost clock (5.7 GHz) than E-cores (4.6 GHz). But at the same time SHA extensions efficiency are lower for P-cores (0.905) vs E-cores (1.350).

New desktop CPU architectures are not planned for release by Intel or AMD in 2025. Might get refreshes with somewhat higher clocks, but standstill on IPC and SHA extensions efficiency.

Even when we get new desktop CPU architectures in future (Nova Lake, Zen 6). Efficiency will be a question of physical limits depending on architecture and node. After that, what CPU clock speed is possible.

With a starting point of ~62 MH/s for Intel 285K (E-core, 4.6 GHz). Possible CPU refreshes and overclock cooling. Up to 20% on top of starting point through 2025.

Prediction: ~**71-77** MH/s (continuous)

## Uncertainties (peaks)

It is possible we will get short peaks of someone running a timelord with exotic cooling. Trying to make a record timelord speed until CPU crashes, or empty of LN2 (liquid nitrogen).

One way to guestimate is to browse [HWBOT](https://hwbot.org/hardware/processors) Geekbench6 [single](https://hwbot.org/benchmark/geekbench6_-_single_core/) and [multi-core](https://hwbot.org/benchmark/geekbench6_-_multi_core/) records.

Problem is getting E-core only overclocks for Intel 285K. We have P-core at about 7.0 GHz (single). A few internet mentions looks to have E-core overclock limit/wall around 5.5 GHz. These are usually records done with LN2 cooling. Just enough to run the benchmark.

For some reason Intel looks to have left more overclock headroom on E-cores than usual. Observed timelords have probably reached 5.5 GHz. With the benefit of doubt, say someone manages to run **Intel 285K P-cores at 7.0 GHz** or **E-cores at 5.7 GHz**. Results: **63.3** and **77.0** MH/s.

More probable with continuous timelord speeds mentioned above.

## Feedback

Other angles on topic above. Share on [`#mmx-timelord`](https://discord.com/channels/852982773963161650/1026219599311675493) or [`#mmx-farming`](https://discord.com/channels/852982773963161650/853417165980827657) channels on Discord.
