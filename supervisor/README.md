# Radio supervisor

This directory contains the Linux radio backend for the native Mesh UI:

| File | Purpose |
|---|---|
| `supervisor.py` | Same-user intent socket, validated explicit-channel actions and authoritative session state |
| `application_services.py` | Setup, channel/profile management, TX confirmation and sanitized diagnostics |
| `channel_setup.py` | Complete draft validation, expiring reviewed plans and verified/recoverable channel setup |
| `channel_import.py` | Bounded private QR/URL preview and compatible legacy import |
| `radio_settings.py` | Private recoverable preferences, opaque channel identity and local saved profiles |
| `state_store.py` | Bounded SQLite conversations, drafts and pins; no automatic resend queue |
| `user_service.py` | Fixed transient user-service lifecycle, private operation lock, stop/restart and recovery policy |
| `radio_guardian.py` | Own daemon/power lease, detect supervisor pipe EOF, stop/reap before restoring switches |
| `cap_lora.py` | MIT reusable Cap bring-up context and diagnostics; ordinary context exit restores prior values |
| `firewall.py`, `firewall-setup.sh` | Fixed-port nftables ensure/check/monitor/remove helper; capability-owned monitor, no app sudo grant |
| `qr_scan.py` | Optional camera decoder, run with a supervisor-enforced subprocess timeout |
| `meshtasticd-cardputerzero.yaml` | Hardware pins only; no channel keys or region |
| `radio_up.py` | Retired entry point; refuses the former unguarded daemon startup path |

Use the package's transient user service and firewall boot integration. The pinned client
library is meshtastic 2.7.11, installed separately in the user's venv. Follow the
[root README](../README.md), [protocol](../docs/supervisor-protocol.md), and
[security model](../docs/security.md). Supervisor source is GPL-3.0-or-later;
MIT components retain their own SPDX notices. LICENSE.GPL contains the full GPL.

Antenna before power; configure the correct region and a reviewed public/private profile
before running the daemon. Software cannot detect an antenna or guarantee safe
firmware boot traffic from an unsafe persisted profile. A bare Python `finally`
cannot restore switches after SIGKILL; use the guardian/service lifecycle.
