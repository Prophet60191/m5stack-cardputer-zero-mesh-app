#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""
Radio-foundation runner: power the Cap the right way, then run meshtasticd
against the SX1262 and report whether the radio actually came up.

This is the test that de-risks everything before we build a UI. It proves, on
real hardware, that: (1) our Cap bring-up works, (2) meshtasticd drives the
SX1262 with our config, and (3) the daemon is reachable so a client can later
join the `backhaul` channel.

Architecture (the pattern the whole app will use):
    with cap_lora.powered_cap():          # Cap powered + RF path enabled...
        spawn meshtasticd as a CHILD       # ...only while this runs...
        watch its log until the radio inits
    # child stopped, Cap powered down -- the kernel frees SPI/GPIO on child exit.

Running the radio as a short-lived child, not a system service, is our
least-privilege boundary: no root daemon, pins freed the instant we stop.

Linux-only; runs ON the device. Needs meshtasticd installed
(see ../../docs/builds/README.md build #2).
"""

from __future__ import annotations

import argparse
import contextlib
import ctypes
import os
import re
import shutil
import signal
import socket
import subprocess
import sys
import time
from pathlib import Path

import cap_lora


# meshtasticd logs "sx1262 init success" / "SX126x init result 0" on success, and
# names a real radio failure on the chip line. We match the radio lines
# specifically -- NOT generic words like "not found", which also appear in the
# harmless "RTC not found" startup warning and must not be read as a failure.
READY_RE = re.compile(r"sx126[12x].*(init success|init result:?\s*0|initiali[sz]ed)", re.I)
ERROR_RE = re.compile(r"(cannot open|resource busy|already in use|permission denied|"
                      r"no lora (radio|chip)|radio init failed|"
                      r"sx126[12x].*init.*(fail|result:?\s*-))", re.I)


def _free_tcp_port() -> int:
    """Ask the kernel for an unused localhost port by binding to port 0."""
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def _die_with_parent() -> None:
    """Run in the child just before exec: ask the kernel to send us SIGTERM if
    our parent (this runner) dies, so meshtasticd can never be orphaned holding
    the radio. (PR_SET_PDEATHSIG = 1.)"""
    with contextlib.suppress(Exception):
        ctypes.CDLL(None, use_errno=True).prctl(1, signal.SIGTERM, 0, 0, 0)


def _find_meshtasticd(explicit: str | None) -> str:
    if explicit:
        return explicit
    found = shutil.which("meshtasticd")
    if found:
        return found
    for candidate in ("/usr/sbin/meshtasticd", "/usr/bin/meshtasticd"):
        if os.path.exists(candidate):
            return candidate
    raise SystemExit("meshtasticd not found. Install it first:\n"
                     "  sudo apt-get install -y ~/meshtasticd-pkg/meshtasticd.deb")


def _stop(proc: subprocess.Popen) -> None:
    if proc.poll() is None:
        proc.terminate()
        try:
            proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()


def run(meshtasticd: str, config: str, timeout: float, keep: bool, port: int = 0) -> int:
    port = port or _free_tcp_port()

    # Give the daemon its own state dir in the user's home, so it needs no root
    # to save prefs/channels. HOME/XDG are pointed here too.
    state = Path.home() / ".local/state/cardputerzero-radio"
    state.mkdir(parents=True, exist_ok=True)
    log_path = state / "meshtasticd.log"

    env = dict(os.environ, HOME=str(state), XDG_DATA_HOME=str(state),
               XDG_CONFIG_HOME=str(state), XDG_STATE_HOME=str(state))
    command = [meshtasticd, "-c", config, "-p", str(port), f"--fsdir={state}"]

    print(f"Cap LoRa-1262 radio foundation test")
    print(f"  meshtasticd : {meshtasticd}")
    print(f"  config      : {config}")
    print(f"  TCP port    : {port}   (a client connects to 127.0.0.1:{port})")
    print(f"  log         : {log_path}")
    print()

    try:
        with cap_lora.powered_cap() as chip_id:
            print(f"[ok] Cap powered, RF switch {cap_lora.EXPANDER_ADDR:#04x} "
                  f"verified (ID {chip_id:#04x}).")
            with open(log_path, "wb") as log:
                proc = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT,
                                        env=env, preexec_fn=_die_with_parent,
                                        start_new_session=True)
            try:
                verdict = _watch_log(proc, log_path, timeout, port)
                if verdict == "ready" and keep:
                    print("\nRadio is up and the daemon is running. "
                          "Press Ctrl-C to stop (this powers the Cap back down).")
                    with contextlib.suppress(KeyboardInterrupt):
                        while proc.poll() is None:
                            time.sleep(0.5)
                return 0 if verdict == "ready" else 1
            finally:
                _stop(proc)
    except cap_lora.CapError as exc:
        print(f"[fail:{exc.kind}] Cap bring-up: {exc}")
        return 1
    finally:
        print("Cap powered down, switches restored.")


def _watch_log(proc: subprocess.Popen, log_path: Path, timeout: float, port: int) -> str:
    """Tail the daemon log until the radio reports ready, an error appears, the
    daemon exits, or we time out. Returns 'ready' / 'error' / 'exit' / 'timeout'."""
    deadline = time.monotonic() + timeout
    seen = 0
    while time.monotonic() < deadline:
        if proc.poll() is not None:
            _tail(log_path, 12)
            print(f"[fail] meshtasticd exited early (code {proc.returncode}).")
            return "exit"
        text = log_path.read_text(errors="replace")
        for line in text[seen:].splitlines():
            if READY_RE.search(line):
                print(f"[ok] radio initialized: {line.strip()}")
                print(f"\nRADIO ONLINE -- meshtasticd is serving on 127.0.0.1:{port}.")
                return "ready"
            if ERROR_RE.search(line):
                print(f"[fail] {line.strip()}")
                _tail(log_path, 12)
                return "error"
        seen = len(text)
        time.sleep(0.25)
    print(f"[timeout] no radio-ready line after {timeout:g}s. Last log lines:")
    _tail(log_path, 15)
    return "timeout"


def _tail(path: Path, n: int) -> None:
    lines = path.read_text(errors="replace").splitlines()
    for line in lines[-n:]:
        print(f"    | {line}")


def main(argv: list[str]) -> int:
    here = Path(__file__).resolve().parent
    parser = argparse.ArgumentParser(description="Power the Cap and bring meshtasticd up on the SX1262.")
    parser.add_argument("--meshtasticd", help="path to the meshtasticd binary (auto-detected if omitted)")
    parser.add_argument("--config", default=str(here / "meshtasticd-cardputerzero.yaml"),
                        help="meshtasticd YAML (defaults to the one beside this script)")
    parser.add_argument("--timeout", type=float, default=30.0,
                        help="seconds to wait for the radio to initialize (default 30)")
    parser.add_argument("--keep", action="store_true",
                        help="leave the radio running until Ctrl-C (so a client can connect)")
    parser.add_argument("--port", type=int, default=0,
                        help="fixed TCP port for the daemon API (default: a free port; 4403 is the Meshtastic default)")
    args = parser.parse_args(argv)

    meshtasticd = _find_meshtasticd(args.meshtasticd)
    return run(meshtasticd, args.config, args.timeout, args.keep, args.port)


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
