---
type: Quickstart
title: NiTTY quickstart
description: Entry point for agents working on the NiTTY PuTTY fork.
tags: [quickstart]
timestamp: 2026-07-21T00:00:00Z
---

# NiTTY quickstart

Windows-first SSH, Telnet, and serial terminal based on PuTTY, with dark-mode UI and KiTTY-inspired QoL features.

## Stack

- C + CMake (Visual Studio generators on Windows)
- Release packaging via `.github/workflows/release.yml` (Track B)
- Validate: `scripts/validate.ps1` (Release build)

## Layout

| Path | Role |
| ---- | ---- |
| [architecture/overview.md](architecture/overview.md) | Fork layout and NiTTY-specific files |
| [workflows/build-release.md](workflows/build-release.md) | Local build and manual release |
| [operations/ci.md](operations/ci.md) | PR/CI on `windows-latest` |

## Commands

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release
./scripts/validate.ps1
```

See root [README.md](../README.md) for attribution and upstream sync.
