# BIND DLZ Plugin for Netbird - Dynamic DNS for VPN Peers

**Post to: https://forum.netbird.io/** (Category: Show & Tell or Integrations)

---

Hello,

We have released an open-source **BIND 9.18+ DLZ plugin** that automatically resolves Netbird peer hostnames to their WireGuard tunnel IPs.

## The Problem

When running a self-hosted Netbird instance, you often want to access peers by hostname rather than memorizing IPs like `100.64.0.x`. While Netbird has built-in DNS, integrating with an existing BIND infrastructure can be challenging.

## The Solution

Our DLZ (Dynamically Loadable Zone) plugin fetches peer data directly from the Netbird API and serves it via BIND. No zone file management needed!

```
dig nas.bird.example.com @your-dns-server
;; ANSWER SECTION:
nas.bird.example.com.    60    IN    A    100.105.130.31
```

## Features

- **Background sync** - Fetches peer data from Netbird API every 5 minutes
- **Thread-safe** - Read-write locks for concurrent DNS queries
- **Zero-downtime updates** - Atomic pointer swap for cache refresh
- **Docker-ready** - Works in containerized BIND deployments
- **Self-hosted friendly** - Point to your own Netbird API endpoint

## Architecture

```
┌─────────────┐     ┌──────────────────────────────────────────┐
│ DNS Client  │────▶│  BIND 9.18+                              │
└─────────────┘     │  ┌────────────────────────────────────┐  │
                    │  │  netbird_dlz.so                    │  │
                    │  │  ┌──────────┐    ┌──────────────┐  │  │
                    │  │  │ Lookup   │◀──▶│ Record Cache │  │  │
                    │  │  │ Handler  │    │ (RW-Locked)  │  │  │
                    │  │  └──────────┘    └──────────────┘  │  │
                    │  │        ▲               ▲           │  │
                    │  │        │          ┌────┴────┐      │  │
                    │  │        │          │ Background    │  │
                    │  │        │          │ Thread (5min) │  │
                    │  └────────┼──────────┴────┬────┴─────┘  │
                    └───────────┼───────────────┼─────────────┘
                                │               │
                                │               ▼
                                │      ┌─────────────────┐
                                │      │ Netbird API     │
                                │      │ /api/peers      │
                                └─────▶└─────────────────┘
```

## Quick Start

```bash
git clone https://github.com/XNet-NGO/bind-dlz-netbird.git
# See README for build instructions
```

## GitHub Repository

https://github.com/XNet-NGO/bind-dlz-netbird

We welcome feedback from the Netbird community. This is an open-source contribution from XNet Inc., a nonprofit corporation building network infrastructure for underserved communities.

Website: https://xnet.ngo

---

**Questions?** Feel free to ask here or open an issue on GitHub!
