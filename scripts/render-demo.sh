#!/usr/bin/env bash
# Edit docs/demo/oled-panel-demo.ass to change captions without recording again.
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"

# Render both themes by default, or pass "light" or "dark" for a single capture.
render() {
    local base=docs/demo/oled-panel-demo
    case "$1" in
        dark) base+=-dark ;;
        light) ;;
        *) echo "Usage: $0 [light|dark]" >&2; return 2 ;;
    esac
    ffmpeg -y -hide_banner -loglevel warning \
        -i "$base-source.mp4" \
        -an -vf 'scale=1280:720:flags=lanczos,ass=docs/demo/oled-panel-demo.ass' \
        -c:v libx264 -preset slow -crf 20 -pix_fmt yuv420p -movflags +faststart \
        "$base-60fps.mp4"

    ffmpeg -y -hide_banner -loglevel warning \
        -i "$base-60fps.mp4" \
        -filter_complex '[0:v]fps=15,scale=960:540:flags=lanczos,split[a][b];[a]palettegen=stats_mode=diff[p];[b][p]paletteuse=dither=bayer:bayer_scale=3:diff_mode=rectangle' \
        -loop 0 "$base-60fps.gif"
}

if (( $# == 0 )); then
    render light
    render dark
elif (( $# == 1 )); then
    render "$1"
else
    echo "Usage: $0 [light|dark]" >&2
    exit 2
fi
