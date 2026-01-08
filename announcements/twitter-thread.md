# Twitter/X Thread

## Tweet 1 (Main)
🚀 Just open-sourced: BIND DLZ plugin for @netaborig VPN

Automatically resolve peer hostnames via DNS:
nas.vpn.example.com → 100.64.0.5

No zone files. No manual updates. Just works.

🔗 https://github.com/XNet-NGO/bind-dlz-netbird

🧵 Thread with details...

---

## Tweet 2
How it works:

1️⃣ Background thread polls Netbird API every 5 min
2️⃣ Caches hostname→IP mappings in memory
3️⃣ BIND serves queries from cache (sub-ms latency)
4️⃣ Thread-safe with read-write locks

New peer joins network? DNS updates automatically.

---

## Tweet 3
Tech stack:
• C with pthreads
• libcurl for API calls
• Jansson for JSON
• BIND 9.18+ DLZ API

Single .so file, Docker-ready 🐳

---

## Tweet 4
This is @XNetNGO's first open-source release!

We're a nonprofit building network infrastructure. Feedback and contributions welcome!

#selfhosted #dns #wireguard #homelab #opensource
