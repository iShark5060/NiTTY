---
type: Operations
title: CI and PR checks
description: Windows validate jobs on GitHub-hosted runners.
tags: [operations, ci]
timestamp: 2026-08-07T00:00:00Z
---

# CI and PR checks

- [`pr.yml`](../../.github/workflows/pr.yml) — on `pull_request`, `windows-latest`, `actions/checkout@v7`, run `scripts/validate.ps1` (30m timeout, concurrency cancels in-progress runs on the same head)
- [`release.yml`](../../.github/workflows/release.yml) — Track B manual release; prepare on `ubuntu-latest`, build/attest/publish on `windows-latest`

No push-to-main CI workflow. Org preference is GitHub-hosted `windows-latest` / `ubuntu-latest` (not Blacksmith). See [build-release](../workflows/build-release.md) for what validate and release produce.
