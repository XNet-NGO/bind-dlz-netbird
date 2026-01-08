# Netbird Slack Message

**Channel: #general or #integrations**

---

Hello,

We have released an open-source BIND DLZ plugin for Netbird:

https://github.com/XNet-NGO/bind-dlz-netbird

**What it does:**
- Polls your Netbird API for peer data
- Serves DNS records via BIND
- Resolves `myserver.bird.example.com` to `100.64.0.x`

**Use case:**
We run a self-hosted Netbird instance and needed to integrate with our existing BIND infrastructure. This plugin enables automatic hostname resolution for all peers.

**Example:**
```
$ dig nas.bird.xnet.ngo
;; ANSWER SECTION:
nas.bird.xnet.ngo.    60    IN    A    100.105.130.31
```

Compatible with BIND 9.18+. The README includes Docker deployment instructions.

Feedback and contributions are welcome.

--
XNet Inc. (https://xnet.ngo)
a nonprofit building network infrastructure
