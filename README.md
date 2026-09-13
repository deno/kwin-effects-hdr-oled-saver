# OLED HDR Panel Saver

Dims the Plasma panel brightness when idle independently of overall brightness settings to prevent OLED burn-in. Works best in HDR mode.

Hovering, touching, or clicking the panel restores full brightness instantly with a fast fade (unless configured to remain dimmed at all times).

## Install

### Fedora

```bash
sudo dnf copr enable deno/kwin-effects-hdr-oled-saver
sudo dnf install -y kwin-effects-hdr-oled-saver
```

### openSUSE Tumbleweed / Slowroll

```bash
# Tumbleweed
sudo zypper addrepo https://download.opensuse.org/repositories/home:/deno_:/kwin-effects-hdr-oled-saver/openSUSE_Tumbleweed/home:deno_:kwin-effects-hdr-oled-saver.repo
# Slowroll
sudo zypper addrepo https://download.opensuse.org/repositories/home:/deno_:/kwin-effects-hdr-oled-saver/openSUSE_Slowroll/home:deno_:kwin-effects-hdr-oled-saver.repo

sudo zypper refresh
sudo zypper install kwin-effects-hdr-oled-saver
```
### Arch Linux

Import and trust the repository key:

```bash
# 1. Fetch and add the OBS repo key to pacman's keyring
curl -fsSL https://download.opensuse.org/repositories/home:/deno_:/kwin-effects-hdr-oled-saver/Arch_Extra/x86_64/home_deno__kwin-effects-hdr-oled-saver_Arch_Extra.key | sudo pacman-key --add -

# 2. Locally sign (trust) the key
sudo pacman-key --lsign-key $(curl -fsSL https://download.opensuse.org/repositories/home:/deno_:/kwin-effects-hdr-oled-saver/Arch_Extra/x86_64/home_deno__kwin-effects-hdr-oled-saver_Arch_Extra.key | gpg --show-keys --with-colons | awk -F: '$1=="pub"{print $5}')
```

Add the repository to `/etc/pacman.conf`:

```ini
[home_deno__kwin-effects-hdr-oled-saver_Arch_Extra]
Server = https://download.opensuse.org/repositories/home:/deno_:/kwin-effects-hdr-oled-saver/Arch_Extra/$arch
```

Then install:

```bash
sudo pacman -Sy kwin-effects-hdr-oled-saver
```

### Ubuntu

Support planned for Ubuntu 26.10 (requires KWin $\ge$ 6.7.0).

### openSUSE Leap

Support planned for Leap 16.1 (requires KWin $\ge$ 6.7.0).

## Running

Enable the effect in System Settings under Desktop Effects.

Restart KWin (`kwin_wayland --replace & disown`) or logout & login from the KDE desktop so the effect loads.

## Screenshots

![Effect entry in Desktop Effects](docs/screenshots/desktop-effects-dark.png#gh-dark-mode-only)
![Effect entry in Desktop Effects](docs/screenshots/desktop-effects-light.png#gh-light-mode-only)

![Effect settings](docs/screenshots/effect-settings-dark.png#gh-dark-mode-only)
![Effect settings](docs/screenshots/effect-settings-light.png#gh-light-mode-only)
