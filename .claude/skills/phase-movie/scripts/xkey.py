#!/usr/bin/env python3
"""Hold or release a key on the WSLg X server (XTEST): xkey.py down|up Control_L"""
import ctypes, sys
x = ctypes.CDLL("libX11.so.6"); t = ctypes.CDLL("libXtst.so.6")
x.XOpenDisplay.restype = ctypes.c_void_p
x.XKeysymToKeycode.restype = ctypes.c_ubyte
x.XStringToKeysym.restype = ctypes.c_ulong
d = x.XOpenDisplay(None)
if not d: sys.exit("no X display")
kc = x.XKeysymToKeycode(ctypes.c_void_p(d), ctypes.c_ulong(x.XStringToKeysym(sys.argv[2].encode())))
t.XTestFakeKeyEvent(ctypes.c_void_p(d), ctypes.c_uint(kc), ctypes.c_int(sys.argv[1] == "down"), ctypes.c_ulong(0))
x.XFlush(ctypes.c_void_p(d)); x.XCloseDisplay(ctypes.c_void_p(d))
