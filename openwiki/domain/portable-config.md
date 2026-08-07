---
type: Domain Concept
title: Portable directory config
description: nitty.ini savemode=dir layout for sessions, host keys, logs, and Pageant key paths.
tags: [domain, portable]
timestamp: 2026-08-07T00:00:00Z
---

# Portable directory config

KiTTY-style “save beside the exe” mode. Enabled when `nitty.ini` next to the executable contains `[NiTTY] savemode=dir`. Implementation: `windows/nitty_portable.c` / `.h`. Relates to [fork layout](../architecture/overview.md).

Without dir mode, sessions and host keys use the normal Windows registry under the PuTTY/NiTTY settings key.

## Layout under `<configdir>`

Default `<configdir>` is the directory containing `nitty.ini` / the exe. Optional `configdir=` may be relative to the exe dir or absolute.

| Path | Purpose |
| ---- | ------- |
| `Sessions\` | One file per saved session (`fileextension` optional, e.g. `.ktx`) |
| `SshHostKeys\` | Cached SSH host keys |
| `SshHostCAs\` | Host CA definitions |
| `Jumplist\` | Jump list / recent session data |
| `Logs\` | Session logs |
| `putty.rnd` | RNG seed file |
| `Pageant\pageant-keys.txt` | Optional persisted private-key **paths** (UTF-8) |

## Session logging

- Portable default `LogFileName`: absolute `...\Logs\nitty-&H-&P.log` (`windows/utils/defaults.c`). `&H` / `&P` expand via upstream `xlatlognam` (per host/port).
- On settings read, relative names (e.g. leftover `kitty.log`) are resolved under `Logs\` via `nitty_portable_resolve_log_filename` (`windows/storage.c`). Absolute user paths are unchanged. Paths containing `..` are not rewritten into `Logs\`.
- Non-portable default remains `putty.log`.

## Pageant

Requires `[NiTTY] savemode=dir` already. Then in `[Pageant]`:

```ini
savemode=dir
PersistKeys=1
```

Stores key file paths only — not passphrases. Pageant may point portable init at NiTTY’s directory when both exes ship together (`nitty_portable_set_config_directory`).

## Watch out for

- Only keys documented in the sample [`nitty.ini`](../../nitty.ini) are read. Most KiTTY.ini toggles are ignored.
- Colours, fonts, scripts, passwords, etc. live in session storage (files or registry), not in `nitty.ini`.
- Do not treat `Logs\` like a wipe target unless product requirements say so; host-key wipe paths in `storage.c` are separate.
