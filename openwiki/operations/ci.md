---
type: Operations
title: CI and PR checks
description: Windows validate jobs on GitHub-hosted runners.
tags: [operations, ci]
timestamp: 2026-07-29T08:20:00Z
---

# CI and PR checks

- `pr.yml` — `windows-latest`, `actions/checkout@v7`, run `scripts/validate.ps1`
- `release.yml` — Track B manual release; Windows job builds, attests, verifies, and publishes (no Actions artifact handoff)

No push-to-main CI workflow and no Blacksmith runners.
