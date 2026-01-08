Subject: [ANN] BIND DLZ Plugin for Netbird VPN - Dynamic Peer DNS Resolution

Hello BIND community,

I'd like to announce the release of a new open-source DLZ driver for BIND 9.18+ that provides dynamic DNS resolution for Netbird VPN peers.

## Overview

Netbird (https://netbird.io) is an open-source WireGuard-based mesh VPN. This DLZ plugin fetches peer information from the Netbird API and serves A records for peer hostnames, allowing DNS queries like:

    nas.vpn.example.com -> 100.64.0.5

## Technical Details

- Written in C using the BIND 9.18+ DLZ dlopen interface
- Uses official BIND headers (dns/dlz_dlopen.h, dns/sdlz.h)
- Background pthread fetches from Netbird API every 5 minutes
- Thread-safe lookups using pthread_rwlock
- Atomic cache updates (pointer swap pattern)
- Links against libcurl, jansson (JSON), and BIND's libdns/libisc

## Configuration Example

    dlz "netbird" {
        database "dlopen /usr/lib/bind/netbird_dlz.so 
                  bird.example.com 
                  YOUR_NETBIRD_API_KEY 
                  https://your-netbird-instance/api/peers";
    };

## Repository

https://github.com/XNet-NGO/bind-dlz-netbird

The README includes:
- Build instructions for BIND 9.18+
- Docker deployment example
- Troubleshooting guide

## Compatibility

Tested with BIND 9.18.39 on Ubuntu 22.04 (ARM64). Should work with any BIND 9.18+ release using the modern DLZ API with dns_sdlzlookup_t.

Feedback and contributions are welcome.

-- 
XNet Inc.
https://xnet.ngo

XNet is a nonprofit corporation building network infrastructure for underserved communities.
