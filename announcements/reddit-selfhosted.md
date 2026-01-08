# I built a BIND DLZ plugin for Netbird VPN - resolve peer hostnames automatically

Hey r/selfhosted!

Just open-sourced a project that scratched my own itch: a **BIND DNS plugin** that automatically resolves Netbird VPN peer hostnames.

## The Problem

I'm running a self-hosted Netbird instance for my homelab/org. While Netbird has built-in DNS, I wanted to integrate with my existing BIND infrastructure so I could do things like:

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

- ⚡ Sub-millisecond lookups (in-memory cache)
- 🔄 Auto-refresh every 5 minutes
- 🔒 Thread-safe (concurrent queries OK)
- 🐳 Docker-friendly
- 📦 Single .so file to deploy

## Tech Stack

- C with pthreads
- libcurl for API calls
- Jansson for JSON parsing
- BIND 9.18+ DLZ API

## GitHub

**https://github.com/XNet-NGO/bind-dlz-netbird**

This is my first real open-source release. Feedback appreciated!

---

Anyone else running Netbird? What's your DNS setup?
