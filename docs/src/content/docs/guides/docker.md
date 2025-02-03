---
title: Docker Usage
description: Examples for using pre-built docker images
---

## Node Containers

Docker images are available via `ghcr.io/madmax43v3r/mmx-node` in 2 build flavours:
- `latest`: Built from the most recent release version
- `edge`: Built from the most recent commit to the master branch

Additionally, each semver tag produces tagged images using the same three flavours from above: `<major>.<minor>.<patch>`, `<major>.<minor>` and `<major>` each with their respective suffix (eg. `0.9.5-amd`)

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
      #- "127.0.0.1:11380:11380"  # API port. Uncomment and set host to 0.0.0.0 in config/local/HttpServer.json for webUI/API access
      #- "11333:11333"  # Harvester port. Uncomment to allow remote harvesters to connect to the farmer
      #- "11330:11330"  # Farmer port. Uncomment to allow remote farmers to connect to the node
```

### AMD GPU

For amd gpu support please see the following `compose.yml`:
```yml
services:
  node:
    image: ghcr.io/madmax43v3r/mmx-node:latest-amd
    restart: unless-stopped
    group_add:
      - video
      - render
    devices:
      - /dev/dri:/dev/dri
      - /dev/kfd:/dev/kfd
    volumes:
      - /some/path/to/mmx-node:/data
    ports:
      - "11337:11337"  # Node p2p port. Forward your router to this port for peers to be able to connect
      #- "127.0.0.1:11380:11380"  # API port. Uncomment and set host to 0.0.0.0 in config/local/HttpServer.json for webUI/API access
      #- "11333:11333"  # Harvester port. Uncomment to allow remote harvesters to connect to the farmer
      #- "11330:11330"  # Farmer port. Uncomment to allow remote farmers to connect to the node
```

### Intel GPU

For intel gpu support please see the following `compose.yml`:
```yml
services:
  node:
    image: ghcr.io/madmax43v3r/mmx-node:latest-intel
    restart: unless-stopped
    group_add:
      - video
      - render
    devices:
      - /dev/dri/renderD128:/dev/dri/renderD128
    volumes:
      - /some/path/to/mmx-node:/data
    ports:
      - "11337:11337"  # Node p2p port. Forward your router to this port for peers to be able to connect
      #- "127.0.0.1:11380:11380"  # API port. Uncomment and set host to 0.0.0.0 in config/local/HttpServer.json for webUI/API access
      #- "11333:11333"  # Harvester port. Uncomment to allow remote harvesters to connect to the farmer
      #- "11330:11330"  # Farmer port. Uncomment to allow remote farmers to connect to the node
```

Note: `- render` in `group_add` might need to be removed, depending on your system.

### NVIDIA GPU

For nvidia gpu support please see the following `compose.yml`:
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
      #- "127.0.0.1:11380:11380"  # API port. Uncomment and set host to 0.0.0.0 in config/local/HttpServer.json for webUI/API access
      #- "11333:11333"  # Harvester port. Uncomment to allow remote harvesters to connect to the farmer
      #- "11330:11330"  # Farmer port. Uncomment to allow remote farmers to connect to the node
```
Note: for nvidia you also need the `NVIDIA Container Toolkit` installed on the host, for more info please see: https://docs.nvidia.com/datacenter/cloud-native/container-toolkit/latest/install-guide.html

## Remote Services

Running a remote harvester or farmer can be done by overwriting the `CMD` of the Dockerfile, for example like this:

### Remote Harvester
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
Note: The harvester must be able to connect to the remote farmer on port 11333. See examples above and below

### Remote Farmer
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
```
Note: The farmer must be able to connect to the remote node on port 11330. See examples above
