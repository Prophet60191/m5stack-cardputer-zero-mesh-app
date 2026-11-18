# Independent review remediation — 2026-11-15

Initial review candidate: **0.1.1**; current candidate: **0.1.13** (latest evidence below).
The [maintenance record](maintenance-pass.md) covers the subsequent measured cleanup.
The [13-item report follow-up](report-followup.md) records the subsequent
verification, corrections and qualified claims.
These changes replace the earlier assumption that
all review concerns had already been resolved. The evidence below covers the
current source and offline package; it does not certify an installed radio.

| Finding | Implemented correction | Regression evidence |
|---|---|---|
| Firewall lost on reboot; daemon exposed before setup | Boot-enabled capability-limited nftables service; fixed root-owned helper; scoped read-only sudo check; fail closed before power; online recheck | Real nftables ensure/check in isolated Linux network namespace; startup refusal test; package service enable/policy validation |
| Location toggle enables public primary | Explicit channel picker; separate public consent; only selected channel enabled; Off zeroes all; disable unapproved public location before power | Mixed public/private protobuf channel test |
| Share action exposes another group's key | Index + request ID carried to reply; serialize exactly selected channel; cancel late/wrong replies | Decode generated channel-set protobuf; native LVGL menu/cancel regression |
| Supervisor SIGKILL orphans daemon and Cap state | Pipe guardian owns child and power; stop/reap before restore; Linux child-death signal, hardware lease and user service cgroup/ExecStopPost recovery | Kill actual controller subprocess; assert daemon gone and fake sysfs values restored |
| Unconfirmed privacy toggles show false Off | Pending/Unknown state; actual admin readback; compare effective values; deadline and disconnect/error handling | Radio-cache/readback mismatch and timeout tests; native UI offline/pending/correlation checks |
| Public/unconfigured channel and DM validation bypass | Validate enabled channel/key and index for all RF actions; public/private supported; strict unicast IDs; no fallback; imported location/MQTT forwarding off with readback | Public/private/default-key classification, exact routing, malformed channel/destination and import tests |
| ACK state not retained; no-ACK pending; retries duplicate | Record before send, store delivery before broadcast, unique upsert, per-attempt token, timeout/stop failure | Snapshot/ACK enqueue ordering, immediate/late/malformed ACK, no-ACK, failure, retry and reconnect payload assertions |
| Retention not applied/synced; UI history unbounded | Trim all threads immediately; global 2,000-message bound; snapshots after removal; sync saved preference at connection | Existing-history/per-thread/global tests and native reconnect preference check |
| Partial intent write corrupts next request | Bounded background writer, nonblocking socket and absolute deadline; close poisoned stream; fail without replay | Real small-buffer socketpair stalls mid-frame; next action refused; UI-thread callback assertion |
| Unbounded frames/queues/work | Frame, JSON depth/value/string, queue byte/allocation/count, connection, operation, trace and UI-poll limits; camera subprocess deadline | Oversized complete line, deep/bad JSON recovery, work/queue/history and parser regressions |
| Package identity/version/template icon mismatch | 0.1.1 across build/About/manifest, real launcher icon/name; all runtime files installed; derive native dependencies from ELF | Manifest/version test; Linux arm64 build, package install, contents/control/unit/sudoers inspection |
| Message details hardcode a channel name | Resolve the message's actual channel index/name | Native thread/channel/share traversal |
| Real chat screenshot; missing license texts | Synthetic native LVGL screenshots; old image removed from local publication history after private backup; full static-library/font notices shipped | Image inspection; history/object scan; package contents inspection |

Additional settings hardening: unique private temp files, file/directory sync,
concurrent-writer and INI line-break/numeric regressions; geographic range and
antipodal map-math guards; app-specific settings,
launcher/binary/docs identity. Legacy preferences retain messaging/theme settings,
with radio auto-start requiring explicit re-enabling after migration.

## Verified offline

- 36 Python regression tests (one Linux-only) using pinned **meshtastic 2.7.11** protobufs, mocked
  radio I/O, and actual local sockets/processes with fake sysfs files.
- Full macOS desktop build; native LVGL/parser/transport CTest regression.
- AddressSanitizer + UndefinedBehaviorSanitizer build and CTest (leak detection
  disabled for the LVGL process-global caches; no claim of a leak audit).
- Linux arm64 device-target build/package on Debian 13 (Trixie), matching the
  documented device OS; install on a fresh matching container, actual library dependency resolution, `visudo -cf`,
  `systemd-analyze verify`, boot-unit enablement, and a non-root NET_ADMIN firewall
  helper run in its own network namespace. The earlier Ubuntu 24.04 build also
  passed. Containers do not reproduce the stock device image or hardware.
