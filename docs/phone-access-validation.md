# Phone access qualification — 0.1.16

Status: implementation and offline qualification in progress. Device/iPhone/RF
results will be recorded separately; no end-to-end acceptance is claimed yet.

## Scope and rationale

The official native Meshtastic client API admits one TCP client and can replace
its previous connection. The implementation therefore adds a protocol-aware,
restricted gateway to the existing owner; it does not expose meshtasticd or add
another physical client. A separately administered Tailscale Serve forwarder
adds authenticated transport and source identity. Mesh checks the selected peer.

Current Apple sources use separate configuration/database nonce values 69420 and
69421, a framed TCP stream and a live heartbeat watchdog. The gateway handles
both phases, ordinary legacy full snapshots and the official Python wake preamble.
The source revision and qualification results will be pinned at closeout.

## Required acceptance checks

- Offline: native sanitizer UI/navigation tests; all existing backend tests;
  protocol parsing, partial/oversized input, selected-peer identity/reassignment,
  redaction, forbidden writes, malformed/slow/flooding clients, bounded recovery,
  duplicate sends, real ACK IDs and a real unmodified Python TCP client.
- Debian arm64: build/install/upgrade package checks, privileges/firewall and
  matching source/artifact hashes; no hardware access during those tests.
- Device: consistent private backups; exact installed artifact; ordinary-user
  runtime; unchanged channel/tuning/TX/sharing/history; raw API still blocked.
- iPhone: official App Store app connects over the private VPN; nodes/channels
  load; incoming/outgoing messages appear; idle, background/foreground,
  disconnect/reconnect, radio restart and Access Off/On behave predictably.
- RF: recipient ACK/peer receive and a reply on an existing private channel,
  without reconfiguring the peer or sending public-channel test messages.

Any unperformed check stays pending. A stationary connection test does not
qualify roaming between Wi-Fi and cellular or general off-grid performance.

## Offline checkpoint — 2026-11-23

- All 153 backend tests pass, including 29 phone protocol/security/failure tests.
  An unmodified meshtastic 2.7.11 TCP client completes configuration and sends
  over a local PROXY-v2 fixture. This is not an actual-iPhone result.
- Native macOS arm64 Address/Undefined sanitizer checks pass (four CTest targets),
  including the phone settings flow, correlated enable/peer selection requests,
  connection details and Esc. A later synthetic capture includes the sharing scope.
- The Debian 13 arm64 package passed build/install/upgrade/firewall gates and
  its then-current 148-test suite. Final CI will build the committed source and
  rerun all tests; device installation must use that exact checked artifact.
- Apple client behavior was reviewed at commit
  `00984f381607568940efcf0b423101fa44d24285` (TCPConnection.swift and
  AccessoryManager's Connect/ToRadio/current manager sources). The 69420/69421
  phases and real-daemon heartbeat response are covered by protocol tests.
- On the owner-authorized device, the separately installed Tailscale 1.104.7
  signed in successfully; private Serve PROXY v2 was configured without Funnel
  or ACL changes. The existing daemon firewall still verifies. The app remains
  at its previous version with the radio Off while the candidate is qualified.
- A consistent SQLite backup plus settings/profile files were saved privately
  outside Git before installation. No private peer IDs, IPs, auth links or keys
  belong in this record. Actual iPhone, live RF and installed-candidate checks
  remain pending.

## Native Retry compatibility follow-up

Review of the same pinned Apple client's `resendMessage` showed that explicit
Retry preserves its original ID. Matching failed messages may now retry after
five seconds; pending/completed duplicates and changed payload, recipient or
channel identity cannot be silently retransmitted. A canonical private payload
hash accompanies phone history for this check. Two additional regression cases
bring the backend suite to 153. No receipt is invented for an unknown outcome.

## Strict protocol-boundary follow-up

Keepalive forwarding now constructs a fresh allowed Heartbeat message rather
than forwarding an untrusted protobuf with retained unknown fields. Exported
snapshots/live records discard unqualified future fields as well as redacting
known host secrets. Three hostile-field cases bring the backend suite to 156.
These checks prevent a schema mismatch from becoming an implicit command or
credential-export permission; they do not qualify arbitrary future firmware.

## Recovery and minimum-export follow-up

Disabled channel slots export empty settings instead of retaining credentials
from an older setup. An absent phone listener retries every 15 seconds while
Radio is Online, so delayed VPN startup does not require restarting RF. Healthy
listeners do not poll the VPN CLI. Both cases are covered; all 158 backend tests
pass before the final source-correlated CI/package run.
