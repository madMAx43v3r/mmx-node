---
title: Optimizing VDF Verification
description: How to improve VDF verification speed.
---
Using OpenCL is an optional (but highly recommended) feature for farming MMX. Offloading the verification of the VDF from the CPU to an OpenCL accelerated GPU (even an APU  with integrated GPU) can increase both performance and power efficiency.

Example Hardware and times spreadsheet (go to current testnet tab): https://docs.google.com/spreadsheets/d/1NlK-dq7vCbX4NHzrOloyy4ylLW-fdXjuT8468__SF64/edit#gid=0

**During initial blockchain sync, CPU usage will be very high in any case.**

Once the blockchain is synced, you will see these lines:
```
[Node] INFO: Verified VDF for height 239702, delta = 10.111297 sec, took 0.089002 sec.
```
which indicates that your node is now verifying the current VDFs as they are received. Your GPU would now be utilized every ~8 seconds.

If OpenCL is not being utilized for VDF verification, you should see relatively high CPU usage across all cores every ~8 seconds.

If you are running a Timelord (the default is OFF), you will see high CPU usage on at least 2 cores in any case, even if your GPU is used for verification.

## OpenCL for Intel iGPUs
Intel iGPUs prior to 11th gen will most likely not be sufficient for mainnet.

Ubuntu 20.04, 21.04
```
sudo apt install intel-opencl-icd
```

Ubuntu ppa for 18.04, 20.04, 21.04
```
sudo add-apt-repository ppa:intel-opencl/intel-opencl
sudo apt update
sudo apt install intel-opencl-icd
```

If the above doesn't work, you can try installing the latest drivers: https://dgpu-docs.intel.com/installation-guides/ubuntu/ubuntu-focal.html


Make sure your iGPU is not somehow disabled, like here for example: https://community.hetzner.com/tutorials/howto-enable-igpu

## OpenCL for AMD GPUs

Ubuntu:

Download installer (with `wget`) from `https://repo.radeon.com/amdgpu-install/latest/ubuntu/`

Change directory to the Download folder and install with `sudo dpkg -i amdgpu-install*.deb`

`sudo apt install rocm-opencl`

Follow on-screen instructions and then run:
```
sudo apt update
sudo apt upgrade
```

Arch Linux:
```
sudo pacman -S opencl-amd
```

Mesa drivers with opencl drivers do seem to work, but performance is very poor (VDF verification >4x longer).\
To install mesa drivers:\
Ubuntu:
```
sudo apt install mesa-opencl-icd
```
Arch:
```
sudo pacman -S mesa mesa-utils opencl-mesa
```

Windows: https://google.com/search?q=amd+graphics+driver+download

## OpenCL for Nvidia GPUs

Install Nvidia drivers.

There's an issue on Windows Laptops which requires a config file `config/local/opencl/platform` with contents `"NVIDIA CUDA"` (including the quotes) to make it detect the GPU (`opencl_device` should be 0 in this case as well).

### Ubuntu

```
sudo apt install nvidia-driver-470
```
Version 470 still works with older Kepler cards like a Quadro K2000.

### Arch Linux
```
sudo pacman -S nvidia nvidia-utils opencl-nvidia
```
For older GPUs, use drivers from the AUR:
```
Kepler series newest driver: 470xx
yay -S nvidia-470xx-dkms nvidia-470xx-utils opencl-nvidia-470xx

Fermi series newest driver: 390xx
yay -S nvidia-390xx-dkms nvidia-390xx-utils opencl-nvidia-390xx
```

## AVX2 Optimizations

VDF verification is now optimized for CPUs that support AVX2 and acceleration is done automatically when starting a node.

To disable VDF verification done on GPU with OpenCL and force the CPU to do it, run `./run_node.sh --Node.opencl_device -1` when running your node.

Or for a more permanent solution, edit your Node.json file in ~/config/local/ and set `opencl_device` value to -1.
