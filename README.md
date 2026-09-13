# NiTTY

[![PR](https://img.shields.io/github/actions/workflow/status/iShark5060/NiTTY/pr.yml?style=flat-square&label=PR)](https://github.com/iShark5060/NiTTY/actions/workflows/pr.yml)
![CMake](https://img.shields.io/badge/CMake-3.x-064F8C?logo=cmake&logoColor=white&style=flat-square)
![Windows](https://img.shields.io/badge/Windows-x64-0078D6?logo=windows&logoColor=white&style=flat-square)
[![Cursor](https://img.shields.io/badge/Cursor-IDE-141414?logo=cursor&logoColor=white&style=flat-square)](https://cursor.com)

NiTTY is a Windows-first SSH, Telnet, and serial terminal based on the [PuTTY](https://www.chiark.greenend.org.uk/~sgtatham/putty/) codebase. Same protocols, same reliability. A dark configuration UI, and extras inspired by community forks, especially [KiTTY](https://www.9bis.net/kitty/).

If you already live in PuTTY, this should feel familiar. Sessions, Pageant, Plink, PuTTYgen. The window chrome is just less 1999.

If you use NiTTY in research, documentation, or redistribution, please cite **PuTTY** as the upstream project and acknowledge **KiTTY** where features trace to that ecosystem. Windows dark-mode behaviour draws on ideas documented in **[win32-darkmodelib](https://github.com/ozone10/win32-darkmodelib)**.

NiTTY is not affiliated with the official PuTTY team or the KiTTY project.

## Why NiTTY?

- **PuTTY's core.** SSH, Telnet, Rlogin, SUPDUP, serial, Pageant, Plink, PuTTYgen, and the same configuration model.
- **Refreshed UI.** Windows 11-style dark configuration dialogs, consistent theming across NiTTYgen, Pageant, and the terminal.
- **Portable and session-friendly.** Optional portable layout (ini + session files) in the spirit of KiTTY-style workflows.
- **Windows terminal binary.** Built as `nterm.exe` with `nterm` / `ntermcfg` icons. PuTTY upstream uses `pterm` on Windows; NiTTY standardises on `nterm`.
- **Extra window and session options.** Layered transparency, minimize-to-tray, clickable URLs, and RuTTY-style session scripts (KiTTY-compatible keywords in storage).
- **Nerd Fonts.** Line height and cell width use Windows Terminal's em-based cell model, so Powerline / Oh My Posh prompts line up instead of stretching or clipping glyphs.

## Nerd Fonts

There are several ways to draw Nerd Font / Powerline glyphs in a terminal. NiTTY follows [Windows Terminal](https://github.com/microsoft/terminal) on purpose. Cell size is a pair of unitless multipliers of font size in px (the em), not of GDI `tmHeight`. Shade blocks and solid Powerline wedges fill the cell as geometry. Outline chevrons, OS icons, and other private-use glyphs stay at the font size.

Under **Window → Appearance**, set **Line height** and **Cell width**. `1.00` / `1.00` is the font's native GDI cell (stock PuTTY: `tmAveCharWidth` × `tmHeight`). A typical Nerd Font + Oh My Posh setup is `1.20` / `0.60`, matching Windows Terminal's em multipliers. Pick a Nerd Font in the same panel.

## Attribution

| Project                                                               | Role                                                                                                                                                                               |
| --------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **[PuTTY](https://www.chiark.greenend.org.uk/~sgtatham/putty/)**      | Original design, protocols, security model, and the majority of the source tree. Copyright © 1997– Simon Tatham and contributors.                                                  |
| **[KiTTY](https://www.9bis.net/kitty/)**                              | A long-running PuTTY fork that popularized many Windows UX and session features. NiTTY adopts ideas and compatibility hooks from that lineage.                                     |
| **[win32-darkmodelib](https://github.com/ozone10/win32-darkmodelib)** | A C++ library for dark mode and themed Win32 controls. NiTTY does not ship it as a dependency, but its techniques informed the Windows configuration UI.                           |

Upstream PuTTY remains the reference for behaviour, security updates, and documentation unless this repository states otherwise. When reporting security-sensitive issues, consider whether they belong in upstream PuTTY first.

## Building

CMake 3.x and Visual Studio 2022 or 2026 (Desktop development with C++).

```bash
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release
```

`"Visual Studio 17 2022"` remains supported. The GitHub Actions release workflow auto-detects between VS 2026 and VS 2022.

See the plain [`README`](README) file in this directory for Unix notes and Halibut documentation builds.

## Verifying release downloads

Official Windows x64 release zips are built by [`.github/workflows/release.yml`](.github/workflows/release.yml) and signed with [GitHub Artifact Attestations](https://docs.github.com/en/actions/security-for-github-actions/using-artifact-attestations/using-artifact-attestations-to-establish-provenance-for-builds).

After downloading a release zip from GitHub, verify it with the [GitHub CLI](https://cli.github.com/) (v2.49.0 or newer):

```bash
gh attestation verify NiTTY-0.83-win64.zip --repo iShark5060/NiTTY
```

Replace `0.83` with the release version and use the actual zip filename from the release.

## Licence

NiTTY inherits PuTTY's licence. See the [`LICENCE`](LICENCE) file in this repository.

## Saved sessions and SSH passwords

NiTTY can store the SSH login password in a saved session (the same `Password` field used on **Connection → Data**), so it is written to the Windows registry or to portable session files alongside other settings.

That value is not stored in plain text. It is obfuscated (XOR plus Base64, keyed by the session name) before being saved. That makes it harder to accidentally copy a readable password out of a config export.

This is not encryption you should trust for secrecy. The obfuscation can be reversed by anyone who can read the source or the running binary, or who controls the machine. Treat it as a convenience and casual deterrent. For real secrets, use SSH keys or a password manager.

## Pageant (Windows)

- **Theming.** Pageant uses the same dark (or light) configuration style as NiTTY-subclassed controls, so it does not look like a half-themed system dialog beside the rest of the suite.
- **Portable key paths.** If you use directory-based portable config (`savemode=dir` in `nitty.ini`), you can optionally ask Pageant to reload a list of private key files on startup. Enable this under the `[Pageant]` section (`savemode=dir` + `PersistKeys=1`). Paths are stored in `<configdir>\Pageant\pageant-keys.txt` (UTF-8, one path per line).
- **Passphrases are not saved.** That file stores only paths to key files. Unlocked keys and remembered passphrases behave like stock Pageant: they live in memory for the running process.

`nitty.ini` is only for portable/bootstrap flags. Session colours, SSH options, and most behaviour still come from saved sessions or the registry.

## Links

- PuTTY home: <https://www.chiark.greenend.org.uk/~sgtatham/putty/>
- KiTTY: <https://www.9bis.net/kitty/>
- win32-darkmodelib: <https://github.com/ozone10/win32-darkmodelib>
- CMake: <https://cmake.org/>
