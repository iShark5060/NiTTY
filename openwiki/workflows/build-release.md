---
type: Workflow
title: Build and release
description: CMake local builds and manual GitHub Releases.
tags: [workflows, release]
timestamp: 2026-07-21T00:00:00Z
---

# Build and release

Depends on [architecture/overview.md](../architecture/overview.md).

Local Windows build uses CMake + Visual Studio 2026 or 2022. Manual releases use `workflow_dispatch` on `.github/workflows/release.yml` (version / prerelease / draft inputs), then `iShark5060/actions-gh-release@v1`.

PR/CI call `scripts/validate.ps1` — see [operations/ci.md](../operations/ci.md).
