---
type: Operations
title: CI and PR checks
description: Windows validate jobs on GitHub-hosted runners.
tags: [operations, ci]
timestamp: 2026-07-21T00:00:00Z
---

# CI and PR checks

- `pr.yml` / `ci.yml` — `windows-latest`, `actions/checkout@v7`, run `scripts/validate.ps1`
- `release.yml` — Track B manual release (unchanged pattern)

No Blacksmith runners.