- Public working files/history and package inspected for secrets and personal
  paths. Public copyright/author identity and known default/synthetic test keys
  are intentional. Original history backup is kept outside the publication tree.

## Release qualification still required on the actual device

Attach the antenna first. Use a reviewed profile and the correct region.
The daemon can act on persisted firmware settings before client connection; a
post-connect check cannot prevent all boot traffic. Verify those settings before
power-on. Confirm safe profile startup, channel import, settings readback, actual
Off behavior, radio shutdown, and private peer delivery on the installed build.
Then cold-boot and verify the firewall precedes the radio, with IPv4/IPv6 LAN API
access refused. Check camera scanning and physical keyboard/navigation on-device.
No RF, live device, ADB, network/firewall or remote publication changes were made
by the offline work in this remediation session.

The history scrub changes commit/tag IDs. Publication updates the main branch and
v0.1.0 tag only after verifying their exact previous remote versions. Other clones
and cached/downloaded copies cannot be recalled by rewriting Git references. The
private recovery bundle is outside this repository and must not be published.


## On-device follow-up — 2026-11-15

The user confirmed physical readiness. Candidate 0.1.1 was installed on the
Debian 13 arm64 development device, with the original app/profile backed up
privately. Initial launch exposed two deployment gaps that the container's unit
syntax checks could not reveal:

- The dependency's system `meshtasticd.service` was enabled for boot. It is now
  masked; installation instructions mask it before dependency installation, and
  the supervisor refuses startup while a conflicting system service is enabled
  or running.
- User-unit sandbox settings caused root UID remapping and implied privilege
  restrictions that broke the scoped sudo check. The user service now retains
  host credentials, uses normal root-owned-file DAC and the same narrow sudo
  grant. A fresh login/reboot applies the new group membership. This does not
  grant the UI root or permission to change firewall rules.

Historical private-only policy (superseded by the dual-channel policy below):
firmware can send identity/telemetry automatically on enabled channels. The
earlier pre-power persisted-profile check permitted transmission only when **every**
enabled channel, including primary, has a private key. Mixed public profiles are
permitted only with transmission explicitly disabled. The original mixed profile
was refused before Cap power-on in an actual-device test. Empty/incomplete or
unqualified preference schemas are also rejected: firmware can discard them
and reset to public defaults. Schema 24 is qualified for this integration.

Verified on the actual device in temporary receive-only mode:

- Radio startup, real admin configuration readback and confirmed position/MQTT
  Off; generated share data contains only the selected private channel. Keys and
  URLs were inspected in memory and withheld from logs.
- Guardian SIGKILL and supervisor SIGKILL: daemon stopped, Cap power returned to
  zero, recovery record cleared; user service restarted with radio Off.
- Missing firewall prevents startup without powering the Cap; rule restored.
- Scoped sudo check succeeds; the firewall `ensure` action is denied to the app
  user without interactive administrative authorization.
- Warm reboot: firewall boot unit active, system daemon masked, Cap power zero.
- IPv4 LAN probe: reachable SSH control and blocked radio API. On-device isolated
  network namespaces proved IPv4 and IPv6 API drops with reachable control ports.
  Physical IPv6 LAN reachability was unavailable, so that path is not claimed.
- Matching Debian arm64 package/install checks and all **27** Python regressions
  pass after the changes. Native C++ and sanitizer results above remain valid;
  this follow-up changed backend/deployment policy, not the compiled UI.

The operator approved the private-only profile. It is now installed and was
verified through real daemon startup/admin readback: exactly one enabled private
primary channel, existing key preserved, regional settings/modulation/frequency/
hop limit unchanged, and position/MQTT Off confirmed. An explicit regional
frequency slot preserves the peer frequency without an absolute-frequency
override. Radio was stopped afterward and Cap power verified zero; transmission
is enabled for the next deliberate private-radio start. No RF test messages,
traces or positions were sent; ordinary firmware traffic during the approved
private startup may occur. The final rebuilt package was installed and its
supervisor source hashes checked against the reviewed working tree.
Actual peer delivery/ACK behavior, GPS/camera behavior and hands-on navigation
in this installed candidate remain operator qualification checks. The reboot
above was a warm reboot, not a physical cold-power cycle.

## Public/private support — 2026-11-15

The operator requested a general-purpose app supporting both channel types. This
supersedes the app-wide private-only restriction above; it does not change the
operator's separate, approved private-only device profile. Public text, DMs,
imports (including unnamed defaults), traces, requests and selected-channel QR
sharing are now supported. All actions keep explicit channel validation.

