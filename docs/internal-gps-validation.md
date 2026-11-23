# Built-in Cap GPS qualification

Status: fitted receiver and app Map confirmed outdoors; both identified defects
have corrections installed. App 0.1.17 refreshes local fixes without broadcasting.
The separate native `+czgps1` package passes eight physical receiver starts and
private RF/phone-gateway checks. A final outdoor acquisition with both installed
together is requested; indoor no-fix readings do not qualify that last step.
USB GPS dongle work remains deferred until it completes.

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

## Retest and additional startup issue

The owner positioned the device outside again with Map open. The first corrected
watch showed node-only refresh completion but no coordinates. Private-log
summaries showed the native receiver probe had given up on this startup, leaving
no detected ATGM336H. This is a separate initialization failure, not proof that
the node refresh itself creates a fix. That interval was interrupted for an
exclusive diagnostic and is not counted as a successful five-minute run.

The bounded identity-only diagnostic received no valid sentences at 9600. At
115200 the receiver answered the same read-only PCAS06 identity query used by
native probing, with GP talker/model label ATGM336H matching the expected native
substring. It emitted no periodic GGA/RMC during that test. Native probe source
turns NMEA output off before identification; failure can leave it off. Identity
response is therefore possible even when there is no position stream. No
permanent receiver configuration or firmware was changed by the diagnostic.

Normal radio restart restored detection and the corrected app subsequently
reported valid/fresh coordinates with source age about 19 seconds. This verifies
the cache correction on device, but does not repair or erase the failed native
probe. A clean five-minute watch is running from this recovered state. Startup
robustness still needs investigation; dongle work remains deferred.

## Clean recovered-state outdoor run

After normal receiver detection recovered, collect 31 samples over 321.3 seconds.
All samples have valid coordinates for this node, Radio Online/send-receive,
healthy firewall, automatic sharing Off and MQTT Off. Twenty-one samples are
fresh; ten honestly exceed the 120-second freshness threshold between receiver
updates. Six fresh/stale transitions and new source timestamps demonstrate that
the app receives successive fixes rather than refreshing an old timestamp. Final
source age is 33 seconds. This passes the bounded recovered-state data-path and
freshness check. Physical Map-marker confirmation is requested and pending.

This does **not** qualify reliable receiver startup: one 0.1.17 startup failed
model detection and left periodic NMEA output suppressed; a later startup
succeeded. The identical read-only identity query succeeds at 115200 in an
exclusive diagnostic. Further work must explain this difference before claiming
startup reliability or moving to USB GPS. Do not silently flash the receiver,
change its persistent baud/constellation settings, open competing readers, or
replace shared radio dependencies as a speculative fix.

Reusable lesson: receiver enablement, raw fix acquisition, daemon NodeDB state,
client-cache freshness and actual screen presentation are separate observations.
Test with RF location sharing Off; never require broadcasting to make a local
map work. Preserve acquisition timestamps and unknown fields such as satellite
count rather than inventing telemetry for a cleaner-looking display.

Guide follow-up: correct the FAQ's suggestion that satellite count is always
available, document the unresolved startup problem and keep its question with
the first explanation across the page break. The final 0.1.17 manual is now
42 pages after that readable reflow; the separate reference remains one page.
Map/FAQ/final reference layouts were visually checked. Replace the existing
Drive PDF in place (same ID/folder), with metadata readback confirming 416523
bytes. Source/link/version checks pass. Earlier 41-page counts describe the
pre-FAQ-reflow intermediate PDF, not this final guide.

## Startup timing cause and prepared dependency correction

After the owner confirmed the Map marker, they brought the device inside and
reconnected USB. Eight exclusive, read-only identity queries at 115200 measured
98.02, 612.95, 977.04, 770.71, 771.38, 769.87, 770.69 and 771.39 ms. Seven exceed
the native ATGM-family identification window of 500 ms. This explains the phase-
dependent detection failure: a queued 1 Hz TXT response can arrive after the
probe moves on. The probe disables periodic NMEA before identification, so a
miss can also leave output suppressed. Receiver firmware is not being flashed.

Prepare a downstream correction on exact upstream commit
`54e0d8d0ab2ff56b3a9ce967e53f79e49af560fb`: only the ATGM-family window changes
to 1500 ms, plus its comment. Actual native-parser replay under ASan/UBSan passes
all eight measured timings versus one with the old deadline, while over-late,
unrelated and overlong responses remain bounded. The arm64 native-tft build and
version/link smoke pass. A full simulated-daemon/official-SDK smoke passes a
complete eight-slot synthetic private RX setup and node-only refresh. An earlier
incomplete channel fixture was corrected; no device setup was changed by it.

