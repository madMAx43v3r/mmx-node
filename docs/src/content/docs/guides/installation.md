---
title: Installation Guide
description: How to install MMX Node.
---

## Windows

Windows installers are available here: https://github.com/madMAx43v3r/mmx-node/releases

You will need to install and update "Microsoft Visual C++ Redistributable for Visual Studio 2022" to the latest:\
Scroll down to "Other tools and redistributables" here: https://visualstudio.microsoft.com/downloads/

## Linux

Linux binary packages are available here: https://github.com/madMAx43v3r/mmx-node/releases

To install a binary package:
```
sudo apt install ~/Downloads/mmx-node-1.1.7-amd64-ubuntu-20.04.deb
```
This will automatically install dependencies as well.

When no matching package is found, continue below to build from source.

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
sudo yum install kernel-devel git cmake automake libtool curl gcc gcc-c++ miniupnpc-devel jemalloc-devel ocl-icd-devel zlib-ng-devel zstd clinfo screen
sudo yum install qt5-qtwebengine-devel  # for native GUI
```

### Building from Source

```
git clone https://github.com/madMAx43v3r/mmx-node.git
cd mmx-node
./update.sh
```

To update to latest version:
```
./update.sh
```

### Rebuilding

If the build is broken for some reason:
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
