---
title: VDF FAQ
description: Frequently asked questions about MMX verified delay function (VDF).
---

### Why do I need to verify VDF as a farmer?

A Verifiable Delay Function (VDF), is the Time part of Proof of Space (and Time). Time is proved through a sequential function executed a number of times.
- Verifiable: Verifier can verify the proof in a shorter amount of time than it took to generate.
- Delay: It proves that a certain amount of real time has elapsed.
- Function: It is deterministic, computing input x always yields the same result y.

VDF verify requires either a modern multi-core CPU, or a decent GPU/iGPU.

### What GPU/CPU do I need to verify VDF?

You want your VDF verify below 5 sec. It is directly related to current speed of [fastest timelord](../../../articles/timelord/timelord-predictions/#tldr). Nvidia's GT1030 is affordable, power efficient and does VDF verifications in < 3 seconds, while GTX1650 in < 1 second.

For best results, devices which support >= OpenCL 1.2 are recommended. It has been shown that OpenCL 1.1 devices can verify VDFs, but performance is significantly lower. Additional requirement of installing specific drivers to support the older cards can be challenging.

Also possible to use iGPU (integrated graphics). If the iGPU is powerful enough.

Many newer CPUs with SHA extensions are capable of verifying VDF. Not as power efficient as GPU, but possible.

For a list of supported GPUs, look [this Google spreadsheet](https://docs.google.com/spreadsheets/d/1LqyZut0JBwQpbCBnh73fPXkT-1WbCYoXVnIbf6jeyac/).\
For a list of GPU/CPU VDF times, look [this Google spreadsheet](https://docs.google.com/spreadsheets/d/1NlK-dq7vCbX4NHzrOloyy4ylLW-fdXjuT8468__SF64/).

:::note[Note]
VDF verify logic was reduced to 1 stream with mainnet, vs. 2 or 3 in testnets. Numbers for testnets are probably too pessimistic.
:::

### My node is warning of VDF verification times

If you get any of these two warnings, you need to check why your VDF verify times are so high:\
`VDF verification took longer than recommended ...`\
`VDF verification took longer than block interval ...`

Verification times below 3 sec are good, whereas anything > 5 seconds is bad. If you are above 5 sec, you will get warning messages.

[Check](#what-gpucpu-do-i-need-to-verify-vdf) that your GPU/CPU is powerful enough to verify VDF. If using GPU, [check](#how-do-i-know-if-node-is-using-gpu-for-vdf) that verification really is performed on it (and not CPU).

### How do I know if node is using GPU for VDF?

To be absolutely sure node is using GPU for VDF verify. Start node, and check:
```
[Node] INFO: Found OpenCL GPU device 'NVIDIA GeForce GT 1030' [0] (NVIDIA CUDA)
[Node] INFO: Using OpenCL GPU device 'NVIDIA GeForce GT 1030' [0] (NVIDIA CUDA)
```

First one means the node found OpenCL device. Second one `Using OpenCL GPU` indicates node is actually using the device. If multiple GPU devices, can configure which one under SETTINGS in WebGUI.

### How to make sure OpenCL is setup correctly?

Go through the [OpenCL Setup](../../../guides/optimize-vdf/) guide. Then check that OpenCL platform and device drivers really are installed and active.

On Linux, check output of `clinfo -l` command:
```
Platform #0: NVIDIA CUDA
 `-- Device #0: NVIDIA GeForce GT 1030
```

On Windows (evaluate yourself if this 3rd-party utility is ok for you):\
[OpenCL Hardware Capability Viewer](https://opencl.gpuinfo.org/download.php)

### Node and OpenCL is configured, only CPU used?

Might not be a problem.

When node is performing initial sync of blockchain DB, only CPU will be used. Any OpenCL GPU device will not be used for VDF verify until node is in sync with network.

VDF verify is only performed every 10 sec (blocktime). If your GPU is somewhat powerful, that compute of VDF verify is a 'blip' of less than 1 sec.
