---
title: Docker Usage
description: Examples for using pre-built docker images
---

## Node Containers

Docker images are available via `ghcr.io/madmax43v3r/mmx-node` in 2 build flavours:
- `latest`: Built from the most recent release version
- `edge`: Built from the most recent commit to the master branch

The following tag suffixes are also available for GPU vendor specific builds:
 - `-amd`
 - `-intel`
 - `-nvidia`

Additionally, each semver tag produces tagged images:
 - `<major>.<minor>.<patch>`
 - `<major>.<minor>`
 - `<major>`
 - each with their respective suffix (eg. `1.1.9-amd`)

Each image provides a volume for `/data` which you can override with your own volume or a mapped path to customize the storage location of the node data.

### CPU only

A `compose.yml` for the cpu only node can look like this:
```yml
services:
  node:
    image: ghcr.io/madmax43v3r/mmx-node:latest
    restart: unless-stopped
    volumes:
      - /some/path/to/mmx-node:/data
    ports:
      - "11337:11337"  # Node p2p port. Forward your router to this port for peers to be able to connect
      - "127.0.0.1:11380:11380"  # API port. Set host to 0.0.0.0 in /data/config/local/HttpServer.json for webUI/API access
      #- "11333:11333"  # Harvester port. Uncomment to allow remote harvesters to connect to the farmer
      #- "11330:11330"  # Farmer port. Uncomment to allow remote farmers to connect to the node
    environment:
      MMX_ALLOW_REMOTE: 'false'  # Set to true to allow connections from remote harvesters/farmers
      MMX_HARVESTER_ENABLED: 'true'  # Set to false to disable local harvester
      MMX_FARMER_ENABLED: 'true'  # Set to false to disable local farmer
```

### AMD GPU

For amd gpu support please see the following `compose.yml`:
```yml
services:
  node:
    image: ghcr.io/madmax43v3r/mmx-node:latest-amd
    restart: unless-stopped
    devices:
      - /dev/dri:/dev/dri
      - /dev/kfd:/dev/kfd
    volumes:
      - /some/path/to/mmx-node:/data
    ports:
      - "11337:11337"  # Node p2p port. Forward your router to this port for peers to be able to connect
      - "127.0.0.1:11380:11380"  # API port. Set host to 0.0.0.0 in /data/config/local/HttpServer.json for webUI/API access
      #- "11333:11333"  # Harvester port. Uncomment to allow remote harvesters to connect to the farmer
      #- "11330:11330"  # Farmer port. Uncomment to allow remote farmers to connect to the node
    environment:
      MMX_ALLOW_REMOTE: 'false'  # Set to true to allow connections from remote harvesters/farmers
      MMX_HARVESTER_ENABLED: 'true'  # Set to false to disable local harvester
      MMX_FARMER_ENABLED: 'true'  # Set to false to disable local farmer
```

### Intel GPU

For intel gpu support please see the following `compose.yml`:
```yml
services:
  node:
    image: ghcr.io/madmax43v3r/mmx-node:latest-intel
    restart: unless-stopped
    devices:
      - /dev/dri/renderD128:/dev/dri/renderD128
    volumes:
      - /some/path/to/mmx-node:/data
    ports:
      - "11337:11337"  # Node p2p port. Forward your router to this port for peers to be able to connect
      - "127.0.0.1:11380:11380"  # API port. Set host to 0.0.0.0 in /data/config/local/HttpServer.json for webUI/API access
      #- "11333:11333"  # Harvester port. Uncomment to allow remote harvesters to connect to the farmer
      #- "11330:11330"  # Farmer port. Uncomment to allow remote farmers to connect to the node
    environment:
      MMX_ALLOW_REMOTE: 'false'  # Set to true to allow connections from remote harvesters/farmers
      MMX_HARVESTER_ENABLED: 'true'  # Set to false to disable local harvester
      MMX_FARMER_ENABLED: 'true'  # Set to false to disable local farmer
```

:::note[Note]
Intel ARC support requires a host system running Linux Kernel 6.3 or above.
:::

### NVIDIA GPU

For Nvidia gpu support please see the following `compose.yml`:
```yml
services:
  node:
    image: ghcr.io/madmax43v3r/mmx-node:latest-nvidia
    restart: unless-stopped
    runtime: nvidia
    volumes:
      - /some/path/to/mmx-node:/data
    ports:
      - "11337:11337"  # Node p2p port. Forward your router to this port for peers to be able to connect
      - "127.0.0.1:11380:11380"  # API port. Set host to 0.0.0.0 in /data/config/local/HttpServer.json for webUI/API access
      #- "11333:11333"  # Harvester port. Uncomment to allow remote harvesters to connect to the farmer
      #- "11330:11330"  # Farmer port. Uncomment to allow remote farmers to connect to the node
    environment:
      MMX_ALLOW_REMOTE: 'false'  # Set to true to allow connections from remote harvesters/farmers
      MMX_HARVESTER_ENABLED: 'true'  # Set to false to disable local harvester
      MMX_FARMER_ENABLED: 'true'  # Set to false to disable local farmer
```

:::note[Note]
For Nvidia you also need the `NVIDIA Container Toolkit` installed on the host, for more info please see: [Installing the NVIDIA Container Toolkit](https://docs.nvidia.com/datacenter/cloud-native/container-toolkit/latest/install-guide.html).
:::

## Remote Services

Running a remote harvester or farmer can be done by overwriting the `CMD` of the Dockerfile, for example like this:

### Remote Harvester

 - Set `MMX_ALLOW_REMOTE` to `true` in the node or farmer compose.yml
 - Make sure port 11333 is uncommented/exposed in the node or farmer compose.yml
 - Edit the following example with the node or farmers ip address and correct paths

```yml
services:
  harvester:
    image: ghcr.io/madmax43v3r/mmx-node:latest
    restart: unless-stopped
    command: './run_harvester.sh -n <some ip or hostname here>'
    volumes:
      - /some/path/to/mmx-node:/data
      - /some/path/to/disks:/disks
```

### Remote Farmer

 - Set `MMX_ALLOW_REMOTE` to `true` in the node compose.yml
 - To allow remote harvesters to connect to this farmer set `MMX_ALLOW_REMOTE` to `true` in the example below
 - Make sure port 11330 is uncommented/exposed in the node compose.yml
 - Edit the following example with the node ip address and correct data path

```yml
services:
  farmer:
    image: ghcr.io/madmax43v3r/mmx-node:latest
    restart: unless-stopped
    command: './run_farmer.sh -n <some ip or hostname here>'
    volumes:
      - /some/path/to/mmx-node:/data
    ports:
      - "11333:11333"  # Farmer listens on this port for remote harvester connections
    environment:
      MMX_ALLOW_REMOTE: 'false'  # Set to true to allow connections from remote harvesters
      MMX_HARVESTER_ENABLED: 'true'  # Set to false to disable local harvester
```
