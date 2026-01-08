Show HN: BIND DLZ Plugin for Netbird VPN – Dynamic DNS for Mesh Networks

https://github.com/XNet-NGO/bind-dlz-netbird

We built a BIND 9.18+ DLZ driver that fetches peer data from Netbird's API and serves DNS records dynamically. Query "myserver.vpn.example.com" and get back the WireGuard tunnel IP.

Technical highlights:
- C with pthreads for background API polling (5-min intervals)
- Read-write locks for thread-safe lookups
- Atomic pointer swap for zero-downtime cache updates
- Uses official BIND headers (dns/sdlz.h) and dns_sdlz_putrr()

Netbird is an open-source WireGuard mesh VPN (like Tailscale). This plugin lets you integrate peer discovery with existing BIND infrastructure.

Built for our nonprofit's network but hopefully useful to others running self-hosted Netbird instances.
