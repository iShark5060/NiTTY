---
type: Architecture Overview
title: NiTTY fork layout
description: Where PuTTY upstream and NiTTY-specific code live.
tags: [architecture]
timestamp: 2026-07-21T00:00:00Z
---

# NiTTY fork layout

Most of the tree is upstream PuTTY. NiTTY-specific pieces cluster in `nitty_*.c` / `nitty_*.h`, `windows/nitty_*.c`, icons under `_Resources/`, and release packaging (`build/shipped.txt`, `LATEST.VER`).

Upstream remote: `https://git.tartarus.org/simon/putty.git`. Prefer keeping upstream structure on shared files and re-applying NiTTY hunks after merges.