Public location is a separate opt-in. The startup preflight durably disables
unapproved public location before firmware starts; confirmed choices are bound
to channel identity/precision and survive restarts. UI confirmations default to
Cancel; automatic public precision is capped at 15 bits, while manual coordinate
text warns that it is precise. Private location is unaffected.

36 Python regressions cover mixed routing/no fallback, default-key classification,
public/private QR isolation, public location consent/readback/restart/revocation,
invalid/full-set imports and frequency-slot validation. Native LVGL tests walk
public/private group sends, DM selection/change/removal, location Cancel/confirm
and Settings flows. Device RF interoperability and physical keyboard/camera/GPS
qualification remain separate from these synthetic regressions.

Dual-channel offline verification: all 36 Python regressions pass in the matching
Debian arm64 package environment and directly on the device with fake radio/sysfs
controls; macOS passes 35 with the Linux-only test skipped. Native LVGL keyboard
regressions and ASAN/UBSAN checks pass (LVGL leak-cache limitation above applies).
Fresh Debian arm64 package installation, service/sudo policy and isolated
firewall checks pass. Synthetic 320x170 renders were visually inspected for the
public/private and DM pickers and both public-location confirmations. No real
public-channel traffic or personal profile changes are part of these tests.

## Publication state

The 0.1.3 source remains a release candidate. The GitHub source and companion
guide include the review fixes and both channel types; the historical v0.1.0
release notes point to this candidate and its qualification limits. Publication
does not certify live public RF delivery or the remaining hardware checks, and
the existing release tag does not contain the later implementation.

## 0.1.2 actual-device qualification

These results supplement the historical sections above. Raw logs, peer IDs,
coordinates, keys and the operator's profile/backup remain outside this tree.

- An old-device traceback confirmed the idle Error cause: timeout cleanup tried
  to signal an elevated sudo child and failed with EPERM. The replacement
  capability-owned firewall monitor works under the actual packaged user unit
  with NoNewPrivileges. No repeated sudo probes remain in the app watchdog.
- Guardian recovery now normalizes the vendor's enabled brightness value 255;
  actual stranded-state recovery passed. A separate NFC runtime overlay claimed
  LoRa's BUSY GPIO; read-only preflight identifies it before power-on. A physical
  off/unplug/on cycle cleared that conflict without unbinding a driver.
- Operator-confirmed cold power, changed boot identity and live timestamps proved
  the firewall active before any radio start. ADB/hotplug and the separate system
  daemon remained inactive after boot.
- A deliberate private message received an ACK from the intended peer. The
  operator confirmed receipt and replied; the matching reply arrived. Public RF
  traffic was not exercised and remains a separate interoperability check.
- The built-in CSI camera decoded the exact wholly synthetic QR target. Measured
  capture startup exceeded the original deadline; the corrected scanner has
  bounded capture dimensions, retry count and total time, with no saved frames.
- Isolated fresh app state started the actual daemon receive-only with location/
  MQTT Off. Create, rename, rotate, disable, enable and remove passed real admin
  readback; named-profile save/preview/offline apply and subsequent RX-only
  startup passed. Firmware-generated empty submessages and derived modem fields
  are compared semantically, rather than as serialized bytes.
- Planned GPS/preset/slot changes and hop-limit writes returned online with
  confirmed requested values; original tuning and TX mode were restored.
- Actual supervisor SIGKILL closed the API, restored Cap power to zero, and
  restarted Off. A committed synthetic draft/pin survived; history count stayed
  unchanged. Only the synthetic test conversation was then removed.
- IPv4 LAN probes were blocked while the actual API was listening on loopback
  and the SSH control was reachable. The physical IPv6 control was unreachable,
  so that path is unqualified; isolated IPv4/IPv6 namespace checks still pass.
- Native arm64 GitHub build/install/tests and a downloaded artifact's checksum/
  source-commit verification passed. Final source revisions repeat those gates.

The receiver had been configured absent. Enabling it worked through planned
restart/readback, but no valid fix was observed indoors. The operator cannot
provide clear sky now; outdoor fix/freshness is explicitly deferred. The previous
receiver setting has been restored, with automatic sharing still Off.
Package installation and sustained radio stability are tracked
in [the implementation checklist](implementation-plan.md). Earlier source-stage
hardware results must not be mistaken for verification of an older installed UI.

### 2026-11-16 completion evidence

After the radio-path fixes, the installed candidate ran for **30m12s** with
**119 healthy samples, zero failures and zero supervisor restarts**. The longest
recorded firewall-verdict age was 2.01s against the seven-second limit. This is
bounded qualification evidence, not a guarantee against every future failure.
The operator also confirmed the added settings screens open and Esc returns.
An actual diagnostic export had mode0600 and only the documented safe fields.

