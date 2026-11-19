# SPDX-License-Identifier: GPL-2.0-or-later
"""Driven by record-demo.sh inside its private KWin session."""
import os
import subprocess
import sys
import time

import gi

gi.require_version("Gst", "1.0")
from gi.repository import Gst


def configure(delay, fade):
    for key, value in (("TimeoutMs", delay), ("FadeMs", fade)):
        subprocess.run(["kwriteconfig6", "--file", "kwinrc", "--group", "Effect-oledpanel",
                        "--key", key, str(value)], check=True)
    subprocess.run([os.environ["DEMO_QDBUS"], "org.kde.KWin", "/Effects",
                    "org.kde.kwin.Effects.reconfigureEffect", "oledpanel"], check=True)


def record(helper, output):
    node = helper.stdout.readline().strip()
    if not node.isdecimal():
        raise RuntimeError("KWin did not provide a PipeWire screencast node")
    pipeline = Gst.parse_launch(
        f"pipewiresrc path={int(node)} do-timestamp=true keepalive-time=16 ! "
        "video/x-raw,framerate=0/1,max-framerate=60/1 ! queue ! videoconvert ! videorate ! "
        "video/x-raw,format=I420,framerate=60/1 ! x264enc tune=zerolatency speed-preset=fast "
        "bitrate=10000 key-int-max=120 ! h264parse ! mp4mux faststart=true ! filesink name=output"
    )
    pipeline.get_by_name("output").set_property("location", output)
    bus = pipeline.get_bus()

    def elapsed():
        return (pipeline.get_clock().get_time() - pipeline.get_base_time()) / Gst.SECOND

    def until(at):
        while (remaining := at - elapsed()) > 0:
            error = bus.pop_filtered(Gst.MessageType.ERROR)
            if error:
                raise RuntimeError(error.parse_error())
            time.sleep(min(remaining, 0.05))

    def move(x, y):
        print(x, y, file=helper.stdin, flush=True)

    def glide(start, end, at, duration):
        until(at)
        while True:
            t = min(1.0, (elapsed() - at) / duration)
            s = t * t * (3 - 2 * t)
            move(*(a + (b - a) * s for a, b in zip(start, end)))
            if t == 1.0:
                return
            time.sleep(1 / 120)

    desktop = (640, 380)
    move(*desktop)
    configure(0, 200)
    try:
        pipeline.set_state(Gst.State.PLAYING)
        _, state, _ = pipeline.get_state(5 * Gst.SECOND)
        if state != Gst.State.PLAYING:
            error = bus.pop_filtered(Gst.MessageType.ERROR)
            raise RuntimeError(error.parse_error() if error else "Screencast did not start")
        # Section boundaries match docs/demo/oled-panel-demo.ass. Positions are
        # logical pixels of the 1280x720 desktop; the source captures at 2x.
        for at, approach, delay, fade, panel, away in (
            (0.00, 2.15, 0, 200, (380, 685), (950, 390)),
            (5.82, 7.62, 600, 500, (880, 685), (280, 300)),
            (12.19, 13.99, 1200, 900, (665, 685), (640, 380)),
        ):
            if at:
                until(at)
                configure(delay, fade)
            glide(desktop, panel, approach, 0.9)
            glide(panel, away, approach + 2.1, 0.8)
            desktop = away
        until(19.53)
        pipeline.send_event(Gst.Event.new_eos())
        msg = bus.timed_pop_filtered(5 * Gst.SECOND, Gst.MessageType.EOS | Gst.MessageType.ERROR)
        if msg is None or msg.type != Gst.MessageType.EOS:
            raise RuntimeError(msg.parse_error() if msg else "Timed out finalizing the movie")
    finally:
        pipeline.set_state(Gst.State.NULL)


if __name__ == "__main__":
    if os.environ.get("WAYLAND_DISPLAY") != "oled-demo":
        sys.exit("Use scripts/record-demo.sh to launch the isolated recording session")
    Gst.init(None)
    with subprocess.Popen([sys.argv[1]], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True) as helper:
        try:
            record(helper, sys.argv[2])
        finally:
            helper.stdin.close()
