# SPDX-License-Identifier: GPL-3.0-or-later
"""Fixed, unprivileged systemd user-service lifecycle. No radio/socket intents.

The service is transient: no unit is installed/generated in a shared user-unit
directory. The user manager still owns the cgroup, restart and stop recovery.
"""
import argparse
import fcntl
import os
from pathlib import Path
import stat
import subprocess
import sys
import time

UNIT = "cardputerzero-mesh-supervisor.service"
SYSTEMCTL = "/usr/bin/systemctl"
SYSTEMD_RUN = "/usr/bin/systemd-run"
PROPERTIES = (
    "Type=exec",
    "KillMode=control-group",
    "TimeoutStopSec=15",
    "Restart=on-failure",
    "RestartSec=3",
    "UMask=0077",
    "NoNewPrivileges=yes",
)


def command(*args, check=True):
    return subprocess.run(args, check=check, text=True, capture_output=True, timeout=30)


def start_command():
    # Transient ExecStopPost arguments do not expand static-unit %t specifiers.
    # The Debian user manager owns /run/user/<kernel UID>; no peer/env path is
    # interpolated into the command property.
    cleanup = ("ExecStopPost=/usr/bin/python3 /usr/lib/cardputerzero-mesh/radio_guardian.py "
               f"--runtime /run/user/{os.getuid()}/cz-mesh --cleanup")
    return [SYSTEMD_RUN, "--user", f"--unit={UNIT}", "--collect", "--quiet",
            "--description=Cardputer Zero Mesh radio supervisor",
            *(f"--property={p}" for p in (*PROPERTIES, cleanup)),
            "/usr/lib/cardputerzero-mesh/run-supervisor"]


def ensure():
    if Path("/run/cardputerzero-mesh-package-update").exists():
        raise RuntimeError("Mesh package installation is incomplete; finish package configuration before reopening")
    state = command(SYSTEMCTL, "--user", "show", "--property=ActiveState", "--value", UNIT,
                    check=False).stdout.strip()
    if state in ("active", "activating", "reloading"):
        return  # Opening a second UI must never restart an online radio.
    if state == "deactivating":
        raise RuntimeError("Mesh supervisor is stopping; wait and launch again")
    # Reload removes a cached legacy static unit after a package upgrade. No unit
    # is written, linked, enabled or started at login/boot by this helper.
    command(SYSTEMCTL, "--user", "reset-failed", UNIT, check=False)
    command(SYSTEMCTL, "--user", "daemon-reload")
    command(*start_command())


def runtime_lock():
    base = Path(os.environ.get("XDG_RUNTIME_DIR", f"/run/user/{os.getuid()}"))
    st = base.lstat()
    if not stat.S_ISDIR(st.st_mode) or st.st_uid != os.getuid() or st.st_mode & 0o077:
        raise PermissionError("A private, owned user runtime directory is required")
    fd = os.open(base / "cardputerzero-mesh-service.lock",
                 os.O_CREAT | os.O_RDWR | os.O_NOFOLLOW | os.O_CLOEXEC, 0o600)
    try:
        st = os.fstat(fd)
        if not stat.S_ISREG(st.st_mode) or st.st_uid != os.getuid() or st.st_nlink != 1 or st.st_mode & 0o077:
            raise PermissionError("Invalid Mesh service lock")
        deadline = time.monotonic() + 10
        while True:
            try:
                fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
                return fd
            except BlockingIOError:
                if time.monotonic() >= deadline:
                    raise TimeoutError("Another Mesh service operation is in progress")
                time.sleep(0.05)
    except BaseException:
        os.close(fd)
        raise


def main():
    parser = argparse.ArgumentParser(description="Manage the current user's Mesh supervisor; restart/stop also stop its radio")
    parser.add_argument("action", choices=("ensure", "restart", "stop", "status"))
    args = parser.parse_args()
    if os.geteuid() == 0:
        parser.error("Run as the logged-in device user, without sudo")
    try:
        if args.action == "status":
            return subprocess.run([SYSTEMCTL, "--user", "status", "--no-pager", UNIT], check=False).returncode
        fd = runtime_lock()
        try:
            if args.action in ("stop", "restart"):
                # Stop completes the cgroup kill/ExecStopPost before any new start.
                result = command(SYSTEMCTL, "--user", "stop", UNIT, check=False)
                if result.returncode and command(SYSTEMCTL, "--user", "show", "--property=LoadState", "--value", UNIT,
                                                check=False).stdout.strip() != "not-found":
                    raise RuntimeError("Mesh supervisor could not be stopped")
            if args.action in ("ensure", "restart"):
                ensure()
        finally:
            os.close(fd)
        return 0
    except (OSError, RuntimeError, subprocess.SubprocessError) as exc:
        print(f"Mesh service unavailable: {exc}. See the installation guide.", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
