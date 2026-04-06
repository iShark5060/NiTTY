# NiTTY

NiTTY is a **Windows-first SSH, Telnet, and serial terminal client** based on the [PuTTY](https://www.chiark.greenend.org.uk/~sgtatham/putty/) codebase. It keeps PuTTY’s reliability and protocol support while refreshing the experience with a modern dark-themed configuration UI and a set of quality-of-life features inspired by community forks—especially [KiTTY](https://www.9bis.net/kitty/).

If you use NiTTY in research, documentation, or redistribution, please cite **PuTTY** as the upstream project and acknowledge **KiTTY** where features trace to that ecosystem.

---

## Why NiTTY?

- **PuTTY’s core**: SSH, Telnet, Rlogin, SUPDUP, serial, Pageant, Plink, PuTTYgen workflow, and the same general configuration model.
- **Refreshed UI**: Windows 11–style dark configuration dialogs, consistent theming across tools (including NiTTYgen), and polished window chrome where supported.
- **Portable & session-friendly**: Optional portable layout (ini + session files) in the spirit of KiTTY-style workflows.
- **Extra window & session options**: Layered transparency, minimize-to-tray, clickable URLs, and RuTTY-style session scripts (KiTTY-compatible keywords in storage)—useful for automation without leaving the PuTTY family of tools.

NiTTY is **not** affiliated with the official PuTTY team or the KiTTY project; it is an independent fork that builds on their work.

---

## Attribution

| Project | Role |
|--------|------|
| **[PuTTY](https://www.chiark.greenend.org.uk/~sgtatham/putty/)** | Original design, protocols, security model, and the majority of the source tree. Copyright © 1997– Simon Tatham and contributors. |
| **[KiTTY](https://www.9bis.net/kitty/)** | A long-running PuTTY fork that popularized many Windows UX and session features. NiTTY gratefully adopts ideas and compatibility hooks from that lineage (e.g. session script concepts and registry keyword patterns where noted in code). |

Upstream PuTTY remains the reference for behaviour, security updates, and documentation unless this repository states otherwise. When reporting security-sensitive issues, consider whether they belong in **upstream PuTTY** first.

---

## Building

NiTTY uses **CMake** (3.x). From the repository root:

```bash
cmake -S . -B build
cmake --build build --config Release
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

## Links

- PuTTY home: <https://www.chiark.greenend.org.uk/~sgtatham/putty/>
- KiTTY: <https://www.9bis.net/kitty/>
- CMake: <https://cmake.org/>
