---
title: VDF FAQ
description: Frequently asked questions the verified delay function.
---

## VDF Verifications

### _What is VDF and why do we need to verify it as a farmer?_
A Verifiable Delay Function, is the Proof of Time part in Proof of Space and Time. It is referred to as a proof that a sequential function was executed a certain number of times.

Verifiable: A verifier can verify the proof in a shorter amount of time than it took to generate it.

Delay: It proofs that a certain amount of real time has elapsed.

Function: This means it’s deterministic. Computing a VDF with an input x always yields the same result y.

VDF verification requires either a fast multi-threaded CPU or a (decent) GPU.

### _What kind of GPU do I need for verifying the VDF? What's the minimum required/recommended GPU for VDF verifications?_
Nvidia's GT1030 is affordable, power efficient and does VDF verifications in < 4 seconds, while the GTX1650 does it in < 1 second. For best results, devices which support OpenCL 1.2 are recommended. It has been demonstrated that OpenCL 1.1 devices can verify VDFs, but performance is significantly lower than cards even 1 generation newer, and the additional requirement of installing specific drivers to support the older cards can be challenging, depending on the operating system used. For a list of supported GPUs, please see:

https://docs.google.com/spreadsheets/d/1LqyZut0JBwQpbCBnh73fPXkT-1WbCYoXVnIbf6jeyac/

For a list of popular GPUs with their VDF times, please see:

https://docs.google.com/spreadsheets/d/1NlK-dq7vCbX4NHzrOloyy4ylLW-fdXjuT8468__SF64/

### _My node is showing `VDF verification took longer than recommended: x.xxx sec` or `[Node] WARN: VDF verification took longer than block interval, unable to keep sync!`. What is wrong?_
Verification times below 3 sec is good, whereas anything > 5 seconds is bad. If your VDF verification, either done by the CPU or GPU, took more than 5 seconds, then you will get this warning message. Upgrading your CPU or GPU is strongly recommended. If you think you have a fast GPU and still getting this message, verify that your GPU driver is properly installed and the verification process is really done with the GPU, not the CPU.

### _How do I know if I have installed and set up OpenCL drivers correctly?_
Run clinfo in the command line. If it's showing at least 1 platform, it should work properly.

For Windows, download proprietary clinfo utility from:

https://opencl.gpuinfo.org/download.php

### _How do I get OpenCL to do VDF verifications?_
https://docs.mmx.network/guides/optimize-vdf/

### _I have an Intel/AMD CPU that come with an iGPU and another discrete GPU installed. MMX can use the iGPU to do OpenCL VDF, but not with the discrete GPU. How can I use the discrete GPU to do OpenCL VDF?_
First, install the drivers for the discrete GPU. Use clinfo for Linux or for Windows, download clinfo utility here:

https://opencl.gpuinfo.org/download.php

Perform OpenCL hardware diagnostic/info tool and get `cl_platform_name`

Then run the node with the GPU you want OpenCL to be done with:
`./run_node.sh --opencl.platform "name"`

For Nvidia, it's "NVIDIA CUDA"

For AMD, it's "AMD Accelerated Parallel Processing"

For Intel, it's "Intel(R) OpenCL"

For a more permanent solution (or in Windows), you can also create a file named `platform` in ~/config/local/opencl/ and put the platform name in there. Please include the quotes "".

If you have an AMD APU and another AMD discrete GPU with the same platform name, you can select which OpenCL device to use:
`./run_node.sh --Node.opencl_device 0/1`

Or if you have an AMD APU and a mix of discrete AMD/Nvidia GPUs, you can combine the parameters. Example:
`./run_node.sh --Node.opencl_device 2 --opencl.platform "NVIDIA CUDA"`

If you want to force use your CPU to do VDFs, while having a GPU installed in your computer, you can run this command:
`./run_node.sh --Node.opencl_device -1`

### _How do I know if MMX is using my GPU to do VDFs?_
Start a node, look for this line:

[Node] INFO: Using OpenCL GPU device [0] GeForce RTX 3070 Ti (total of 1 found)

If you have more than 1 GPU, please select accordingly in file `/config/local/Node.json`.

### _I have installed and set up OpenCL drivers correctly, yet MMX only uses my CPU. What is wrong?_
VDF verification is done every 10 seconds and only for the latest block height. Syncing still uses the CPU.