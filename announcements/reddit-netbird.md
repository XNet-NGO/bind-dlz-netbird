# BIND DLZ Plugin for Netbird - Dynamic DNS for VPN Peers

**Post to: r/netbird**

---

Hello,

We have released an open-source BIND 9.18+ DLZ plugin that automatically resolves Netbird peer hostnames to their WireGuard tunnel IPs.

## The Problem

When running a self-hosted Netbird instance, accessing peers by hostname rather than IP address simplifies administration. While Netbird provides built-in DNS, integrating with existing BIND infrastructure requires additional work.

## The Solution

Our DLZ (Dynamically Loadable Zone) plugin fetches peer data directly from the Netbird API and serves it via BIND:

```
$ dig nas.bird.example.com @your-bind-server
;; ANSWER SECTION:
nas.bird.example.com.    60    IN    A    100.105.130.31
```

No zone files to maintain. When peers join the network, DNS updates automatically.

## Features

- Background sync from Netbird API every 5 minutes
- Thread-safe lookups with read-write locks
- Zero-downtime cache updates (atomic pointer swap)
- Compatible with Docker deployments
- Works with self-hosted Netbird instances

## Technical Details

- Written in C using the BIND 9.18+ DLZ dlopen interface
- Uses official BIND headers (dns/dlz_dlopen.h, dns/sdlz.h)
- Links against libcurl, jansson (JSON), and BIND's libdns/libisc

## Configuration

```
dlz "netbird" {
    database "dlopen /usr/lib/bind/netbird_dlz.so 
              bird.example.com 
              YOUR_NETBIRD_API_KEY 
              https://your-netbird-instance/api/peers";
};
```

## Repository

https://github.com/XNet-NGO/bind-dlz-netbird

The README includes build instructions, Docker deployment examples, and a troubleshooting guide.

Feedback and contributions are welcome.

--
XNet Inc.
https://xnet.ngo

XNet is a nonprofit corporation building network infrastructure for underserved communities.
