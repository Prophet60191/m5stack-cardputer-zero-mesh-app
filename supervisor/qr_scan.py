#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Fixed camera capture and bounded QR decode. Never persists a camera frame."""
import importlib.util
import io
import os
import shutil
import subprocess
import sys
import time


def capture_program():
    # The built-in IMX219 is a libcamera/CSI device, not an ordinary V4L2
    # webcam. Opening /dev/video0 with OpenCV alone does not operate its ISP.
    return shutil.which('rpicam-still') or shutil.which('libcamera-still')


def available():
    if importlib.util.find_spec('pyzbar') is None: return False
    if capture_program(): return importlib.util.find_spec('PIL') is not None and os.path.exists('/dev/media0')
    return importlib.util.find_spec('cv2') is not None and os.access('/dev/video0', os.R_OK | os.W_OK)


def share_url(symbols):
    for symbol in symbols:
        if len(symbol.data) > 4096: continue
        try: value = symbol.data.decode('utf-8', 'strict')
        except UnicodeDecodeError: continue
        if value.startswith('https://meshtastic.org/'): return value
    return None


def scan():
    try:
        from pyzbar.pyzbar import decode
    except (ImportError, OSError):
        raise RuntimeError('Repair the app installation and Debian libzbar0t64 dependency') from None
    program = capture_program()
    if program:
        try: from PIL import Image
        except ImportError: raise RuntimeError('Install python3-pil for the CSI camera') from None
        deadline = time.monotonic() + 9
        for _ in range(2):
            remaining = deadline - time.monotonic()
            if remaining < 3: break
            try:
                result = subprocess.run([program, '--nopreview', '--timeout', '700',
                    '--width', '960', '--height', '720', '--encoding', 'png', '--output', '-'],
                    stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, timeout=min(8, remaining))
            except subprocess.TimeoutExpired:
                raise RuntimeError('CSI camera timed out; close other camera apps and retry') from None
            if result.returncode or not result.stdout or len(result.stdout) > 2 * 1024 * 1024:
                raise RuntimeError('CSI capture failed; check camera permissions and close other camera apps')
            with Image.open(io.BytesIO(result.stdout)) as frame:
                if frame.size != (960, 720): raise RuntimeError('Unexpected camera frame dimensions')
                value = share_url(decode(frame))
                if value: return value
        return None
    try: import cv2
    except ImportError: raise RuntimeError('No supported camera capture backend installed') from None
    camera = cv2.VideoCapture(0)
    try:
        if not camera.isOpened(): raise RuntimeError('Camera unavailable or busy')
        for _ in range(40):
            ok, frame = camera.read()
            if ok:
                value = share_url(decode(frame))
                if value: return value
    finally:
        camera.release()
    return None


if __name__ == '__main__':
    try:
        value = scan()
        if value: print(value)
    except (RuntimeError, OSError, ValueError) as exc:
        print(str(exc)[:240], file=sys.stderr)
        raise SystemExit(2)
