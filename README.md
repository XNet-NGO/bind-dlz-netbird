# BIND DLZ Netbird Plugin

A high-performance BIND 9 Dynamically Loadable Zone (DLZ) plugin that integrates with the [Netbird](https://netbird.io/) VPN Management API. This plugin enables automatic DNS resolution for all Netbird peers without manual zone file management.

## Features

*   **Real-time Integration**: Fetches peer data directly from the Netbird API every 5 minutes
*   **High Performance**: "Dual-Plane" architecture separates API fetching from DNS queries
    *   **Management Plane**: Background thread handles API fetching and JSON parsing
    *   **Data Plane**: DNS lookups served from thread-safe in-memory cache with sub-millisecond response times
*   **Resilient**: Continues serving last known good cache if the Netbird API goes down
*   **BIND 9.18+ Compatible**: Uses official BIND DLZ dlopen API with proper `dns_sdlz_putrr()` integration
*   **Case-insensitive**: Hostname lookups work regardless of case (e.g., `IndigoStation` matches `indigostation`)

## Architecture

```
┌─────────────────┐     ┌──────────────────┐     ┌─────────────────┐
│  Netbird API    │────▶│  Background      │────▶│  In-Memory      │
│  (every 5 min)  │     │  Thread          │     │  Record Cache   │
└─────────────────┘     └──────────────────┘     └────────┬────────┘
                                                          │ rwlock
                                                          ▼
┌─────────────────┐     ┌──────────────────┐     ┌─────────────────┐
│  DNS Query      │────▶│  dlz_lookup()    │────▶│  BIND Response  │
│  (dig, nslookup)│     │  (< 1ms)         │     │  (A record)     │
└─────────────────┘     └──────────────────┘     └─────────────────┘
```

## Prerequisites

**BIND 9.18+** with development headers:

```bash
# Debian/Ubuntu
sudo apt-get install bind9 bind9-dev libcurl4-openssl-dev build-essential

# Jansson is built statically to avoid symbol conflicts with BIND's libjson-c
wget https://github.com/akheron/jansson/releases/download/v2.14/jansson-2.14.tar.gz
tar -xzf jansson-2.14.tar.gz && cd jansson-2.14
./configure --disable-shared --enable-static --with-pic CFLAGS="-fvisibility=hidden"
make && sudo make install
```

## Build Instructions

1.  Clone the repository:
    ```bash
    git clone https://github.com/XNet-NGO/bind-dlz-netbird.git
    cd bind-dlz-netbird
    ```

2.  Compile the shared library:
    ```bash
    gcc -fPIC -shared -o netbird_dlz.so netbird_dlz.c \
        -I/usr/include/bind9 -I/usr/include \
        -lcurl /usr/local/lib/libjansson.a \
        -ldns -lisc
    ```

3.  Install to system library path:
    ```bash
    sudo cp netbird_dlz.so /usr/lib/netbird_dlz.so
    sudo chmod 644 /usr/lib/netbird_dlz.so
    ```

## Configuration

Add a DLZ block to your BIND configuration (`/etc/bind/named.conf` or `/etc/bind/named.conf.local`):

```bind
dlz "netbird" {
    database "dlopen /usr/lib/netbird_dlz.so bird.example.com YOUR_API_KEY https://your-netbird.example.com/api/peers";
    search yes;
};
```

**Parameters:**
| Parameter | Description |
|-----------|-------------|
| `bird.example.com` | The DNS zone to serve (e.g., `bird.xnet.ngo`) |
| `YOUR_API_KEY` | Netbird API token (from Settings → Personal Access Tokens) |
| `https://...` | Netbird API URL (optional, defaults to `https://api.netbird.io/api/peers`) |

**Important:** Add `search yes;` to allow BIND to search the DLZ for any query in the zone.

## Docker Deployment

See `Dockerfile.bind` for a complete containerized deployment example that:
- Builds Jansson from source with hidden visibility
- Compiles the DLZ plugin with proper BIND 9.18+ headers
- Includes Webmin for DNS management

## Usage

Restart BIND after configuration:

```bash
sudo systemctl restart bind9
```

Test DNS resolution for your Netbird peers:

```bash
# Query a peer by hostname
dig @localhost myserver.bird.example.com A +short
# Output: 100.105.x.x

# Full response with TTL
dig @localhost myserver.bird.example.com A
```

## Debug Logging

The plugin writes debug logs to `/tmp/dlz.log`:

```bash
tail -f /tmp/dlz.log
```

API responses are cached to `/tmp/netbird_debug.json` for inspection.

## Troubleshooting

| Issue | Solution |
|-------|----------|
| `SERVFAIL` response | Check `/tmp/dlz.log` for errors. Verify API key is valid. |
| Container crashes with exit code 139 | Segfault - ensure using BIND 9.18+ headers and linking `-ldns -lisc` |
| Records not updating | Background thread fetches every 5 min. Check API connectivity. |
| Case sensitivity | Lookups are case-insensitive. `MyServer` matches `myserver`. |

## License

Copyright (C) 2026 XNet Inc.

Permission to use, copy, modify, and distribute this software for any purpose with or without fee is hereby granted, provided that the above copyright notice and this permission notice appear in all copies.
