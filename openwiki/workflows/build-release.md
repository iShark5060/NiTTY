---
type: Workflow
title: Build and release
description: CMake local builds and manual GitHub Releases.
tags: [workflows, release]
timestamp: 2026-08-07T00:00:00Z
---

# Build and release

Depends on [architecture/overview.md](../architecture/overview.md). PR validation is described in [operations/ci.md](../operations/ci.md).

## Local build

Quality gate (preferred):

```powershell
./scripts/validate.ps1
```

This requires `LATEST.VER`, configures with Visual Studio 2026 or 2022 (`-A x64`), and builds Release. Main GUI binary: `build/Release/NiTTY.exe`. Shipped name list: `build/shipped.txt` (`NiTTY.exe`, `NiTTYtel.exe`, `NiTTYgen.exe`, `pageant.exe`, `plink.exe`, `pscp.exe`, `psftp.exe`, `nterm.exe`).

## Release

Manual Track B: `workflow_dispatch` on `.github/workflows/release.yml`.

- Inputs: optional `version` (else `LATEST.VER`), `prerelease`, `draft`
- Version must match `X.YY`
- Windows job builds, attests, verifies, and publishes the zip (no Actions artifact handoff between jobs)
- Consumers verify with `gh attestation verify … --repo iShark5060/NiTTY` (see README)

After cutting a release aligned with upstream, keep `LATEST.VER` consistent with the intended ship lineage.
