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
| `bundled_runtime.py` | Fixed package-owned native/wheel manifest, ownership, mode and checksum validation |
| `runtime_prepare.py` | Offline, locked per-user Python cache preparation/reuse/repair; no hardware startup |
| `app_version.py` | Reads the installed/source app-builder version metadata |
| `local_node_refresh.py` | Bounded local GPS node snapshots on the one physical API connection |
| `phone_gateway.py`, `phone_services.py` | Selected-phone protocol gateway and private VPN configuration checks; no second radio owner |
| `user_service.py` | Fixed transient user-service lifecycle, private operation lock, stop/restart and recovery policy |
| `radio_guardian.py` | Own daemon/power lease, detect supervisor pipe EOF, stop/reap before restoring switches |
| `cap_lora.py` | MIT reusable Cap bring-up context and diagnostics; ordinary context exit restores prior values |
| `firewall.py`, `firewall-setup.sh` | Fixed-port nftables ensure/check/monitor/remove helper; capability-owned monitor, no app sudo grant |
| `qr_scan.py` | Optional camera decoder, run with a supervisor-enforced subprocess timeout |
| `meshtasticd-cardputerzero.yaml` | Hardware pins only; no channel keys or region |
| `radio_up.py` | Retired entry point; refuses the former unguarded daemon startup path |

## Installed startup

Mesh 0.1.19 bundles client 2.7.11, its locked dependencies and the qualified
GPS-corrected native executable. Do not create a manual radio venv or install a
global daemon for this version. The normal launcher calls `user_service.py ensure`;
the transient user service executes the package's `run-supervisor` helper. That
helper prepares/reuses the private Python cache offline and starts `supervisor.py`.

The fixed native executable is
`/usr/lib/cardputerzero-mesh/runtime/radio/meshtasticd`; PATH is not searched.
Original wheels, requirements/manifest and licenses are in that `runtime/` tree.
The managed environment is under
`${XDG_DATA_HOME:-$HOME/.local/share}/cardputerzero-mesh/managed-runtimes/<python_runtime_id>/`.
The old `venv/` is retained for rollback, not used by current launches.
Channels/history remain in `~/.local/state/cardputerzero-radio/`, independently
of the disposable managed environment.

## Operations and development

Run the fixed lifecycle helper as the logged-in user, without sudo:

```sh
python3 /usr/lib/cardputerzero-mesh/user_service.py status
python3 /usr/lib/cardputerzero-mesh/user_service.py ensure
# Explicitly stops reception and powers the radio down:
python3 /usr/lib/cardputerzero-mesh/user_service.py stop
```

`restart` stops/starts the service and applies the saved startup policy; check
Radio afterward. `ensure` never restarts an already active owner. Installation
marks replacement in progress and stops this unit; finish interrupted package
configuration rather than removing its marker manually.

Use the installed service, not a second `python supervisor.py` against live
hardware. A source/shared-backend mirror does not contain the root-owned runtime
and is not a drop-in live launcher. Develop with offline tests, then build/install
a complete package for hardware checks. Follow the [root README](../README.md),
[complete operations guide](../docs/install.md), [build guide](../docs/build.md),
[protocol](../docs/supervisor-protocol.md) and [security model](../docs/security.md).
Supervisor source is GPL-3.0-or-later; MIT components retain their SPDX notices.
LICENSE.GPL contains the full GPL. Full native/Python corresponding source and
notices accompany distribution, as described in [distribution](../docs/distribution.md).

Antenna before power; configure the correct region and a reviewed public/private profile
before running the daemon. Software cannot detect an antenna or guarantee safe
firmware boot traffic from an unsafe persisted profile. A bare Python `finally`
cannot restore switches after SIGKILL; use the guardian/service lifecycle.
