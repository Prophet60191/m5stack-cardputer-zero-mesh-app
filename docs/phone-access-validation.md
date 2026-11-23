# Phone access qualification — 0.1.16

Status: installed; initial connection, node/channel loading and two-way messaging
work, but **the owner reports a foreground idle disconnect with the screen awake**.
A subsequent owner-confirmed, traced five-minute repeat passed without a
disconnect. The earlier failure was not reproduced; its cause remains unresolved.
Intermittent foreground reliability is not fully qualified. Protocol fixture results
remain separate from handset observations and roaming behavior.

## Completed short acceptance checks

| Check | Result |
| --- | --- |
| Official iPhone app connection and nodes/channels | Owner confirms pass |
| Private message/reply and both displayed on Lockon | Owner confirms pass, including after idle |
| Return after phone lock/background | Owner confirms reconnection |
| iPhone Wi-Fi interruption | Reconnected; message arrived |
| Phone access Off/On | Radio unaffected; manual phone reconnect needed, then delivery passed |
| Radio stop/start | Settings preserved; manual phone reconnect needed, then delivery passed |
| Foreground idle | One reported failure; controlled five-minute repeat passed with trace; cause unresolved |
| iPhone cellular path, Lockon on Wi-Fi | Both directions arrived; connection stayed up |
| Physical cold power cycle | VPN/setup persisted; post-boot two-way messaging passed without new pairing/sign-in |

These are bounded observations, not a guarantee against intermittent disconnects
or evidence of arbitrary carrier/network availability. No production code was
changed during this extended qualification.

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
unchanged. The owner confirms that the iPhone required a manual reconnect
afterward and that its private test message then arrived. This is successful
manual recovery, not proof of automatic reconnection.

The first longer-idle attempt sampled a phone disconnect after approximately
153 seconds while RF stayed Online/send-receive, the firewall remained healthy
and the physical radio connection count stayed one. The owner confirms the
phone locked or switched apps during that interval. This is not a qualified
foreground idle failure or pass. A new five-minute test waits for a real phone
connection and asks the owner to keep Meshtastic visible on an awake screen;
the owner reports a disconnect while the screen remained awake. The automatic
monitor never saw a ready connection within its two-minute start window, so
that particular handset observation lacks a synchronized backend trace. It is
still a failed owner acceptance result; the earlier 40-second local heartbeat
fixture is not evidence against it. A follow-up records only loopback TCP header
timing/direction/length/close flags, without packet payloads, keys or chat text.
Earlier owner-confirmed reopening after lock worked.

## Controlled repeat with synchronized trace

The owner manually reconnected during a payload-free loopback TCP-header trace,
kept the phone screen awake and reports five minutes connected. The trace
independently recorded at least 317.2 seconds of connection activity, with zero
TCP FIN/reset events on that phone flow. All 31 reduced status samples during
the connected interval show the phone connected, Radio Online/send-receive,
a healthy firewall and one physical radio connection. Heartbeat-size requests
were forwarded to the real daemon and its replies returned through the gateway.
No synthetic client replaced the handset or generated a local liveness receipt.

This repeat passes that bounded test. The earlier foreground disconnect remains
an owner-reported intermittent failure without a synchronized failure trace;
no cause or repair is claimed, and no production code changed. The owner then confirms the post-idle private message/reply arrived in both
directions and both appeared on Lockon's screen. That post-idle display and
message exchange check passes.

## Cellular-path acceptance

With iPhone Wi-Fi disabled and mobile data selected as instructed, the owner
confirms both directions of the private iPhone/T-Deck exchange arrived and the
connection stayed up. Lockon remained on its existing Wi-Fi, without a SIM.
This passes the owner-observed Wi-Fi-to-cellular path change and two-way use;
it does not qualify arbitrary carrier coverage, roaming or extended outages.

## Cold-power platform recovery

The owner completed normal shutdown, USB removal, at least ten seconds switched
Off, then powered On and returned to the launcher. Fresh uptime was approximately
67 seconds at the first device check. Before opening Mesh: Tailscale and ADB
services were active, VPN status was Running without reauthentication, the exact
private Serve forwarder persisted, and the firewall service was active. Hashes
of saved channel/radio/module settings, profiles and app configuration match the
pre-power baseline. Stored Phone access On and the selected phone hash match too.

The transient Mesh supervisor had not started at the launcher, as designed.
Automatic radio start remains Off. The owner opened Mesh, started the radio and
confirms that both directions of the private iPhone/T-Deck exchange arrived
without new pairing, phone selection or VPN sign-in. The full cold-power check
therefore passes. Final device readback confirms Online/send-receive, Phone
access On and connected, a healthy firewall and one physical daemon connection;
configuration/profile/selected-phone hashes still match the pre-cycle baseline.
ADB and Tailscale remain active.

## Remaining limits and follow-up

The planned short owner-assisted checks are complete. Longer everyday use,
extended outages and intermittent-disconnect investigation remain open. The
reported awake-screen failure has no synchronized failure trace; one successful
repeat cannot explain or erase it. If it recurs, correlate the time, visible
phone connection/error state and Wi-Fi/mobile-data path with a payload-free TCP
header/status trace. Do not invent heartbeat replies, relax selected-peer/privacy
checks or restart unrelated networking to conceal a dropped handset session.

No release, Store submission or upstream contribution was created.
