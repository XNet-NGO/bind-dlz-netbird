# BIND DLZ Netbird Plugin

A high-performance, custom BIND 9 Dynamically Loadable Zone (DLZ) plugin that integrates directly with the [Netbird](https://netbird.io/) Management API. This plugin allows BIND to resolve DNS queries for Netbird peers in real-time without manual zone file management or server restarts.

## Features

*   **Real-time Integration**: Fetches peer data directly from the Netbird API.
*   **High Performance**: Implements a "Dual-Plane" architecture.
    *   **Management Plane**: Background thread handles API fetching and JSON parsing separate from the query path.
    *   **Data Plane**: DNS lookups are served from an in-memory, thread-safe shadow table (Read-Write Lock optimized) with $O(n)$ lookup time.
*   **Resilient**: If the Netbird API goes down, the plugin continues to serve the last known good cache.
*   **Smart Label Handling**: automatically sanitizes Netbird hostnames (converting spaces to hyphens) and strips FQDNs to match relative labels correctly.

## Architecture

The plugin operates by efficiently bridging slow HTTP APIs with fast DNS protocols:

1.  **Background Thread**: Wakes up every 5 minutes (configurable in source) to fetch the peer list.
2.  **Atomic Swap**: Updates the in-memory lookup table using an atomic pointer swap, ensuring no locking contention during updates.
3.  **Zero-Latency Lookups**: BIND threads read from memory, delivering sub-millisecond response times.

## Prerequisites

To build this plugin, you need a Linux environment (or WSL2) with GCC and development headers for BIND's dependencies.

*   **BIND 9** (headers are included via `dlz_minimal.h` for portability)
*   **libcurl** (for API requests)
*   **libjansson** (for JSON parsing)
*   **pthread** (standard in glibc)

### Install Dependencies (Debian/Ubuntu/WSL)

```bash
sudo apt-get update
sudo apt-get install build-essential libcurl4-openssl-dev libjansson-dev
```

## Build Instructions

1.  Clone the repository:
    ```bash
    git clone https://github.com/XNet-NGO/bind-dlz-netbird.git
    cd bind-dlz-netbird
    ```

2.  Compile the shared library:
    ```bash
    make
    ```

    This will produce `netbird_dlz.so`.

3.  (Optional) Install to system library path:
    ```bash
    sudo cp netbird_dlz.so /usr/lib/bind/
    ```

## Configuration

Edit your BIND configuration file (usually `/etc/bind/named.conf.local` or `/etc/named.conf`) and add a `dlz` zone definition.

### Syntax

```bind
dlz "netbird_zone" {
    database "dlopen /path/to/netbird_dlz.so <ZONE_NAME> <API_KEY> [API_URL]";
};
```

*   **ZONE_NAME**: The DNS zone you are serving (e.g., `vpn.xnet.ngo`).
*   **API_KEY**: Your Netbird Personal Access Token (PAT).
*   **API_URL** (Optional): Defaults to `https://api.netbird.io/api/peers`. Useful if you host self-hosted Netbird.

### Example Configuration

```bind
dlz "netbird_zone" {
    database "dlopen /usr/lib/bind/netbird_dlz.so vpn.xnet.ngo nb_pwt_xxxxxxxxxxxxxxxxxxxxxxxxxx";
};
```

## Usage

Once configured, restart BIND:

```bash
sudo systemctl restart bind9
```

Test a query using `dig`. If you have a Netbird peer named "NAS", it should resolve at `nas.vpn.xnet.ngo`:

```bash
dig @localhost nas.vpn.xnet.ngo +short
# Output: 100.64.0.5
```

## Troubleshooting

*   **Logs**: The plugin logs to BIND's standard logging facility (usually `/var/log/syslog` or systemd journal). Look for "Netbird DLZ" messages.
    ```bash
    journalctl -u bind9 -f
    ```
*   **Permission Denied**: Ensure the user running BIND (usually `bind` or `named`) has read/execute permissions on `netbird_dlz.so`.
    ```bash
    sudo chown root:bind /usr/lib/bind/netbird_dlz.so
    sudo chmod 750 /usr/lib/bind/netbird_dlz.so
    ```
*   **API Errors**: If the API token is invalid, check logs for HTTP 401/403 errors.

## License

Copyright (C) XNet NGO.

Permission to use, copy, modify, and distribute this software for any purpose with or without fee is hereby granted, provided that the above copyright notice and this permission notice appear in all copies.
