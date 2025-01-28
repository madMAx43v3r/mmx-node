---
title: Installation Guide
description: How to install MMX Node.
---
## Windows

You will need to install and update "Microsoft Visual C++ Redistributable for Visual Studio 2022" to the latest.

Scroll down to "Other tools and redistributables:" https://visualstudio.microsoft.com/downloads/

Windows executables: https://github.com/madMAx43v3r/mmx-node/releases

## Linux

### Dependencies

Ubuntu Linux:
```
sudo apt update
sudo apt install git cmake build-essential automake libtool pkg-config curl libminiupnpc-dev libjemalloc-dev libzstd-dev zlib1g-dev ocl-icd-opencl-dev clinfo screen
sudo apt install qtwebengine5-dev  # for native GUI
```

Arch Linux:
```
sudo pacman -Syu
sudo pacman -S base-devel git cmake curl miniupnpc jemalloc zstd zlib opencl-headers ocl-icd clinfo screen
sudo pacman -S qt5-webengine  # for native GUI
```

Fedora Linux:
```
yum install kernel-devel git cmake automake libtool curl gcc gcc-c++ miniupnpc-devel jemalloc-devel ocl-icd-devel zlib-ng-devel zstd clinfo screen
```

Note: OpenCL packages are optional, ie. `ocl-icd-opencl-dev`, etc...

OpenCL provides faster and more [efficient VDF verification](https://github.com/madMAx43v3r/mmx-node/wiki/Optimizations-for-VDF-Verification) using an integrated or dedicated GPU.

A standard iGPU found in Intel 1165G7 with 96EU iGPU on a low power laptop can verify VDF in about 4 sec at 48 MH/s timelord speed. For more info, please see: [What kind of GPU do I need for VDF verifications?](https://github.com/madMAx43v3r/mmx-node/wiki/Frequently-Asked-Questions#what-kind-of-gpu-do-i-need-for-verifying-the-vdf-whats-the-minimum-requiredrecommended-gpu-for-vdf-verifications)

If you dont have a fast modern (>4 GHz) CPU, it is required to have a GPU with OpenCL support.

Make sure to be in the `video` and or `render` group (depends on distribution) to be able to access a GPU:
```
sudo adduser $USER video
sudo adduser $USER render
```

### Building

```
git clone https://github.com/madMAx43v3r/mmx-node.git
cd mmx-node
./update.sh
```

To update to latest version:
```
./update.sh
```

To switch to latest testnet:
```
rm NETWORK
./update.sh
```

### Rebuilding

If you suspect some files might not build properly or if you want to recompile, stop the node then run:
```
./clean_all.sh
./update.sh
```
This is needed when updating system packages for example.

### Windows via WSL

To setup Ubuntu 20.04 in WSL on Windows you can follow the tutorial over here: \
https://docs.microsoft.com/en-us/learn/modules/get-started-with-windows-subsystem-for-linux/

Make sure to install Ubuntu in step 2: https://www.microsoft.com/store/p/ubuntu/9nblggh4msv6

Then type "Ubuntu" in the start menu and start it, you will be asked to setup a user and password.
After that you can follow the normal instructions for Ubuntu 20.04.

To get OpenCL working in WSL:
https://devblogs.microsoft.com/commandline/oneapi-l0-openvino-and-opencl-coming-to-the-windows-subsystem-for-linux-for-intel-gpus/

## Docker

Docker images are available via `ghcr.io/madmax43v3r/mmx-node` in a few flavours:
- `edge`: Latest commit cpu only
- `edge-amd`: Latest commit with amd gpu support
- `edge-nvidia`: Latest commit with nvidia gpu support

Additionally, each semver tag produces tagged images using the same three flavours from above: `<major>.<minor>.<patch>`, `<major>.<minor>` and `<major>` each with their respective suffix (eg. `0.9.5-amd`). `latest` is set for the latest cpu only semver image.

Each image provides a volume for `/data` which you can override with your own volume or a mapped path to customize the storage location of the node data.

### CPU only

A `docker-compose.yml` for the cpu only node can look like this:
```yml
version: '3'
services:
  node:
    image: ghcr.io/madmax43v3r/mmx-node:edge
    restart: unless-stopped
    volumes:
      - /some/path/to/mmx/node/data:/data
```

### AMD GPU

For amd gpu support please see the following `docker-compose.yml`:
```yml
version: '3'
services:
  node:
    image: ghcr.io/madmax43v3r/mmx-node:edge-amd
    restart: unless-stopped
    group_add:
      - video
      - render
    devices:
      - /dev/dri:/dev/dri
      - /dev/kfd:/dev/kfd
    volumes:
      - /some/path/to/mmx/node/data:/data
```
Note: `- render` in `group_add` might need to be removed, depending on your system.

### NVIDIA GPU

For nvidia gpu support please see the following `docker-compose.yml`:
```yml
version: '3'
services:
  node:
    image: ghcr.io/madmax43v3r/mmx-node:edge-nvidia
    restart: unless-stopped
    runtime: nvidia
    volumes:
      - /some/path/to/mmx/node/data:/data
```
Note: for nvidia you also need the `NVIDIA Container Toolkit` installed on the host, for more info please see: https://docs.nvidia.com/datacenter/cloud-native/container-toolkit/install-guide.html#docker

### Remote harvester

Running a remote harvester is done by overwriting the `CMD` of the Dockerfile, for example like this:
```yml
version: '3'
services:
  harvester:
    image: ghcr.io/madmax43v3r/mmx-node:edge
    restart: unless-stopped
    volumes:
      - /some/path/to/mmx/node/data:/data
      - /some/path/to/disks:/disks
    command: './run_harvester.sh -n <some ip or hostname here>:11333'
```

## Custom storage path

To change the storage path for everything you can set environment variable `MMX_HOME` to `/your/path/` (trailing slash required). By default the current directory is used, ie. `mmx-node`.
