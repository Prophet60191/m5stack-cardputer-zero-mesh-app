# Phone access qualification — 0.1.16

Status: installed; the owner confirms the official App Store iPhone app connects,
loads nodes/channels, and sends/receives successfully. Extended background,
reconnection and lifecycle checks are in progress. Protocol fixture results
remain separate from handset observations and roaming behavior.

## Source and exact installed artifact — 2026-11-23

- Functional source: `8cfa3b64eedea06bbb77190e22feb892aabbb617`.
- [Source-correlated CI](https://github.com/Prophet60191/m5stack-cardputer-zero-mesh-app/actions/runs/45196242905):
  Debian 13 arm64 build, fresh install, upgrade, firewall and non-publishing Store
  preflight passed. The parallel Linux sanitizer/regression job also passed, as
  did the subsequent documentation-only closeout run `45197355666`.
- Installed package: `cardputerzero-mesh_0.1.16-rev1_arm64.deb`, SHA-256
  `1419101f867601aded97b9658eac13f0e03d3b3003944ac941135307c40dc4b1`.
  All 114 installed regular files match its bytes, modes and UID/GID. The
  ordinary-user supervisor was restarted from the package, then the existing
  send/receive setup was resumed with the owner's antenna confirmation.
- All 158 backend tests pass, including 34 phone tests: selected-peer identity,
  malformed/partial/oversized/slow/flooding input, bounded queues, redaction,
  forbidden and unknown fields, snapshot/live ordering, retries and stale
  targets, dormant key omission and delayed VPN recovery. An unmodified official
  Python client is included in local protocol interoperability coverage.
- Native macOS arm64 Address/Undefined sanitizer checks pass (four CTest targets),
  including real settings navigation/events and full synthetic screenshot capture.
  The capture fixture restores its surviving screen after temporary theme tests.

Later closeout changes to this record do not change the installed runtime source.

## Design and reviewed official behavior

The firmware API admits one TCP client and can replace its previous connection.
The gateway therefore runs in the existing owner, with one physical daemon
connection, rather than exposing meshtasticd or adding a competing client.
A separately administered private Tailscale Serve forwarder supplies authenticated
transport and PROXY-v2 identity; Mesh admits only the explicitly selected peer.
No Funnel, tailnet ACL changes, app root/operator grants or public daemon exposure.

Apple client sources were reviewed at commit
`00984f381607568940efcf0b423101fa44d24285`: TCPConnection.swift and the
AccessoryManager Connect/ToRadio/manager sources. The gateway supports its
69420/69421 configuration/database phases, actual-daemon heartbeat responses,
quiet rejection of the automatic clock-setting hint and explicit Retry retaining
its original ID. Only matching failed sends may retry after five seconds;
pending/completed duplicates and changed payloads/targets are rejected. This is
not an exactly-once guarantee across arbitrary history deletion or crashes.
The official Python client's bounded UART wake preamble is also supported.

Radio/channel/profile administration, phone position upload, MQTT proxy and
arbitrary opaque/encrypted packets are outside this messaging/viewing gateway.
Host Wi-Fi/security/admin/MQTT secrets and dormant channel credentials are omitted;
active mesh keys/messages/positions are intentionally shared with the chosen phone.
Forwarding reconstructs allowed heartbeat fields and discards unknown exported
protobuf fields. An absent listener retries after delayed VPN startup while RF
remains owned by the supervisor; healthy listeners do not poll the VPN CLI.

## Installed-device and private RF evidence

- A consistent private SQLite/settings/profile backup was made outside Git before
  installation. Final comparison preserves all five original message rows, three
  thread rows, the original preference row, saved radio/profile/identity/privacy
  and app configuration files. Qualification adds test messages and phone
  preferences. Runtime logs and the RF airtime ledger legitimately change; they
  are not required to match their earlier bytes. No private data is restored over
  newer state or published in this record.
- Tailscale 1.104.7 was installed from its signed official Debian repository and
  signed in by the owner. Private Serve TCP 4403 forwards PROXY v2 to loopback
  47401. DNS was left unchanged. ADB remains enabled by owner instruction.
- After final installation: Radio Online/send-receive; existing private channel
  and tuning unchanged; one physical daemon TCP connection; firewall healthy.
  A non-selected VPN client received no Mesh bytes from 4403; raw radio 47400
  remained blocked over the VPN. Earlier LAN checks also blocked raw/gateway
  ports. No public-channel RF test or peer reconfiguration was performed.
- A native owner-path private message received a T-Deck recipient ACK. On the
  final installed source, an official Python TCP client through a trusted local
  PROXY-v2 fixture completed configuration and sent a private directed message
  with PKI requested; the real T-Deck recipient ACK reported NONE/no error.
  This proves the gateway/daemon/RF/ACK path, not positive iPhone/VPN admission.
- On the final installed source, a separate Apple-like local fixture completed
  both snapshot phases, checked host-secret redaction and received eight actual
  daemon heartbeat replies over 40 seconds. No invented keepalive receipt or
  extra physical radio connection was used. Fixtures leave a real phone session
  undisturbed if already connected.
- At closeout the radio remains Online/send-receive and Phone access is enabled
  for the selected iPhone, ready for its connection. Test fixture sessions closed.

## Documentation and remaining acceptance

The Markdown guide and 41-page PDF cover setup, permissions, limits and Retry;
the one-page reference stays one page. All seven chapters, 31 synthetic images,
links/anchors and version markers pass source validation. Reflowed pages and the
final Retry paragraph were visually checked. The existing Drive manual was
replaced in place, preserving its file ID/folder; metadata confirms the new size.
Real addresses, peer IDs, keys, auth links, private chats and backups stay out of Git.

## Owner handset acceptance follow-up — 2026-11-23

The owner confirms the official iPhone app connected and loaded nodes/channels,
then reports sending and receiving works perfectly. These are actual handset
observations, beyond the earlier local protocol fixtures. Separate display of
both directions on Lockon's own screen has not been explicitly confirmed.

The owner completed the approximately two-minute lock/background step and
reports that Meshtastic reconnected without reopening Tailscale. This confirms
foreground reconnection; delivery specifically after that step was not separately
reported. Tailscale's VPN can remain active while its app is not in the foreground.
Backend samples observed the session return while RF remained Online/send-receive,
with a healthy firewall and one physical radio connection.

The owner then disabled iPhone Wi-Fi for the requested interval, restored it,
and confirms Meshtastic reconnected and the private test message arrived. This
passes recovery across that Wi-Fi interruption. Whether cellular carried traffic
in between was not observed, so this is not evidence of Wi-Fi/cellular handover.

Extended tests are being conducted over existing Wi-Fi/Tailscale. No SIM is
required in the Cardputer. Cellular handover remains untested; an iPhone with
working mobile data can provide that path while the Cardputer stays on Wi-Fi. Losing the only Internet connection is an expected loss
of remote access, not proof of a radio failure.

A live ordinary-user `phone.set` Off/On check passed: Off closed the gateway
listener and session; On restored the listener. Both stages kept Radio
Online/send-receive and the firewall healthy. The owner confirms that a manual
reconnect was needed afterward, then messaging worked. Recovery is usable but
automatic handset reconnection after an intentional access toggle is not proven.

A normal `radio.stop`/`radio.start` lifecycle check passed. Phone access stopped
with the radio and resumed after Online; the send/receive setting and hashes of
saved channel/radio/module settings, named profiles and app configuration remain
unchanged. Actual handset reconnection after this step is being checked separately.

Still pending with the owner:

- Explicit confirmation of both message directions on Lockon's own screen.
- Actual handset reconnection after radio restart; longer foreground idle
  behavior and roaming between Wi-Fi and cellular.
- Cold-boot phone/VPN recovery and representative longer handset use. Automated
  recovery/slow-client tests and a stationary 40-second heartbeat check do not
  substitute for these physical results.

No release, Store submission or upstream contribution was created.