All 266 baseline payload checksums and 64 conffile checksums match the installed
original. Since the old repository version is unavailable, reconstruct a pristine
rollback package; do not call it the original distribution archive. Candidate
payload comparison changes only the executable and adds two provenance files.
Its fresh/upgrade container checks retain the administrator service mask and do
not run a daemon. Native dependency CI/build/source instructions are prepared;
no patched native service has yet been installed. Existing app 0.1.17 remains
installed. Repeated physical starts and RF/phone compatibility remain required.

## Exact native installation and bounded qualification — 2026-11-23

The [native CI run](https://github.com/Prophet60191/m5stack-cardputer-zero-mesh-app/actions/runs/45246363258)
passed at support source `759dcbe0322ab6dd67ee7cf40044315d9a164d54`.
The [main regression run](https://github.com/Prophet60191/m5stack-cardputer-zero-mesh-app/actions/runs/45246363243)
at that source also passes all 167 backend tests and native/arm64 package gates.
The native artifact includes full corresponding source, build metadata and hashes.
Its exact CI executable SHA-256 is
`9accde6d5111625d453d7491eaf09c68ee7f5ed774eed6d8173838a22d734913`.
The timeout patch SHA-256 is
`33620bd494e4de69eff9944a9ae3d98418dada08f008355ee78947c215e64036`.

Package `meshtasticd_2.7.26.61+czgps1_arm64.deb` is constructed from that CI
executable and the verified pristine baseline payload. SHA-256:
`59460d73248e8bd93be112a43e6923727902c681662566122ce187e3f6032101`.
It installs Debian version `2.7.26.61~obs54e0d8d~beta+czgps1`; the compiled banner
still reads upstream `2.7.26.54e0d8d`. Identify the correction by package version
and executable hash, not the banner. Original rollback package and consistent
private app/radio backups are retained outside Git. No release was published
and no upstream submission was made.

After ordinary Radio Off, install the exact package and return through the
ordinary unprivileged supervisor. The standalone system daemon stays masked and
inactive; no extra hardware owner, app privilege or configuration file is added.
All **332 regular installed files** match expected hashes, modes and ownership,
including the vendor maintainer script's existing config/web ownership.

Eight consecutive normal Radio Off/On cycles each detect ATGM336H, with no
probe give-up. Detection plus a three-second receiver-setup allowance completes
9.7–11.7 seconds after Radio becomes Online. Every sample stays send/receive,
GPS ENABLED, location/MQTT Off, firewall healthy. Saved radio/module/channel and
profile hashes remain unchanged. These are eight bounded real hardware starts,
not a claim of universal startup reliability or full-system cold-power coverage.

One normal private message gets a persisted delivered/Recipient ACK. A separate
unmodified SDK protocol fixture through the real phone gateway completes its
handshake and encrypted private send with a recipient ACK (`NONE` routing error).
The fixture first checks that no actual handset is connected and leaves it
undisturbed if one is. This is protocol/RF compatibility, not a repeated physical
iPhone user-flow test. At completion one physical daemon TCP connection remains;
Phone access is On/ready, Radio Online/send-receive and ADB stays active.
All 24 original messages, three threads and three preferences remain; only the
two deliberate test messages are added. App settings/profiles/identity/privacy
files match the original backup.

The last indoor start has no valid fix, as expected without an adequate sky view.
The existing nonfatal ATGM configuration ACK warning remains; it also occurred
in the earlier working outdoor run and is not concealed by the identification
fix. No enable-NMEA/update-frequency failure or probe give-up is logged. Final
combined outdoor acquisition/freshness is requested before dongle work. Keep
that separate from the earlier successful 321.3-second outdoor watch and the
owner's confirmed Map marker.

The FAQ now distinguishes the separate radio-service correction from the app
update. The refreshed manual stays 42 pages, 416535 bytes, SHA-256
`0bd9fd2fc42272233f8289e548a279dad6aed96bbae446451041e4dbcd8874c3`.
Changed FAQ/adjacent-page layout and source/link/version checks pass. The same
Drive file is replaced in place; readback verifies its ID, folder and size. The one-page quick
reference is unchanged.
