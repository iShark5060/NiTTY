# NiTTY

## Org standards

CI/README/validate conventions live in AppBase `docs/org-standards/` with personal-repo overrides (`personal-repos.md`). GitHub-hosted `windows-latest` / `ubuntu-latest`, not Blacksmith. Quality gate: `scripts/validate.ps1` (CMake Release). Release **Track B**: manual `.github/workflows/release.yml`. GitHub is `iShark5060/NiTTY`.

## Overview

Windows-first SSH / Telnet / serial terminal forked from PuTTY. Most of the tree is upstream. NiTTY work lives in `nitty_*` sources and thin Windows hooks. Product copy, attribution, and password-obfuscation warning: `README.md`.

## Fork layout

Prefer `windows/nitty_*.c` / `nitty_*.h` over rewriting shared PuTTY files. After `git merge` from upstream (`https://git.tartarus.org/simon/putty.git`, remotes are often `upstream` or `putty`), keep upstream shape on shared files and re-apply NiTTY hunks. Drop NiTTY-only bugfixes in upstream-owned files once equivalent fixes land there.

Windows binaries: `putty` → `NiTTY.exe`, `puttytel` → `NiTTYtel.exe`, `puttygen` → `NiTTYgen.exe`, Windows terminal → `nterm.exe`. Ship version is `LATEST.VER`. `validate.ps1` tries VS 2026 then VS 2022.

Touching `logging.c` for log paths is high merge cost. Portable log resolution lives in `nitty_portable`, `windows/utils/defaults.c`, and `windows/storage.c`. Config file pickers must keep process cwd (`preserve_cwd` in `windows/controls.c`) so relative paths do not follow the last browsed `.ppk` directory.

## Portable config

Enabled when `nitty.ini` next to the exe has `[NiTTY] savemode=dir`. Sample `nitty.ini` documents only keys NiTTY actually reads; most KiTTY.ini toggles are ignored. Colours, fonts, scripts, and passwords live in session storage (files or registry), not in `nitty.ini`.

Portable default log name is `Logs\nitty-&H-&P.log`. Relative leftovers such as `kitty.log` are relocated under `Logs\` on load; absolute paths and paths with `..` are not. Pageant persist (`[Pageant] savemode=dir` + `PersistKeys=1`) stores key **paths** only, never passphrases.

## Nerd Fonts

Line height and cell width are unitless multipliers of font size in px (the em), matching Windows Terminal AtlasEngine, not GDI `tmHeight`. `1.00` / `1.00` is native GDI. Oh My Posh setups typically want `1.20` / `0.60`.

`windows/nitty_termfont.c`: solid Powerline wedges `U+E0B0` / `U+E0B2` are GDI polygons that fill the cell; shades `U+2591`–`U+2593` are an 8×8 dither; outline `U+E0B1`, icons, and `U+E0C7` stay at the session font. NiTTY rasterises glyphs on Windows. Do not install a Nerd Font on the SSH host for this client.
