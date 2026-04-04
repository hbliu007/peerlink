# PeerLink Open-Source Documentation Design Spec

**Date**: 2026-04-04
**Status**: Approved
**Framework**: MkDocs Material + mkdocs-i18n
**Language**: Chinese (primary) + English (bilingual)

---

## 1. Goals

Build a production-grade open-source documentation site for PeerLink P2P Platform, targeting:
- **Developers** integrating PeerLink SDK into their products
- **Ops engineers** deploying and maintaining PeerLink services
- **Technical decision-makers** evaluating PeerLink vs alternatives

## 2. Architecture: Diataxis Four-Quadrant

```
┌────────────────────────────┬────────────────────────────────┐
│  Tutorials (learning)      │  How-to Guides (task-oriented)  │
│  "Follow along to learn"   │  "Solve a specific problem"     │
├────────────────────────────┼────────────────────────────────┤
│  Concepts (understanding)  │  Reference (information)        │
│  "Understand how it works" │  "Look up API/config/proto"     │
└────────────────────────────┴────────────────────────────────┘
```

## 3. Site Structure

```
docx/
├── mkdocs.yml
├── requirements.txt
├── docs/
│   ├── index.md                  # Landing page
│   ├── tutorials/                # 6 pages
│   │   ├── quick-start.md
│   │   ├── ssh-tunnel.md
│   │   ├── mobile-connect.md
│   │   ├── multi-device.md
│   │   └── python-sdk.md
│   ├── how-to/                   # 7 pages
│   │   ├── deploy-aliyun.md
│   │   ├── configure-tls.md
│   │   ├── firewall-traversal.md
│   │   ├── rate-limiting.md
│   │   ├── monitoring.md
│   │   ├── security-hardening.md
│   │   └── build-from-source.md
│   ├── concepts/                 # 8 pages
│   │   ├── how-peerlink-works.md # CORE long-form article
│   │   ├── architecture.md
│   │   ├── nat-traversal.md
│   │   ├── dcutr-protocol.md
│   │   ├── circuit-relay.md
│   │   ├── security-model.md
│   │   ├── performance.md
│   │   └── comparisons.md
│   ├── reference/                # 7 pages
│   │   ├── c-api.md
│   │   ├── python-api.md
│   │   ├── config.md
│   │   ├── cli.md
│   │   ├── protocols.md
│   │   ├── faq.md
│   │   └── changelog.md
│   ├── contributing/             # 3 pages
│   │   ├── index.md
│   │   ├── code-of-conduct.md
│   │   └── documentation.md
│   └── assets/
│       ├── images/
│       └── diagrams/
├── i18n/en/                      # English translations
└── overrides/partials/           # Custom templates
```

Total: 31 pages x 2 languages = 62 content files

## 4. Visual Design

- **Framework**: MkDocs Material
- **Primary color**: `#2196F3` (trust blue)
- **Accent color**: `#00BFA5` (connection green)
- **Dark mode**: Supported
- **Navigation**: Top nav bar + sidebar TOC
- **Search**: Built-in (mkdocs-material search)
- **i18n**: Language switcher (CN/EN) via mkdocs-i18n plugin

## 5. Landing Page Layout

1. Hero: Project name + tagline + 2 CTAs (Quick Start / How It Works)
2. Feature highlights: 3 key value propositions with icons
3. 3-Step Quick Start: Deploy → Install → Connect
4. Architecture diagram (Excalidraw rendered)
5. Comparisons teaser: vs Tailscale / vs FRP / vs ZeroTier
6. Footer: License, links, stats

## 6. Key Content Pages

### 6.1 how-peerlink-works.md (CORE)
Tailscale-style narrative article covering:
- Problem: Enterprise firewalls block inbound connections
- Solution: Three-layer fallback (UDP punch → TCP punch → TCP 443 relay)
- Architecture: 6-layer stack (Core → Protocol → Transport → NAT → Security → Platform)
- Security: Ed25519 identity, TLS 1.3, SignedEnvelope
- Performance: Benchmarks vs Python baseline

### 6.2 comparisons.md
Honest comparison table covering:
- PeerLink vs Tailscale (self-hosted vs SaaS)
- PeerLink vs FRP (P2P vs always-relay)
- PeerLink vs ZeroTier (protocol level)

### 6.3 config.md (Nebula-style)
Every TOML field documented with:
- Type, default value, required/optional
- Example configuration snippet
- Cross-references to related how-to guides

## 7. Team Assignment

| Role | Member | Scope | Deliverables |
|------|--------|-------|-------------|
| Team Lead | Dr. John | mkdocs.yml, index.md, how-peerlink-works.md, architecture.md, comparisons.md | Framework + 5 core pages |
| Doc Engineer A | Alice | tutorials/ (6) + contributing/ (3) | 9 pages |
| Doc Engineer B | Bob | how-to/ (7) + reference/ (7) | 14 pages |
| Visual Designer | Eve | Landing page, architecture diagrams, flow charts, overrides/ templates | Visual assets + 3 pages |

## 8. Quality Gates

- Every page has both CN and EN versions
- Every page has proper frontmatter (title, description, tags)
- Code examples are tested and runnable
- Architecture diagrams use Excalidraw (source in assets/diagrams/)
- TOML config reference covers every field with type/default/example
- Comparisons are honest and factual

## 9. Archive Plan

Move existing `docs/` to `docx/archive/` for reference. Do not delete — historical context is valuable.

## 10. Build & Deploy

```bash
cd docx
pip install -r requirements.txt
mkdocs serve     # Dev server on localhost:8000
mkdocs build     # Static site to docx/site/
```
