# Recording the panel dimming demos

From the repository root, run:

```sh
./scripts/record-demo.sh
```

This records both Breeze themes. Pass `light` or `dark` to replace only that theme’s source MP4, captioned MP4, and GIF preview in this directory. The [recorder](../../scripts/record-demo.sh) starts a private Plasma session on a virtual KWin output, with its own D-Bus session and PipeWire server. It does not capture your desktop or its system tray.

## Requirements

- KDE Plasma 6 with the panel dimming effect installed for the running KWin version, the Breeze Light and Dark desktop themes, and the Flow wallpaper.
- A C compiler, `pkg-config`, Wayland client headers, `wayland-scanner`, and the `plasma-wayland-protocols` XML files. If the XML files are not in `/usr/share/plasma-wayland-protocols`, set `PLASMA_WAYLAND_PROTOCOLS_DIR` to their directory.
- PipeWire, WirePlumber with the `policy` profile, and Python 3 with PyGObject and GStreamer. The recording pipeline uses the PipeWire source, video conversion and rate adjustment, x264 encoder, H.264 parser, and MP4 muxer.
- FFmpeg with libass and libx264 for rendering captions and previews.

## Updating just the captions

Edit [oled-panel-demo.ass](oled-panel-demo.ass), then run `./scripts/render-demo.sh` from the repository root. The [renderer](../../scripts/render-demo.sh) reuses the existing source recordings; it does not start Plasma or record again. It also accepts `light` or `dark` to render just one theme.
