---
title: WebGUI FAQ
description: Frequently asked questions about MMX web interface (WebGUI).
---

### How do I access the WebGUI?

If running Windows install or desktop Linux binary package. You already have access to WebGUI, wrapped as UI inside the application window.

If running mmx-node in the background on Linux, or just want to access the WebGUI through a browser. It is available locally on machine: http://localhost:11380/gui/

When accessed through a browser, you need to give a password. It is a randomly generated one, usually located here:
- Windows: `C:\Users\<user>\.mmx\PASSWD`
- Linux: `~/.mmx/PASSWD`
- Linux: `~/mmx-node/PASSWD`

In all cases MMX Node needs to be started, or mmx-node running in background.

### How do I access the WebGUI remotely?

Per-default, WebGUI is only exposed on localhost. You need to browse to it on machine it is running on. Through an unsecured `http` connection.

It is possible to access it remotely. Either through an SSH tunnel (recommended), or opening up for external connections (not recommended).

:::danger[Warning]
Make sure you understand the security risks, consequences, and measures needed if performing any of the steps below. The connection is unsecured `http`, and need to go over a 100% trusted network. Where no one can capture your network traffic to get access to sensitive information like passwords or wallets.
:::

Recommended way is to tunnel port `11380` through an SSH connection, from remote WebGUI machine. Making you able to browse and login locally on machine you have SSH'ed from. The security of the SSH connection will protect the unsecured `http` traffic. You can get some tips in [Remote Services](../../../guides/remote-services/#remote-connections-over-public-networks) guide.

<ins>Not</ins> recommended way is to configure mmx-node's internal `HttpServer` to answer external connection requests. Edit `config/local/HttpServer.json` file to `"host": "0.0.0.0",` (default: `"host": "localhost",`). Now, after a restart, it will answer to external connection requests. Need to also check if OS firewall is blocking incoming requests on port `11380`. If machine with WebGUI has IP `192.168.1.10`, and reachable from your machine: http://192.168.1.10:11380/gui/
