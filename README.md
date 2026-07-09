# NiTTY

NiTTY is a **Windows-first SSH, Telnet, and serial terminal client** based on the [PuTTY](https://www.chiark.greenend.org.uk/~sgtatham/putty/) codebase. It keeps PuTTY’s reliability and protocol support while refreshing the experience with a modern dark-themed configuration UI and a set of quality-of-life features inspired by community forks—especially [KiTTY](https://www.9bis.net/kitty/).

If you use NiTTY in research, documentation, or redistribution, please cite **PuTTY** as the upstream project and acknowledge **KiTTY** where features trace to that ecosystem. Windows dark-mode behaviour and control theming draw on ideas documented in **[win32-darkmodelib](https://github.com/ozone10/win32-darkmodelib)** (see below).

---

## Why NiTTY?

- **PuTTY’s core**: SSH, Telnet, Rlogin, SUPDUP, serial, Pageant, Plink, PuTTYgen workflow, and the same general configuration model.
- **Refreshed UI**: Windows 11–style dark configuration dialogs, consistent theming across tools (NiTTYgen, **Pageant**, and the terminal), and polished window chrome where supported.
- **Portable & session-friendly**: Optional portable layout (ini + session files) in the spirit of KiTTY-style workflows.
- **Windows terminal binary**: Built as **nterm.exe** with **nterm** / **ntermcfg** icons (PuTTY upstream uses the name **pterm** on Windows; NiTTY standardises on **nterm**).
- **Extra window & session options**: Layered transparency, minimize-to-tray, clickable URLs, and RuTTY-style session scripts (KiTTY-compatible keywords in storage)—useful for automation without leaving the PuTTY family of tools.

NiTTY is **not** affiliated with the official PuTTY team or the KiTTY project; it is an independent fork that builds on their work.

---

## Attribution

| Project | Role |
|--------|------|
| **[PuTTY](https://www.chiark.greenend.org.uk/~sgtatham/putty/)** | Original design, protocols, security model, and the majority of the source tree. Copyright © 1997– Simon Tatham and contributors. |
| **[KiTTY](https://www.9bis.net/kitty/)** | A long-running PuTTY fork that popularized many Windows UX and session features. NiTTY gratefully adopts ideas and compatibility hooks from that lineage (e.g. session script concepts and registry keyword patterns where noted in code). |
| **[win32-darkmodelib](https://github.com/ozone10/win32-darkmodelib)** | A C++ library for dark mode and themed Win32 controls. NiTTY does not ship it as a dependency, but its techniques (e.g. undocumented UxTheme hooks, `WM_CTLCOLOR*` handling for read-only edits, scrollbar theming) informed the Windows configuration UI and related tooling. |

Upstream PuTTY remains the reference for behaviour, security updates, and documentation unless this repository states otherwise. When reporting security-sensitive issues, consider whether they belong in **upstream PuTTY** first.

---

## Building

NiTTY uses **CMake** (3.x). From the repository root:

```bash
cmake -S . -B build
```

On Windows with **Visual Studio**, use a generator you prefer, for example:

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Install (Unix):

```bash
cmake --build build --target install
```

See the plain [`README`](README) file in this directory for additional notes (e.g. Unix `pterm` privileges, documentation builds with Halibut).

---

## Upstream sync

NiTTY tracks [official PuTTY](https://git.tartarus.org/?p=simon/putty.git). Add the upstream remote once per clone:

```bash
git remote add putty https://git.tartarus.org/simon/putty.git
git fetch putty
```

Check what upstream has that NiTTY does not yet have:

```bash
git fetch putty
git log --oneline HEAD..putty/main
```

Merge upstream changes (resolve any conflicts in shared files; NiTTY-specific code is mostly under `nitty_*.c/h`, `windows/nitty_*.c`, and related config):

```bash
git fetch putty
git merge putty/main
```

After merging, verify `LATEST.VER` matches the upstream release you intend to ship, run a local or CI build, and test NiTTY-specific features (dark mode, portable config, session scripts, Pageant key persistence).

If a merge conflicts in shared upstream files, prefer keeping upstream structure and re-applying any NiTTY-specific hunks manually. General bugfixes made on upstream-owned files (for example in `proxy/`, `unix/local-proxy.c`, or `windows/utils/subprocess_waiter.c`) should be checked against upstream `main` after each sync — drop them once equivalent fixes land upstream.

Tagged upstream releases are available as `putty/0.84`, `putty/0.83`, and so on if you prefer merging a specific release rather than `putty/main`.

---

## Documentation

PuTTY’s manuals are built from the `.but` sources under `doc/` using [Halibut](https://www.chiark.greenend.org.uk/~sgtatham/halibut/). Prebuilt snapshots often ship docs; building from a bare clone may require generating them yourself.

---

## Licence

NiTTY inherits PuTTY’s licence. See the [`LICENCE`](LICENCE) file in this repository.

---

## Saved sessions and SSH passwords

NiTTY can **store the SSH login password** in a saved session (the same `Password` field used on **Connection → Data**), so it is written to the Windows registry or to portable session files alongside other settings.

That value is **not stored in plain text**: it is **obfuscated** (XOR plus Base64, keyed by the session name) before being saved. That makes it harder to accidentally copy a readable password out of a config export or a quick glance at a file.

**Security warning:** this is **not encryption you should trust for secrecy**. The obfuscation can be **reversed** by anyone who can read the source or the running binary, or who controls the machine. Treat it as a **convenience and casual deterrent**, not protection against a motivated attacker. For real secrets, use SSH keys, a password manager, or another mechanism that matches your threat model.

---

## Pageant (Windows)

- **Theming:** Pageant uses the same dark (or light) configuration style as NiTTY—subclassed controls, immersive dark title bar where supported, and consistent colours—so it does not look like a half-themed system dialog beside the rest of the suite.
- **Portable key paths:** If you use directory-based portable config (`savemode=dir` in `nitty.ini`; see the sample file in the repo), you can optionally ask Pageant to reload a list of private key **files** on startup. Enable this under the `[Pageant]` section (`savemode=dir` + `PersistKeys=1`). Paths are stored in `<configdir>\Pageant\pageant-keys.txt` (UTF-8, one path per line) and updated when keys are added or removed.
- **Passphrases are not saved:** That file—and this feature—stores **only paths** to key files (e.g. `.ppk`). **Passphrases are never written to disk** for persistence. Unlocked keys and remembered passphrases behave like stock Pageant: they live in memory for the running process (and are cleared when you use “forget passphrases” or exit), not in `pageant-keys.txt`.

`nitty.ini` is only for portable/bootstrap flags and the small set of keys read by the portable layer; session colours, SSH options, and most behaviour still come from saved sessions or the registry—see comments in `nitty.ini` and `windows/nitty_portable.c`.

---

## Links

- PuTTY home: <https://www.chiark.greenend.org.uk/~sgtatham/putty/>
- KiTTY: <https://www.9bis.net/kitty/>
- win32-darkmodelib (theming reference): <https://github.com/ozone10/win32-darkmodelib>
- CMake: <https://cmake.org/>
