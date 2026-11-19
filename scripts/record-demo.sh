#!/usr/bin/env bash
# Record real Plasma panel transitions on a private virtual output, not the user's screen.
set -euo pipefail
case "${1:-both}" in light|dark|both) theme=${1:-both} ;; *) echo "Usage: $0 [light|dark|both]" >&2; exit 2 ;; esac
[[ $# -le 1 ]] || { echo "Usage: $0 [light|dark|both]" >&2; exit 2; }
root=$(cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT

protocols=${PLASMA_WAYLAND_PROTOCOLS_DIR:-/usr/share/plasma-wayland-protocols}
for name in fake-input screencast; do
    xml=$protocols/$name.xml
    if [[ $name == screencast ]]; then xml=$protocols/zkde-screencast-unstable-v1.xml; fi
    wayland-scanner client-header "$xml" "$work/$name.h"
    wayland-scanner private-code "$xml" "$work/$name.c"
done
flags=$(pkg-config --cflags --libs wayland-client)
read -ra wayland_flags <<< "$flags"
cc -O2 -I"$work" "$root/scripts/demo/wayland-capture.c" "$work"/{fake-input,screencast}.c \
    "${wayland_flags[@]}" -o "$work/capture"

# In particular, do not let the session inherit the host's tray watcher, X display,
# PipeWire socket, or user configuration. Keep the real desktop unchanged on exit.
unset DISPLAY WAYLAND_DISPLAY WAYLAND_SOCKET SESSION_MANAGER DBUS_SESSION_BUS_ADDRESS \
    PIPEWIRE_REMOTE PIPEWIRE_RUNTIME_DIR PULSE_SERVER XDG_ACTIVATION_TOKEN
export HOME="$work/home" XDG_RUNTIME_DIR="$work/runtime" XDG_CONFIG_HOME="$work/config" \
    XDG_DATA_HOME="$work/data" XDG_CACHE_HOME="$work/cache" XDG_STATE_HOME="$work/state"
mkdir -m 700 "$HOME" "$XDG_RUNTIME_DIR" "$XDG_CONFIG_HOME" "$XDG_DATA_HOME" "$XDG_CACHE_HOME"
# A fresh profile would otherwise launch Welcome Center over the demo.
plasma_version=$(QT_QPA_PLATFORM=offscreen plasmashell --version)
kwriteconfig6 --file plasma-welcomerc --group General --key LastSeenVersion "${plasma_version##* }"
export XDG_SESSION_TYPE=wayland KDE_FULL_SESSION=true KDE_SESSION_VERSION=6 LC_ALL=C.UTF-8
export WAYLAND_DISPLAY=oled-demo QT_QPA_PLATFORM=wayland
kwriteconfig6 --file kwinrc --group Plugins --key oledpanelEnabled true
kwriteconfig6 --file kwinrc --group Effect-oledpanel --key MonitorMode All
kwriteconfig6 --file kwinrc --group Effect-oledpanel --key DimFactor 0.25

record_session() {
    local root=$1 work=$2 theme=$3
    local bus effects=/Effects effect_api=org.kde.kwin.Effects
    bus=$(command -v qdbus6 || command -v qdbus-qt6 || command -v qdbus)
    export DEMO_QDBUS="$bus"
    children=()
    trap 'kill "${children[@]}" 2>/dev/null || :; wait || :' EXIT
    pipewire >"$work/pipewire.log" 2>&1 & children+=("$!")
    wireplumber --profile=policy & children+=("$!")
    kwin_wayland --virtual --socket "$WAYLAND_DISPLAY" --width 2560 --height 1440 \
        --no-lockscreen --no-global-shortcuts --no-kactivities & children+=("$!")
    gdbus wait --session --timeout 20 org.kde.KWin
    kscreen-doctor output.Virtual-0.scale.2
    if [[ $("$bus" org.kde.KWin "$effects" "$effect_api.isEffectLoaded" oledpanel) != true ]]; then
        echo 'Install the oledpanel effect for the running KWin version first.' >&2; exit 1
    fi
    local activities
    activities=$(sed -n 's/^Exec=//p' /usr/share/dbus-1/services/org.kde.ActivityManager.service)
    "$activities" & children+=("$!")
    gdbus wait --session --timeout 15 org.kde.ActivityManager
    plasmashell --no-respawn & children+=("$!")
    gdbus wait --session --timeout 20 org.kde.plasmashell
    local -a shell=("$bus" org.kde.plasmashell /PlasmaShell org.kde.PlasmaShell.evaluateScript)
    for ((i=0; i<100; ++i)); do
        [[ $("${shell[@]}" 'print(panels().length)' 2>/dev/null) == 1 ]] && break
        sleep 0.1
    done
    [[ $("${shell[@]}" 'print(panels().length)') == 1 ]] || { echo 'Expected one default Plasma panel' >&2; exit 1; }
    local mode suffix wallpaper
    for mode in light dark; do
        [[ $theme == both || $theme == "$mode" ]] || continue
        suffix=; wallpaper=images
        if [[ $mode == dark ]]; then suffix=-dark; wallpaper=images_dark; fi
        wallpaper="/usr/share/wallpapers/Flow/contents/$wallpaper/5120x2880.jpg"
        [[ -r $wallpaper ]] || { echo "Missing Flow wallpaper: $wallpaper" >&2; exit 1; }
        plasma-apply-desktoptheme "breeze-$mode"
        "${shell[@]}" "const d=desktops()[0]; d.wallpaperPlugin='org.kde.image';
            d.currentConfigGroup=['Wallpaper','org.kde.image','General']; d.writeConfig('Image','file://$wallpaper');
            const p=panels()[0]; p.height=56; p.opacity='opaque';
            for (const w of p.widgets()) {
                if (w.type==='org.kde.plasma.digitalclock') { w.currentConfigGroup=['Appearance']; w.writeConfig('showDate',false); }
                if (w.type==='org.kde.plasma.icontasks') { w.currentConfigGroup=['General']; w.writeConfig('launchers',
                    ['applications:systemsettings.desktop','applications:org.kde.dolphin.desktop','applications:org.kde.konsole.desktop','applications:org.mozilla.firefox.desktop']); }
            }"
        sleep 1 # Let the wallpaper and theme transition finish before the first frame.
        timeout 45s python3 "$root/scripts/demo/hover-cycles.py" "$work/capture" "$work/$mode.mp4"
        cp "$work/$mode.mp4" "$root/docs/demo/oled-panel-demo$suffix-source.mp4"
        "$root/scripts/render-demo.sh" "$mode"
    done
}
export -f record_session
dbus-run-session -- bash -c 'set -euo pipefail; record_session "$@"' _ "$root" "$work" "$theme"
