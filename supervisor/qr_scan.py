#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Bounded by the supervisor's subprocess timeout; camera index is fixed."""
try:
    import cv2
    from pyzbar.pyzbar import decode
except ImportError:
    raise SystemExit(0)

camera = cv2.VideoCapture(0)
try:
    for _ in range(40):
        ok, frame = camera.read()
        if not ok:
            continue
        for symbol in decode(frame):
            value = symbol.data.decode('utf-8', 'strict')
            if len(value) <= 4096 and value.startswith('https://meshtastic.org/'):
                print(value)
                raise SystemExit(0)
finally:
    camera.release()
