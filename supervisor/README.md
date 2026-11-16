# Radio supervisor

This directory contains the Linux radio backend for the native Mesh UI:

| File | Purpose |
|---|---|
| `supervisor.py` | Same-user intent socket, validated explicit-channel actions and authoritative session state |
| `radio_guardian.py` | Own daemon/power lease, detect supervisor pipe EOF, stop/reap before restoring switches |
| `cap_lora.py` | MIT reusable Cap bring-up context and diagnostics; ordinary context exit restores prior values |
| `firewall.py`, `firewall-setup.sh` | Fixed-port nftables ensure/check/remove helper; only `check` is granted to the app user |
| `qr_scan.py` | Optional camera decoder, run with a supervisor-enforced subprocess timeout |
| `meshtasticd-cardputerzero.yaml` | Hardware pins only; no channel keys or region |
| `radio_up.py` | Retired entry point; refuses the former unguarded daemon startup path |

Use the package's user service and firewall boot integration. The pinned client
library is meshtastic 2.7.11, installed separately in the user's venv. Follow the
[root README](../README.md), [protocol](../docs/supervisor-protocol.md), and
[security model](../docs/security.md). Supervisor source is GPL-3.0-or-later;
MIT components retain their own SPDX notices. LICENSE.GPL contains the full GPL.

Antenna before power; configure the correct region and a reviewed public/private profile
before running the daemon. Software cannot detect an antenna or guarantee safe
firmware boot traffic from an unsafe persisted profile. A bare Python `finally`
cannot restore switches after SIGKILL; use the guardian/service lifecycle.
