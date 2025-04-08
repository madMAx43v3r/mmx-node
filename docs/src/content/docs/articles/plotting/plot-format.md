---
title: Plot format
description: Plot format for MMX (mainnet), introduced with testnet11
prev: false
next: false

articlePublished: 2024-04-06
articleUpdated: 2025-01-30

authorName: voidxno
authorPicture: https://avatars.githubusercontent.com/u/53347890?s=200
authorURL: https://github.com/voidxno
---

Plot format, MMX.

_**IMPORTANT:**_ _My own understanding, reading comments when mainnet plot format was rolled out with testnet11. Still relevant info, as an introduction. Reference official information if unsure._

## TLDR;

- Plot format for MMX (mainnet), introduced with testnet11
- **Resistant against compression**
- Two types, HDD and SSD-plots (min k29, max k32):
  | | `k29` | `k30` | `k31` | `k32` |
  | :--- | :--- | :--- | :--- | :--- |
  | `hdd` | 36.2 GiB | 75.3 GiB | 155.4 GiB | 320.3 GiB |
  | `sdd` | 14.7 GiB | 30.3 GiB |  62.3 GiB | 128.3 GiB |
- SSD-plots have missing data, smaller, but require high disk IOPS
- Plotting: GPU required, full RAM, partial RAM, disk mode
- Plotting: Any old 4GB VRAM (Pascal), `k32` needs 6GB VRAM
- Farming: CPU only (plots), GPU not supported (not worth it)
- Farming: Still iGPU/GPU recommended for VDF-verify (CPU possible)
- Compression: Still supported, practically not worth it (1-4%, C1-C15)
- Bonus: Open source mmx-node builds (GPU plotter too)

## Compression

### Previously

Plot compression is not really traditional compression. It is information left out of plot file, reducing its size. Reconstructed on-the-fly when farming.

Data in plot files are several nested tables that takes a certain amount of time to create (plotting). If passing plot filter, this data is used to create potential proofs. That again could be the winning proof for a block.

Previously plot files where cleverly reworked to smaller sizes, with missing information. When farming, that information was reconstructed on-the-fly when needed. Branded as plot compression.

### Today

Testnet11 introduced a new plot format that are orders of magnitude more resistant to these kind of compression techniques. Reworked by Max to make it practically worthless. Not possible to be 100% compression resistant. It's math. But reworked algorithms, required proof time, limits it greatly.

As a principle, compression is still an option. The higher compression chosen (C1-C15), longer plot times, more CPU compute when farming. Gaining **only 1-4% compression, compared to +200% with other Proof of Space**.

HDD-plots have C0-C4 as recommended compression (1%). Still easily processed by CPU when farming. Going further up, too much load/time on CPU.

## SSD-plots

SSD-plots are a type of pre-compressed plots, missing hashes. Reconstructed on-the-fly when farming, but requires very high disk IOPS. Storing them on HDD will not work, not enough IOPS on spinning platters. Goal is to balance cost/reward potential between SSD and HDD storage (2.5x SSD advantage, but more expensive media).

Basically: Given **identical k-size, SSD-plots has same effective space on network as HDD-plots**. Takes less space to store (**2.5x advantage**), but more expensive media (SSD).

Given the high IOPS for SSD-plots, recommended to use C0 for compression (0%).

## Plotting

**RAM requirements** for plotter, in [memory GB](https://en.wikipedia.org/wiki/Byte#Multiple-byte_units "Memory GB units, 1024^3") (similar for HDD and SSD-plots):
| | full RAM | partial RAM | disk mode |
| :--- | :--- | :--- | :--- |
| `hdd`/`sdd-k29` | 60 GB | 32 GB | 8 GB |
| `hdd`/`sdd-k30` | 110 GB | 64 GB | 8 GB |
| `hdd`/`sdd-k31` | 216 GB | 120 GB | 8 GB |
| `hdd`/`sdd-k32` | 410 GB | 232 GB | 8 GB |

_NOTE: Disk mode usually less than 8 GB, depends on `-S` streams option._\
_NOTE: If close to 32 GB, 32 GB system RAM not enough. 48/64 GB needed._

**Temp disk space** needed when plotting (similar for HDD and SSD-plots):
| | full RAM | partial RAM | disk mode |
| :--- | :--- | :--- | :--- |
| `hdd`/`sdd-k29` | 0.0 GiB | 24 GiB | 51 GiB |
| `hdd`/`sdd-k30` | 0.0 GiB | 48 GiB | 101 GiB |
| `hdd`/`sdd-k31` | 0.0 GiB | 100 GiB | 202 GiB |
| `hdd`/`sdd-k32` | 0.0 GiB | 200 GiB | 404 GiB |

**Total disk writes** when plotting, in [TiB](https://en.wikipedia.org/wiki/Byte#Multiple-byte_units "TebiByte units, 1024^4") written:
| | full RAM | partial RAM | disk mode |
| :--- | :--- | :--- | :--- |
| `hdd`/`sdd-k29` | 0.04 / 0.02 TiBW | 0.08 / 0.06 TiBW | 0.33 / 0.28 TiBW |
| `hdd`/`sdd-k30` | 0.08 / 0.03 TiBW | 0.16 / 0.12 TiBW | 0.68 / 0.57 TiBW |
| `hdd`/`sdd-k31` | 0.16 / 0.06 TiBW | 0.33 / 0.25 TiBW | 1.40 / 1.14 TiBW |
| `hdd`/`sdd-k32` | 0.32 / 0.13 TiBW | 0.67 / 0.50 TiBW | 2.88 / 2.27 TiBW |

- Testnet11 introduced a new farmer key, 33 bytes ECDSA vs old 48 BLS (`-f`, 66 chars vs old 96).

Follow [`#mmx-general`](https://discord.com/channels/852982773963161650/925017012235817042) on Discord.

## Farming

**Disk seeks** per plot (independent of k-size or C-compression):
| | pass filter | full proof
| :--- | :--- | :--- |
| `hdd` | ~2 | ~256 |
| `ssd` | ~4096 | 0 |

Follow [`#mmx-farming`](https://discord.com/channels/852982773963161650/853417165980827657) on Discord.
