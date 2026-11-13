#!/usr/bin/env bash
# Edit docs/demo/oled-panel-demo.ass to change captions without recording again.
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"

ffmpeg -y -hide_banner -loglevel warning \
    -i docs/demo/oled-panel-demo-source.mp4 \
    -an -vf 'scale=1280:720:flags=lanczos,ass=docs/demo/oled-panel-demo.ass' \
    -c:v libx264 -preset slow -crf 20 -pix_fmt yuv420p -movflags +faststart \
    docs/demo/oled-panel-demo-60fps.mp4

ffmpeg -y -hide_banner -loglevel warning \
    -i docs/demo/oled-panel-demo-60fps.mp4 \
    -filter_complex '[0:v]fps=15,scale=960:540:flags=lanczos,split[a][b];[a]palettegen=stats_mode=diff[p];[b][p]paletteuse=dither=bayer:bayer_scale=3:diff_mode=rectangle' \
    -loop 0 docs/demo/oled-panel-demo-60fps.gif
