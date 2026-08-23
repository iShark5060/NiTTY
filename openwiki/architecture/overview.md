---
type: Architecture Overview
title: NiTTY fork layout
description: Where PuTTY upstream and NiTTY-specific code live.
tags: [architecture]
timestamp: 2026-08-23T20:00:00Z
---

# NiTTY fork layout

Most of the tree is upstream PuTTY. Keep upstream structure on shared files; put NiTTY behavior in dedicated sources and small platform hooks.

Depends on [portable-config](../domain/portable-config.md) for directory-mode storage details. Build/release is covered in [build-release](../workflows/build-release.md).

## Where to start

| Area | Start here |
| ---- | ---------- |
| Portable / dir config | `windows/nitty_portable.c`, sample `nitty.ini` |
| Session files on disk | `windows/nitty_sessfile.c`, `windows/storage.c` (dir-mode branches) |
| Dark config UI | `windows/nitty_config_theme.c`, `windows/controls.c`, `windows/dialog.c` |
| Win11 chrome / extras | `windows/nitty_winfeat.c` |
| Login scripts | `windows/nitty_script.c` |
| URL open from terminal | `windows/nitty_url.c` |
| Terminal cell metrics / Powerline | `windows/nitty_termfont.c`, thin hooks in `windows/window.c` |
| Saved SSH password field | `windows/nitty_autologin.c`, `utils/nitty_sesspass.c` |
| About strings | `nitty_about.h` |

Binary renames (Windows): `putty` → `NiTTY.exe`, `puttytel` → `NiTTYtel.exe`, `puttygen` → `NiTTYgen.exe`, Windows terminal → `nterm.exe` (see `windows/CMakeLists.txt`, `build/shipped.txt`).

## Upstream merge

Upstream: `https://git.tartarus.org/simon/putty.git` (clone remotes may be named `upstream` or `putty`).

1. `git fetch <remote>`
2. `git log --oneline HEAD..<remote>/main` — pending commits
3. `git merge <remote>/main` — resolve conflicts preferring upstream shape; re-apply NiTTY hunks
4. Confirm `LATEST.VER`, run `scripts/validate.ps1`, smoke-test portable mode / theming / scripts / Pageant persist

Drop NiTTY-only bugfixes in upstream-owned files once equivalent fixes land upstream (README notes examples under `proxy/`, subprocess helpers, etc.).

## Watch out for

- Touching `logging.c` for log paths is high merge cost; portable log resolution lives in `nitty_portable` + `windows/utils/defaults.c` + `windows/storage.c`.
- Config file pickers should preserve process cwd (`preserve_cwd` in `windows/controls.c`) so relative paths do not follow the last browsed `.ppk` directory.
- Event Log and host-key dialogs use the shared theme helpers in `windows/dialog.c`; About was already themed separately.
