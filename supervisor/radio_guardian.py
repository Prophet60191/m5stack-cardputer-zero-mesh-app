#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Own Cap power and the daemon while the supervisor holds our stdin pipe open.

EOF detects supervisor death, including SIGKILL/OOM. No daemon inherits the
pipe writer. The user service's control group provides backup cleanup.
"""
from __future__ import annotations
import argparse
import ctypes
import sys
import fcntl
import json
import os
import selectors
import signal
import subprocess
import threading
from pathlib import Path
import cap_lora


def child_death_binding():
    """Linux kernel backstop if this guardian is itself killed.

    Only this single-threaded guardian may use preexec_fn, never the threaded
    supervisor. Resolve libc before forking and close the parent-death race.
    """
    if not sys.platform.startswith("linux"):
        return None  # non-hardware host process tests still exercise pipe EOF
    libc = ctypes.CDLL(None, use_errno=True)
    prctl = libc.prctl
    prctl.argtypes = [ctypes.c_int, ctypes.c_ulong, ctypes.c_ulong, ctypes.c_ulong, ctypes.c_ulong]
    prctl.restype = ctypes.c_int
    parent = os.getpid()
    def bind():
        if prctl(1, signal.SIGKILL, 0, 0, 0) != 0 or os.getppid() != parent:
            os._exit(127)
    return bind


def restore(saved: Path) -> None:
    if saved.stat().st_size > 1024: raise ValueError("invalid power recovery record")
    values = json.loads(saved.read_text())
    if not isinstance(values, dict):
        raise ValueError("invalid power recovery record")
    power = cap_lora.switch_value(values.get("power"))
    mux = cap_lora.switch_value(values.get("mux"))
    cap_lora._write_text(cap_lora.MUX_LED, mux)
    cap_lora._write_text(cap_lora.POWER_LED, power)
    saved.unlink()


def cleanup(runtime: Path) -> None:
    """Recover switches only when no guardian holds the hardware lease."""
    runtime.mkdir(mode=0o700, parents=True, exist_ok=True)
    with open(runtime / "radio.lock", "a") as lease:
        try:
            fcntl.flock(lease, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            return
        saved = runtime / "power.json"
        if saved.exists():
            restore(saved)


def run(command: list[str], runtime: Path, control_fd: int = 0) -> int:
    runtime.mkdir(mode=0o700, parents=True, exist_ok=True)
    with open(runtime / "radio.lock", "a") as lease:
        fcntl.flock(lease, fcntl.LOCK_EX | fcntl.LOCK_NB)
        saved = runtime / "power.json"
        if saved.exists():
            restore(saved)
        cap_lora.preflight()
        with open(saved, "w", opener=lambda path, flags: os.open(path, flags, 0o600)) as record:
            json.dump({"power": cap_lora.switch_value(cap_lora._read_text(cap_lora.POWER_LED)),
                       "mux": cap_lora.switch_value(cap_lora._read_text(cap_lora.MUX_LED))}, record)
            record.flush()
            os.fsync(record.fileno())
        stopped = threading.Event()
        previous = {sig: signal.signal(sig, lambda *_: stopped.set())
                    for sig in (signal.SIGTERM, signal.SIGINT)}
        proc = None
        try:
            with cap_lora.powered_cap():
                proc = subprocess.Popen(command, stdin=subprocess.DEVNULL, close_fds=True,
                                        preexec_fn=child_death_binding())
                try:
                    with selectors.DefaultSelector() as selector:
                        selector.register(control_fd, selectors.EVENT_READ)
                        while not stopped.is_set() and proc.poll() is None:
                            if selector.select(timeout=0.2):
                                os.read(control_fd, 1)
                                break
                finally:
                    # Stop the daemon BEFORE powered_cap restores hardware.
                    if proc.poll() is None:
                        proc.terminate()
                        try:
                            proc.wait(timeout=3)
                        except subprocess.TimeoutExpired:
                            proc.kill()
                            proc.wait()
                return proc.returncode or 0
        finally:
            if proc is not None and proc.poll() is None:
                proc.kill()
                proc.wait()
            # Recover on exceptions too; leave the record if restoration fails.
            if saved.exists():
                restore(saved)
            for sig, handler in previous.items():
                signal.signal(sig, handler)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runtime", type=Path, required=True)
    parser.add_argument("--cleanup", action="store_true")
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    if args.cleanup:
        cleanup(args.runtime)
        return 0
    command = args.command[1:] if args.command[:1] == ["--"] else args.command
    if not command:
        parser.error("daemon command is required")
    return run(command, args.runtime)


if __name__ == "__main__":
    raise SystemExit(main())
