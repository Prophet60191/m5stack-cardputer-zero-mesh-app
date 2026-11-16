#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Root-owned, fixed-purpose nftables helper. No caller-selected port/command."""
import json
import os
import shutil
import subprocess
import sys
import math
import pwd
import signal
import stat
import tempfile
import threading
import time
from pathlib import Path

PORT = 47400
TABLE = "cz_mesh"
STATUS = Path("/run/cardputerzero-mesh-firewall/status.json")


def health_status(path=STATUS, *, uid=None, now=None, boot=None):
    """Read a capability-service-owned verdict, never spawn sudo from a UI.

    No application-selected path is accepted over the intent socket. Optional
    parameters exist only for offline filesystem/clock regression tests.
    """
    try:
        expected = pwd.getpwnam("cz-mesh-firewall").pw_uid if uid is None else uid
        info = path.lstat()
        parent = path.parent.lstat()
        if (not stat.S_ISREG(info.st_mode) or info.st_uid != expected or info.st_mode & 0o022 or
                not stat.S_ISDIR(parent.st_mode) or parent.st_uid != expected or parent.st_mode & 0o022):
            raise RuntimeError("firewall health ownership is not trusted")
        if info.st_size > 2048: raise RuntimeError("invalid firewall health record")
        data = json.loads(path.read_text())
        if not isinstance(data, dict): raise RuntimeError("invalid firewall health record")
        checked = data.get("checked")
        if type(checked) not in (int, float) or not math.isfinite(checked):
            raise RuntimeError("invalid firewall health timestamp")
        age = (time.monotonic() if now is None else now) - checked
        actual_boot = Path("/proc/sys/kernel/random/boot_id").read_text().strip() if boot is None else boot
        if not 0 <= age <= 7 or data.get("boot") != actual_boot:
            raise RuntimeError("firewall monitor is stale; restart its system service")
        if data.get("ok") is not True:
            raise RuntimeError("radio API firewall is absent or changed; keep radio off")
        return {"ok": True, "age": round(age, 2)}
    except (OSError, KeyError, ValueError) as exc:
        raise RuntimeError("firewall health unavailable; install/start the firewall system service") from exc


def monitor():
    stopped = threading.Event()
    for sig in (signal.SIGTERM, signal.SIGINT): signal.signal(sig, lambda *_: stopped.set())
    boot = Path("/proc/sys/kernel/random/boot_id").read_text().strip()
    nft = shutil.which("nft")
    if not nft: raise RuntimeError("nftables is required")
    previous = None
    while True:
        ok = False
        if not stopped.is_set():
            try:
                result = subprocess.run([nft, "-j", "list", "table", "inet", TABLE],
                                        capture_output=True, text=True, timeout=3)
                ok = result.returncode == 0 and protected(json.loads(result.stdout))
            except (OSError, ValueError, subprocess.TimeoutExpired): pass
        payload = json.dumps({"ok": ok, "checked": time.monotonic(), "boot": boot}).encode()
        fd, temp = tempfile.mkstemp(prefix=".verdict-", dir=STATUS.parent)
        try:
            with os.fdopen(fd, "wb") as out: out.write(payload)
            os.chmod(temp, 0o644)
            os.replace(temp, STATUS)
        finally:
            if os.path.exists(temp): os.unlink(temp)
        if ok != previous:
            print("radio API firewall " + ("verified" if ok else "unavailable"), flush=True)
            previous = ok
        if stopped.wait(2):
            if not ok: break
            continue  # publish an immediate invalid verdict on orderly stop


def protected(document: dict) -> bool:
    entries = document.get("nftables", [])
    chain_ok = any(e.get("chain", {}).get("family") == "inet"
                   and e["chain"].get("table") == TABLE
                   and e["chain"].get("name") == "input"
                   and e["chain"].get("type") == "filter"
                   and e["chain"].get("hook") == "input"
                   and e["chain"].get("prio") == -1 for e in entries)
    rules = [e["rule"] for e in entries if "rule" in e]
    expected = [
        {"match": {"op": "!=", "left": {"meta": {"key": "iifname"}}, "right": "lo"}},
        {"match": {"op": "==", "left": {"payload": {"protocol": "tcp", "field": "dport"}},
                   "right": PORT}},
        {"drop": None},
    ]
    # No earlier accept/jump rules may bypass our drop.
    return chain_ok and len(rules) == 1 and rules[0].get("family") == "inet" \
        and rules[0].get("table") == TABLE and rules[0].get("chain") == "input" \
        and rules[0].get("expr") == expected


def main() -> int:
    os.environ["PATH"] = "/usr/sbin:/usr/bin:/sbin:/bin"
    if len(sys.argv) != 2 or sys.argv[1] not in ("ensure", "check", "remove", "monitor", "health"):
        raise SystemExit("usage: firewall.py {health|ensure|check|remove|monitor}")
    if sys.argv[1] == "health":
        try: health_status()
        except (RuntimeError, OSError) as exc: raise SystemExit(str(exc)) from None
        print("radio API firewall health verified")
        return 0
    if sys.argv[1] == "monitor":
        monitor()
        return 0
    nft = shutil.which("nft")
    if not nft:
        raise SystemExit("nftables is required; radio must remain off")
    result = subprocess.run([nft, "-j", "list", "table", "inet", TABLE],
                            capture_output=True, text=True, timeout=5)
    if sys.argv[1] == "remove":
        if result.returncode == 0:
            subprocess.run([nft, "delete", "table", "inet", TABLE], check=True, timeout=5)
        return 0
    if result.returncode == 0 and protected(json.loads(result.stdout)):
        return 0
    if sys.argv[1] == "check":
        raise SystemExit("radio API firewall is absent or changed; keep radio off")
    # Replace only our table, atomically; never flush the machine's ruleset.
    prefix = f"delete table inet {TABLE}\n" if result.returncode == 0 else ""
    rules = prefix + f'''table inet {TABLE} {{
 chain input {{
  type filter hook input priority -1; policy accept;
  iifname != "lo" tcp dport {PORT} drop
 }}
}}
'''
    subprocess.run([nft, "-f", "-"], input=rules, text=True, check=True, timeout=5)
    checked = subprocess.run([nft, "-j", "list", "table", "inet", TABLE],
                             capture_output=True, text=True, check=True, timeout=5)
    if not protected(json.loads(checked.stdout)):
        raise SystemExit("firewall verification failed; keep radio off")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
