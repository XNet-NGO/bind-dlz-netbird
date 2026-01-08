# BIND DLZ Plugin for Netbird VPN - Automatic Peer Hostname Resolution

Hello r/selfhosted,

We have open-sourced a **BIND DNS plugin** that automatically resolves Netbird VPN peer hostnames.

## The Problem

When running a self-hosted Netbird instance, Netbird provides built-in DNS, but integrating with existing BIND infrastructure requires additional work. We wanted to enable queries like:

```bash
ssh nas.bird.mydomain.com    # Instead of ssh 100.64.0.5
ping printer.bird.mydomain.com
```

## The Solution

A DLZ (Dynamically Loadable Zone) plugin that:

1. Fetches peer list from Netbird API every 5 minutes
2. Caches hostnames → IPs in memory
3. Responds to DNS queries instantly from cache

No zone files to maintain. Peers join the network and DNS just works.

## Features

- Sub-millisecond lookups (in-memory cache)
- Auto-refresh every 5 minutes
- Thread-safe (concurrent queries supported)
- Docker-friendly
- Single .so file to deploy

## Technical Stack

- C with pthreads
- libcurl for API calls
- Jansson for JSON parsing
- BIND 9.18+ DLZ API

## Repository

https://github.com/XNet-NGO/bind-dlz-netbird

This project is developed by XNet Inc., a nonprofit corporation building network infrastructure for underserved communities. Feedback and contributions are welcome.

Website: https://xnet.ngo

---

For those running Netbird: what DNS solution are you currently using?
