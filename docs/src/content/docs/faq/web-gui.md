---
title: Web GUI FAQ
description: Frequently asked questions about using MMX Web Interface.
---
# Web GUI

### _How do I access the web GUI?_
Since testnet6, WebGUI requires a login. For now, the password is randomly generated and can be accessed from your MMX home directory. In order to access the WebGUI, your node needs to be running (not necessarily synced).

For Windows, it's usually located at `C:\Users\name\.mmx\PASSWD` (open with notepad or text editor)

For Linux, it's usually located at `~/mmx-node/PASSWD` (cat or nano into the file)

**If somehow the password you copied is wrong, you might have to generate a new one.**
Delete `config/local/passwd` file, run `./activate.sh` again and restart your node.

### _I've got some remote farmer/harvester setup all in a LAN. How do I access web GUI to my main node?_
Edit `config/local/HttpServer.json` to set `host` as follows: `{"host": "0.0.0.0"}` (Node restart needed to apply)

Then entering `[MMX-Node-IP]:11380/gui/` in the URL bar of your browser should work.

For example: `192.168.1.123:11380/gui/`

Note: Only use this option if your local network is secure and no devices can sniff traffic and capture your password.

Option #2:
https://docs.mmx.network/guides/remote-services/#remote-connections-over-public-networks