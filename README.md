# OLED HDR Panel Saver

Dims the Plasma panel brightness when idle independently of overall brightness settings to prevent OLED burn-in. Works best in HDR mode.

Hovering, touching, or clicking the panel restores full brightness instantly with a fast fade (unless configured to remain dimmed at all times).

## How it works

`kwin-effect/oledpanel` scales the composited panel surface in KWin's own
linear-light pipeline (`WindowPaintData::multiplyBrightness`, same mechanism
as DimInactive) — real per-surface luminance control that stays correct under
HDR, not an overlay and not translucency. Idle is panel-relative: only
presence over a panel counts as activity, so working elsewhere still lets it
dim. Settings live in System Settings → Desktop Effects (minimum brightness,
idle delay, dimming time, keep dimmed at all times) via the bundled `kwin_oledpanel_config` KCM.

## Install (Fedora)

```bash
sudo dnf install -y kwin-devel extra-cmake-modules libepoxy-devel libdrm-devel kf6-kcmutils-devel
cmake -S kwin-effect -B kwin-effect/build -G Ninja \
    -DCMAKE_INSTALL_PREFIX=/usr -DKDE_INSTALL_PLUGINDIR=$(rpm --eval "%{_qt6_plugindir}")
sudo cmake --install kwin-effect/build --prefix /usr
```

Or build the RPM (preferred — clean uninstall/upgrade):

```bash
sudo dnf install -y mock rpm-build
git archive --format=tar.gz --prefix=kwin-panel-hdr-oled-saver-effect-0.9.0/ HEAD \
    -o ~/rpmbuild/SOURCES/kwin-panel-hdr-oled-saver-effect-0.9.0.tar.gz
cp kwin-panel-hdr-oled-saver-effect.spec ~/rpmbuild/SPECS/
rpmbuild -bs ~/rpmbuild/SPECS/kwin-panel-hdr-oled-saver-effect.spec
mock -r fedora-44-x86_64 --rebuild ~/rpmbuild/SRPMS/kwin-panel-hdr-oled-saver-effect-*.src.rpm
```

Then remove any manually installed copies, install the RPM, and restart
KWin (`kwin_wayland --replace & disown`). Code changes always need the
compositor restart — hot unload/load reuses the old mapped library.

## Layout

- `kwin-effect/` — effect + settings KCM source, metadata, CMake.
- `kwin-panel-hdr-oled-saver-effect.spec` — RPM packaging.
- `docs/upstream-integration.md` — notes toward upstreaming.
