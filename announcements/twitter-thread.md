# Twitter/X Thread

## Tweet 1 (Main)
New open-source release: BIND DLZ plugin for @netaborig VPN

Automatically resolve peer hostnames via DNS:
nas.vpn.example.com -> 100.64.0.5

No zone files. No manual updates.

https://github.com/XNet-NGO/bind-dlz-netbird

Thread:

---

## Tweet 2
How it works:

1. Background thread polls Netbird API every 5 min
2. Caches hostname-to-IP mappings in memory
3. BIND serves queries from cache (sub-ms latency)
4. Thread-safe with read-write locks

New peer joins the network? DNS updates automatically.

---

## Tweet 3
Technical stack:
- C with pthreads
- libcurl for API calls
- Jansson for JSON parsing
- BIND 9.18+ DLZ API

Single .so file, Docker-ready.

---

## Tweet 4
Developed by @XNetNGO, a nonprofit building network infrastructure for underserved communities.

Feedback and contributions welcome.

https://xnet.ngo

#selfhosted #dns #wireguard #opensource
