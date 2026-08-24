---
type: Operations
title: Oh My Posh and Nerd Fonts over SSH
description: Bring Ubuntu and DietPi boxes up to the Windows Terminal prompt used for NiTTY glyph testing.
tags: [operations, fonts, ssh]
timestamp: 2026-08-23T00:00:00Z
---

# Oh My Posh and Nerd Fonts over SSH

This is the remote half of matching the Windows prompt (oh-my-posh + `blackbird.json` + MesloLGM Nerd Font). The SSH host only has to emit UTF-8. NiTTY draws those bytes with the **Windows** font you pick in the session.

Tested on Ubuntu 26.04 (`test@10.50.50.200`). DietPi is Debian underneath, so the same commands work with the notes at the end.

## What you are matching

Windows Terminal reference:

- Colour scheme: Campbell
- Font: MesloLGM Nerd Font, 12 pt, weight Normal
- Line height 1.2, cell width 0.6
- Theme: `~/Documents/PowerShell/blackbird.json`

NiTTY has the same knobs under Window → Appearance, below font quality: **Line height** and **Cell width**. `1.00` / `1.00` leaves GDI's native cell (`tmAveCharWidth` × `tmHeight`). Any other pair is a unitless multiplier of font size in px (the em), matching Windows Terminal AtlasEngine: `"cellHeight": "1.2"` is `1.2 * em`, not `1.2 * tmHeight`. For this prompt use 1.20 and 0.60.

Windows Terminal does not stretch the Nerd Font for Powerline. AtlasEngine replaces box drawing (`U+2500`–`U+259F`) and Powerline separators (`U+E0B0`–`U+E0BF`) with builtin vector shapes that fill the cell. `U+E0C7` is not in that set, so it stays at the font size. NiTTY matches that split in `windows/nitty_termfont.c`: solid wedges `U+E0B0` / `U+E0B2` are GDI polygons that fill the cell; shades `U+2591`–`U+2593` are an 8×8 dither; outline `U+E0B1`, icons, and `U+E0C7` stay at the session font with one shared vertical offset.

## Client (NiTTY / Windows)

1. Install [MesloLGM Nerd Font](https://www.nerdfonts.com/font-downloads) (Meslo LG M). The "Windows Compatible" zip is the one that shows up in the font picker.
2. Session → Window → Appearance → Font: **MesloLGM Nerd Font**, 12 (or 16 px if the dialog is in pixels). Set Line height to 1.20 and Cell width to 0.60.
3. Session → Window → Translation → Remote character set: **UTF-8**.
4. Session → Connection → Data → Terminal-type string: `xterm-256color` (optional; `.bashrc` below also maps plain `xterm`).
5. Save the session.

If icons are empty boxes here but look fine in Windows Terminal, the host is doing its job and the bug is font fallback / GDI in NiTTY.

## Host: Ubuntu

Needs `curl`, `unzip`, a UTF-8 locale, and bash as the login shell.

```bash
sudo apt-get update
sudo apt-get install -y curl unzip ca-certificates locales
sudo locale-gen en_US.UTF-8
sudo update-locale LANG=en_US.UTF-8
```

Log out and back in if `locale` did not already show `LANG=en_US.UTF-8`.

Install oh-my-posh into `/usr/local/bin`:

```bash
curl -fsSL https://ohmyposh.dev/install.sh | sudo bash -s -- -d /usr/local/bin
oh-my-posh --version
```

Put `blackbird.json` in the user's home (copy from Windows `Documents\PowerShell\blackbird.json`, or from `_Resources/testVM/blackbird.json` in this repo). Then:

```bash
mkdir -p ~/.config/oh-my-posh
cp ~/blackbird.json ~/.config/oh-my-posh/blackbird.json
```

Append this once to `~/.bashrc` (Ubuntu already sources `.bashrc` from `.profile` on SSH login):

```bash
# --- NiTTY/oh-my-posh --------------------------------------------------------
if [ "$TERM" = "xterm" ]; then
  export TERM=xterm-256color
fi
if command -v oh-my-posh >/dev/null 2>&1; then
  eval "$(oh-my-posh init bash --config "$HOME/.config/oh-my-posh/blackbird.json")"
fi
# -----------------------------------------------------------------------------
```

Reconnect. You should see the blackbird segments (OS icon, `user@host`, folder, Powerline tail).

Sanity check without guessing at the prompt:

```bash
printf "Powerline: \ue0b0 \ue0b1 \ue0b2 \ue0b3\n"
printf "PUA: \uf013 \uf017\n"
printf "Plane-15: \U000f08c7 \U000f062c\n"
```

On this test VM that is also `~/glyph-test.sh`.

Do **not** install the Nerd Font on Linux for NiTTY SSH. The client rasterises glyphs. Install a Nerd Font on the box only if you also use a local Linux terminal.

## Host: DietPi

Same stack, smaller image. Typical gaps: no `unzip`, locale not generated, `curl` present but `ca-certificates` stale.

```bash
sudo apt-get update
sudo apt-get install -y curl unzip ca-certificates locales bash
```

Locale: either `dietpi-config` → Language/Regional Options, or:

```bash
sudo dpkg-reconfigure locales   # enable en_US.UTF-8 or en_GB.UTF-8
```

`C.UTF-8` is enough for the glyphs; oh-my-posh does not care which UTF-8 locale you pick as long as it is UTF-8.

DietPi often logs you into bash already. If `echo $SHELL` is `ash`/`sh`, set bash:

```bash
chsh -s /bin/bash
```

Then the Ubuntu oh-my-posh install and `.bashrc` snippet. On first boot, DietPi may not have `/usr/local/bin` on PATH for non-login shells; the install directory above is on the default Debian PATH.

ARM boards: the install script picks `linux-arm` / `linux-arm64` from `uname -m`. Do not copy an amd64 binary onto a Pi.

RAM: skip `oh-my-posh font install`. That unpacks a large zip you do not need for SSH.

## This test VM

Already done on `test@10.50.50.200`:

- oh-my-posh 30.6.5 in `/usr/local/bin`
- theme at `~/.config/oh-my-posh/blackbird.json`
- `.bashrc` hook above
- `~/glyph-test.sh`

SSH key is `_Resources/testVM/test.priv` (OpenSSH). `_Resources/testVM/test.ppk` is the PuTTY form for NiTTY. User `test`, password `test` if a key is not used. Passwordless sudo is on.
