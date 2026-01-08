# Netbird Slack Message

**Channel: #general or #integrations**

---

Hey everyone! 👋

Just released an open-source BIND DNS plugin for Netbird:

**🔗 https://github.com/XNet-NGO/bind-dlz-netbird**

**What it does:**
- Polls your Netbird API for peer data
- Serves DNS records via BIND
- `myserver.bird.example.com` → `100.64.0.x`

**Why?**
We run a self-hosted Netbird instance and wanted to integrate with our existing BIND infrastructure. Now peers are automatically resolvable by hostname!

**Quick example:**
```
$ dig nas.bird.xnet.ngo
;; ANSWER SECTION:
nas.bird.xnet.ngo.    60    IN    A    100.105.130.31
```

Works with BIND 9.18+. README has Docker deployment example.

First time open-sourcing something - feedback welcome! 🙏
