#!/usr/bin/env python3
"""Scroll the mouse wheel over a game position of the WSLg DM2Mac window (XTest), for the automap zoom.

  DISPLAY=:0 python3 xwheel.py X Y CLICKS [up|down]   X, Y in 320x200 game pixels; up (button 4) zooms in

The control socket has no wheel request, so this needs the game started with DM2_VIDEO=x11 (a real window).
"""
import ctypes
import re
import subprocess
import sys
import time

gx, gy, clicks = int(sys.argv[1]), int(sys.argv[2]), int(sys.argv[3])
button = 5 if len(sys.argv) > 4 and sys.argv[4] == 'down' else 4
info = subprocess.run(['xwininfo', '-name', 'DM2Mac'], capture_output=True, text=True, check=True).stdout
get = lambda key: int(re.search(rf'{key}:\s+(-?\d+)', info).group(1))  # noqa: E731
left, top, width, height = get('Absolute upper-left X'), get('Absolute upper-left Y'), get('Width'), get('Height')
scale = min(width / 320, height / 200)  # the game keeps its aspect: letterbox offsets if the window is not 16:10
ox, oy = left + (width - 320 * scale) / 2, top + (height - 200 * scale) / 2
x, y = int(ox + (gx + 0.5) * scale), int(oy + (gy + 0.5) * scale)

xlib, xtst = ctypes.CDLL('libX11.so.6'), ctypes.CDLL('libXtst.so.6')
xlib.XOpenDisplay.restype = ctypes.c_void_p
display = ctypes.c_void_p(xlib.XOpenDisplay(None))
if not display.value:
    sys.exit('no X display')
xtst.XTestFakeMotionEvent(display, -1, x, y, 0)
xlib.XFlush(display)
time.sleep(0.2)
for _ in range(clicks):
    xtst.XTestFakeButtonEvent(display, button, 1, 0)
    xtst.XTestFakeButtonEvent(display, button, 0, 0)
    xlib.XFlush(display)
    time.sleep(0.15)
xlib.XCloseDisplay(display)
print(f'wheel {"up" if button == 4 else "down"} x{clicks} at game {gx},{gy} (screen {x},{y}, scale {scale:.2f})')
