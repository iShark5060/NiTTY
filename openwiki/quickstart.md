---
type: Quickstart
title: NiTTY quickstart
description: Entry point for agents working on the NiTTY PuTTY fork.
tags: [quickstart]
timestamp: 2026-08-07T00:00:00Z
---

# NiTTY quickstart

Windows-first SSH, Telnet, and serial terminal forked from PuTTY, with dark-mode UI and a small set of KiTTY-inspired QoL features. Most of the tree is upstream; NiTTY work is concentrated in `nitty_*` sources and thin Windows hooks.

## Stack

- C + CMake (Visual Studio 2026 or 2022 generators on Windows)
- Quality gate: `scripts/validate.ps1` (Release configure + build)
- Manual releases: `.github/workflows/release.yml` (Track B)

## Concept pages

| Path | Role |
| ---- | ---- |
| [architecture/overview.md](architecture/overview.md) | Fork layout, `nitty_*` map, merge rules |
| [domain/portable-config.md](domain/portable-config.md) | `nitty.ini` / `savemode=dir`, Logs, Pageant keys |
| [workflows/build-release.md](workflows/build-release.md) | Local build, binaries, release workflow |
| [operations/ci.md](operations/ci.md) | PR validate on `windows-latest` |

## Commands

```powershell
./scripts/validate.ps1
# or manually:
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release --parallel
```

`validate.ps1` also tries Visual Studio 2022 if 2026 is missing. Output binaries land under `build/Release/` (e.g. `NiTTY.exe`).

## Agent gotchas

- Prefer changing `windows/nitty_*.c` / `nitty_*.h` over rewriting shared PuTTY files; re-apply NiTTY hunks after `git merge` from upstream.
- Upstream remote is often named `upstream` or `putty` → `https://git.tartarus.org/simon/putty.git`. Check pending commits with `git log --oneline HEAD..<remote>/main`.
- Sample [`nitty.ini`](../nitty.ini) documents only keys NiTTY actually reads; most legacy KiTTY.ini options are ignored.
- Session logging default in portable mode is under `Logs\` with `nitty-&H-&P.log` — see [portable-config](domain/portable-config.md). Relative `kitty.log` / `putty.log` names are relocated there on load.
- Do not document or commit secrets; portable Pageant persistence stores key **paths** only, never passphrases.
- Ship version is `LATEST.VER` (currently aligned with upstream 0.84 lineage).

Human-facing attribution, passwords-in-sessions warning, and attestation verify steps: root [README.md](../README.md).

## Backlog

| Area | Anchor | Why deferred |
| ---- | ------ | ------------ |
| Session scripts / URL click | `windows/nitty_script.c`, `windows/nitty_url.c` | Stable QoL; document when behavior changes |
| Dark-theme internals | `windows/nitty_config_theme.c` | Implementation detail; follow win32-darkmodelib ideas in README |
| Full PuTTY protocol map | `ssh/`, `terminal/` | Upstream-owned; use PuTTY docs |
