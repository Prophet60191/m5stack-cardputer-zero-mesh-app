# Built-in Cap GPS qualification

Status: real outdoor receiver operation confirmed; an app cache defect was
found. Candidate 0.1.17 implements a correction; installation and app/map retest
are pending. The owner wants
the GPS already fitted to the LoRa Cap tested first, with USB GPS dongle work
explicitly deferred until this path works. The dongle is outside this change.

## Starting state — 2026-11-23

Installed runtime source remains `8cfa3b6` (0.1.16). Radio Online/send-receive;
GPS mode NOT_PRESENT; no valid/fresh coordinates and no satellite count reported.
Automatic location sharing and MQTT were confirmed Off.

Using the normal `config.set` intent, change only `position.gps_mode` to ENABLED.
The ordinary stop/start/readback path confirmed ENABLED, Radio Online/send-receive
and automatic sharing still Off. Do not interpret the earlier NOT_PRESENT enum
as proof the physical receiver is absent, or a timestamp alone as a position fix.

The configured source is `/dev/serial0` (currently resolves to `/dev/ttyS0`),
not a USB receiver. M5Stack's [official Zero usage guide](https://docs.m5stack.com/en/compute/cardputerzero/usage)
specifies the Cap LoRa-1262 for the GPS app. The existing meshtasticd configuration
points to that Cap serial path. Never open another GPS app/serial reader while
the radio daemon owns the port.

## Outdoor procedure and privacy

Place the device stationary outdoors with a clear sky view, its LoRa antenna
attached and Mesh running, within existing Wi-Fi range. Start a five-minute
watch only after the owner confirms placement. Use the existing authenticated
SSH user key over the private VPN; verify its server host key against the public
host key read directly through USB. No SSH/VPN privilege or access policy change.

Collect reduced validity/freshness/age/satellite and radio/privacy status only;
keep actual coordinates, identities and locations out of committed evidence.
Confirm that a fresh real fix reaches this node/map in the app, and later check
stale/no-fix presentation and any necessary repair/retest. Do not send coordinates
on LoRa, enable sharing, publish a location or treat a five-minute timeout as
proof of a defective receiver. A longer first acquisition may need investigation.

Ignored helper/evidence directory: `build/internal-gps-qualification/`.
## Actual outdoor result

Owner confirmed placement outside and the five-minute SSH watch ran for 321.8
seconds (31 samples). All app samples reported no valid/fresh position and no
satellite count, while Radio stayed Online, firewall healthy and location/MQTT
sharing Off. This did not qualify the application GPS path.

Read-only daemon inspection found its serial descriptor open on `/dev/ttyS0`,
fixed-position mode Off, a detected ATGM336H, and local coordinate updates in
its private log despite the app's empty position cache. The log also reported
one nonfatal receiver-configuration warning; it is not proof of hardware failure.
Persisted NodeDB files were old, so they were not used to infer live fix status.

For isolation, stop Radio normally and use the existing guardian/hardware lease
for one bounded serial reader; never run two readers together. A 60-second read
at 115200 produced 129 checksum-valid sentences, zero checksum failures, 59 GGA
fix samples, 59 active RMC samples and up to 27 satellites used. Only counts and
fix booleans were retained; no raw sentences or coordinates were saved in the
test evidence. Radio was restored Online/send-receive, GPS ENABLED, sharing Off.
The fitted receiver therefore works outdoors; a dongle is not needed to repair
this application data path.

## Cause and correction

The pinned Python library's `getMyNodeInfo()` reads a cache. The qualified native
daemon can update its own NodeDB from GPS without sending those updates over the
client stream when position broadcasts are suppressed. Reading that stale cache
more often cannot fix the map. Enabling RF broadcasts would violate user intent.

Inspect the actual native firmware source at commit `54e0d8d`: PhoneAPI.cpp/.h
supports a node-only snapshot nonce `69421`, including this node's latest
position. TypeConversions.cpp exports PositionLite without satellite count, so
an unknown satellite count from this path stays unknown; the direct-reader
count is not injected into app state.

Candidate 0.1.17 adds a bounded read-only snapshot every 30 seconds while GPS is
enabled and the existing owner is Online/config-confirmed. It uses the same
physical API connection, never a second client, RF packet or serial reader.
One request may be pending; ten-second timeout/backoff and stop/replacement
identity checks prevent floods or late completion changing a new owner. No SDK
cache clearing, channel appending, synthetic position timestamp, clock update or
privacy/TX setting change. Diagnostics expose only a fixed `gps_refresh` state.

Nine focused tests cover real SDK cache decoding and a later no-fix record,
channel/identity/cache retention, receive-only/privacy behavior, gating, timing,
timeout/send errors, synchronous callback and teardown. All 167 backend tests pass in the matching Debian environment. Package validation
and an exact installed outdoor/map/freshness retest remain required.

Official firmware references:
[PhoneAPI](https://github.com/meshtastic/firmware/blob/54e0d8d/src/mesh/PhoneAPI.cpp),
[NodeDB](https://github.com/meshtastic/firmware/blob/54e0d8d/src/mesh/NodeDB.cpp),
[GPS](https://github.com/meshtastic/firmware/blob/54e0d8d/src/gps/GPS.cpp).


## Source-correlated candidate package

Functional source `b430b20e0d4aed3f84253c4d7ddeb93002285c63` passed all
167 backend tests and both [CI regression jobs](https://github.com/Prophet60191/m5stack-cardputer-zero-mesh-app/actions/runs/45234313200),
including native sanitizers and Debian arm64 build/install/upgrade/firewall/Store
preflight. The [guide workflow](https://github.com/Prophet60191/m5stack-cardputer-zero-mesh-app/actions/runs/45234313456)
also passed. Exact candidate `cardputerzero-mesh_0.1.17-rev1_arm64.deb` SHA-256:
`dd64f03ec087f0e5b4cae48b3689da589c8d0e46f95129b6e069499c90a29d3a`.

Package downloaded and verified; installation awaits owner USB/Slave connection.
The owner may bring Lockon inside during preparation. Current running runtime
remains 0.1.16, with GPS enabled and sharing Off. New guide text is tightened to
keep the offline manual at 41 pages; the one-page reference stays one page.
No installed correction or map acceptance is claimed yet.

## Installed correction — 2026-11-23

The owner reconnected USB/Slave and authorized installation. A consistent private
backup was saved outside Git before replacing the package. The exact candidate
above is installed; all 116 regular files match archive hashes, modes and UID/GID.
The ordinary-user supervisor was restarted from the installed package and reports
0.1.17. All 24 original message rows, three thread rows and three preference rows
are preserved, with zero added/removed rows during the install check. Saved app
configuration, radio/module/channel files, profiles and identity/privacy files
match the pre-install baseline; runtime log/airtime bookkeeping is excluded.

Resume through normal radio.start, preserving send/receive, GPS ENABLED, location
sharing/MQTT Off. `gps_refresh` reports current, with one physical daemon TCP
connection and a healthy firewall; ADB remains active. Indoor diagnostics do not
yet show a valid position and are not counted as an outdoor failure. The owner
is asked to open Map and position the device outside again before starting the
corrected five-minute interval. The installed correction's outdoor/map/freshness
acceptance remains pending; no dongle work has begun.