A final storage-only correction commits RAM drafts when saved history is
re-enabled, without waiting for another edit, and updates all connected UIs.
Both CI jobs passed **67** backend regressions plus native ASAN/UBSAN and Debian
arm64 package/install/policy checks on that functional source. The downloaded
artifact's checksum and source ID were verified. That exact CI artifact was
installed afterward: all ten runtime hashes matched, saved data survived, and
original keys/tuning/TX/GPS settings were confirmed. Radio online and firewall
health readback passed. ADB/hotplug were disabled and inactive again over SSH;
no temporary source override remains. This follow-up does not change the radio
lifecycle qualified above, and is not represented as another 30-minute run.


## 0.1.3 settings and conversation layout

Settings retains Radio first and groups related radio, channel/node, location/
Internet, messaging, notification and application options. Action identifiers are
independent of visual row order. Headings are not selectable; tests walk all 26
entries, both selection bounds and existing privacy/configuration actions.
Long information screens support keyboard scrolling and Escape.

Conversations keep channel/peer context in a fixed header above the scrolling
messages and use the footer solely for keyboard help or compose input. Long peer
names are ellipsized after the channel/classification. The route is revalidated
for display when a DM selection changes. Native tests measure actual font/text
and object bounds at 320x170, including selection, composing and return to the list.
ASAN/UBSAN and all 67 backend tests in the Debian arm64 package gate pass. Current
screenshots are synthetic renders.

The actual checksummed CI artifact from the 0.1.3 functional source was installed
on the development device. All ten runtime hashes matched, current channel keys/
configuration and saved messages survived, and actual radio-online/API/firewall
readback passed. Launcher refresh passed. This UI change does not establish the
previously deferred outdoor GPS/public-RF/physical-IPv6 checks.
The operator subsequently confirmed the Settings order and the separate channel
header/keyboard-help layout on the actual device.


## 0.1.4 explicit QR scanning

Scan QR code opens instructions with `Return: scan`; the camera does not start
until Return. The screen immediately shows Scanning while the supervisor performs
capture/decode and confirmed channel import. Repeated Return does nothing while
busy. The matched result shows imported count, already-present channels or an
error with `Return: retry`. Escape leaves the view, with the operation allowed to
finish; disconnect reports uncertainty and asks the user to check Channels.
The developer-only simulator note has been removed and keyboard hints use normal
text contrast.

Native ASAN/UBSAN regressions cover this flow, stale/dismissed replies, unsolicited
setup updates, busy key repeats, send failure and disconnect/reconnect. Ready and
busy keyboard hints are checked against real-font pixel bounds at 320x170;
rendered examples use synthetic data. All 68 backend cases and Debian 13 arm64
package/install/policy checks pass. Existing in-memory camera capture and import
validation are unchanged. These offline results do not claim a new physical
camera scan.

Both GitHub CI jobs passed on functional source `8213d63`:
[workflow run](https://github.com/Prophet60191/m5stack-cardputer-zero-mesh-app/actions/runs/43878767196).
The downloaded 0.1.4 package's source ID and checksum were verified before
installation. All ten installed runtime hashes match that artifact; current
channel keys/tuning/TX/GPS/privacy settings, app preferences, conversations and
drafts are unchanged. Saved messages survived. Actual radio-online/API/firewall
readback and launcher refresh passed. No test texts, location requests or traces
were sent. The previous outdoor GPS/public-RF/physical-IPv6 limits remain.

## 0.1.12 second maintenance pass

The [second-pass record](maintenance-pass.md#second-pass-implementation--2026-11-17)
describes all seven corrections, selected cleanup, regression evidence and
remaining qualification. Local source and package gates pass, including 102
backend tests. Installed-runtime evidence is added there separately. No new
outdoor GPS fix, public-peer delivery or physical IPv6 qualification is implied.

## 0.1.13 unified channel setup

The [flow implementation record](channel-setup-flow.md) separates source tests,
first-candidate hardware readback and the final candidate's qualification. The
form preserves local entries, makes shared radio effects explicit, binds/cancels
reviews and verifies saved settings, with explicit partial/uncertain results.
Existing profiles/chats are preserved; no new outdoor GPS, public-peer RF or
physical IPv6 qualification is implied.


## 0.1.14 form-as-review and contextual actions

The [usability qualification record](channel-setup-usability.md) covers direct
fixed-key Save, named imports, removal of redundant footer navigation, retained
specific consent/validation and exact package/device evidence. Original saved
state is preserved and no new RF send is claimed. A pre-install .13 firewall
check failure recovered two seconds later; its cause is unknown. Fixed safe
failure labels improve diagnosis without changing fail-closed behavior. The
bounded final ten-minute RX run does not establish a root-cause fix.
