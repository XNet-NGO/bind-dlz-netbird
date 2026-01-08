Show HN: BIND DLZ Plugin for Netbird VPN - Dynamic DNS for Mesh Networks

https://github.com/XNet-NGO/bind-dlz-netbird

We developed a BIND 9.18+ DLZ driver that fetches peer data from Netbird's API and serves DNS records dynamically. Query "myserver.vpn.example.com" and receive the WireGuard tunnel IP.

Technical details:
- C with pthreads for background API polling (5-minute intervals)
- Read-write locks for thread-safe lookups
- Atomic pointer swap for zero-downtime cache updates
- Uses official BIND headers (dns/sdlz.h) and dns_sdlz_putrr()

Netbird is an open-source WireGuard mesh VPN. This plugin enables integration of peer discovery with existing BIND infrastructure.

Developed by XNet Inc. (https://xnet.ngo), a nonprofit building network infrastructure for underserved communities.
