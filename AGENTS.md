# Cardputer Zero Mesh

Current instructions: docs/install.md (operations), docs/distribution.md (build/source
assets), docs/README.md (index). Current installed runtime/package is 5776364
(Mesh 0.1.20); later documentation changes do not imply another installation.
Older dated notes below are historical and do not override those current runbooks.

## Current focus — everyday use (2026-11-24)

Owner has closed the UI checkup and will use installed Mesh 0.1.20 (exact
runtime/package source 5776364), reporting concrete issues before more changes.
Do not resume implementation, cleanup, deployment or publication without a new
request; start future fixes from the screen/action, expected result and actual
result. See docs/ui-checkup.md and docs/current-status.md for qualification and
deferrals. Last device verification: Cap absent, Radio Off, ADB On, saved data
and settings preserved. Ordinary owner use may change that state; verify it
before future maintenance and retain the standing instruction to leave ADB On.

Native C++/LVGL UI (`src/`) talks to a Python radio supervisor (`supervisor/`)
over newline JSON. The UI owns no radio hardware. Read `CHANGELOG.md`,
`docs/security.md`, and `docs/review-remediation.md` before changing lifecycle,
privacy settings, protocol, or package integration.

- UI/build code is MIT; the supervisor and guardian/firewall helpers are GPL.
  Preserve upstream M5Stack attribution and both license texts.
- Test commands and build requirements are in `docs/build.md`. Regression tests
  use synthetic data, real LVGL/protobufs, and fake radio/sysfs controls.
- Device builds/package validation need a matching Debian arm64 environment.
  Inspect the actual package, sudo policy, and service files.
- Never put channel keys, share URLs, runtime state, logs, personal device IDs,
  addresses, or real chats into fixtures/screenshots. Known public default-key
  constants and synthetic keys in tests are intentional validation inputs.
- Keep hardware qualification distinct from simulator/container results. Do not
  power a Cap without its antenna; do not deploy, publish, or transmit RF as a
  side effect of running the offline regression suite.
- Keyboard changes must use the official map and independent fixture linked in
  `docs/keyboard.md`. Keep the reusable decoder and raw evdev-to-textarea checks
  together; direct text insertion is not proof of physical-key translation.

## Channel setup usability closeout — 2026-11-17

Mesh 0.1.14 is installed from exact runtime/package source `e8dadd3`: form-as-review,
fixed `1: Save channel/profile`, clear named scan success and contextual footers.
HELP, validation and specific radio/privacy/antenna consent are preserved. Read
[`docs/channel-setup-usability.md`](docs/channel-setup-usability.md) for exact CI,
124 backend/native/arm64/package gates, live UI/replay, private data preservation
and the ten-minute RX result. Later closeout changes are docs only. A pre-install
.13 firewall check briefly failed; its cause remains unknown. Fixed safe reason
labels improve diagnosis without weakening shutdown or automatically restarting.
Keep ADB enabled; preserve original settings/profile/chats and other apps/services.
GPS dongle model/interface/source/fallback design remains the next separate task.

## Unified channel setup closeout — 2026-11-17

Candidate 0.1.13 is installed from exact runtime/package source `88fd8e5`; its
complete form, verified saves, preservation cases and qualification limits are
in [`docs/channel-setup-flow.md`](docs/channel-setup-flow.md). All 119 backend
tests and native/arm64/package gates pass. Later closeout changes are docs only.
Preserve settings/profiles/chats, keep ADB enabled and leave Surveyor/EN1 service
ownership intact. One private T-Deck message received a persisted Recipient ACK; original radio,
profile and data preservation are verified. The owner subsequently passed the
specified physical form/`Test/1` entry/Review/Discard check; saved state remains
unchanged. Owner physical camera capture decoded TestQR013 and displayed its private
classification; this exposed the .13 clarity problem being fixed above. A full
physical keyboard sweep and public-peer checks remain unperformed.
GPS dongle work is next: establish receiver model/interface and explicit source,
priority and fallback before implementing. Do not claim automatic USB support.

## Maintenance closeout and next work

The owner resumed the second pass after Surveyor finished. All seven maintenance
items and the simplification review are complete; see
[`docs/maintenance-pass.md`](docs/maintenance-pass.md#second-pass-implementation--2026-11-17)
for source/package correspondence, regressions, device measurements and limits.
Functional/package source is `2e0035c`; later closeout changes are documentation
only. Candidate 0.1.12 passed its own installation and owner-confirmed physical navigation;
0.1.13 supersedes it as documented above.
Preserve features, the modular UI/supervisor boundary, saved mesh profiles and the active setup
and all privacy/lifecycle safeguards. Keep ADB enabled under the standing owner
instruction. Leave Surveyor code and shared hardware ownership intact.
GPS dongle support is the next separate task; do not claim automatic USB priority
or a qualified outdoor GPS fix. Establish receiver model/interface and explicit
source/fallback behavior before implementation.

## Deferred keyboard-alignment review — 2026-11-18

Owner requested a follow-up log only. See
[the pending review](docs/keyboard-alignment-follow-up.md): compare this app with
current official guidance/input implementation and justify any differences with
reasons, tradeoffs and evidence. Alignment has not been newly established. Resume
when the owner asks; no keyboard changes or device tests are authorized by this note.

## Everyday-use refinement — 2026-11-22

The 0.1.15 candidate and current validation are documented in
[the refinement record](docs/usability-refinement.md). Preserve its Cancel-first
deletion, stable action targets, grouped form, truthful delivery/mode wording,
readable values and explicit profile application consequences. Do not infer new
radio/privacy defaults from presentation changes. Keep ADB enabled. Device
installation/qualification belongs in that record, separate from offline tests.

The exact 0.1.15 CI artifact is now installed; file hashes/modes/ownership,
ordinary-user synthetic replay and owner physical navigation/deletion-cancel
checks passed. Current qualification/source links are in the refinement record.
The startup CLI does not implement `--version`; use About or package metadata
for version checks rather than inadvertently starting another framebuffer UI.

## Phone access qualification — 2026-11-23

0.1.16 is installed from functional source `8cfa3b6`; later documentation commits
do not alter that runtime. See docs/phone-access.md and the current evidence in
docs/phone-access-validation.md before further phone work.

- Optional, selected-device PhoneAPI gateway behind separately administered
  private Tailscale Serve. Preserve one physical meshtasticd connection, the raw
  daemon firewall, typed intent boundary, TX/privacy/channel identity checks and
  actual-daemon heartbeat responses. No phone admin/location/MQTT writes, public
  forwarding or app root/operator grant.
- All 158 backend tests, native sanitizer checks, Debian arm64 package gates and
  exact installed verification (114 regular files) pass. Distinguish protocol
  fixtures and RF ACKs from actual handset observations.
- Owner confirms official App Store iPhone connection, nodes/channels, two-way
  messages, reopening after lock, and Wi-Fi interruption recovery. Access Off/On
  and radio stop/start recover, but each needed manual handset reconnect.
  Settings/profile hashes were preserved across the radio restart.
- Owner reported one foreground disconnect with screen awake; its cause remains
  unresolved. A later synchronized repeat passed: owner-confirmed five minutes,
  at least 317.2 seconds of TCP activity without FIN/reset, 31 connected status
  samples with RF Online/send-receive, healthy firewall and one radio connection.
  Owner then confirmed post-idle private message/reply in both directions and
  both displayed on Lockon. Do not claim the earlier drop was fixed or full
  intermittent reliability accepted.
- Cellular and full physical cold-power checks pass: owner confirms both message
  directions over iPhone mobile data, and again after cold boot without new
  pairing/sign-in. VPN/Serve/ADB, settings/profiles and selected phone persist.
  Final state: Online/send-receive, phone connected, healthy firewall, one radio
  connection. Automatic radio start remains Off by existing preference; Lockon
  requires no SIM for its Wi-Fi/Tailscale path.
- Short acceptance checks are complete; longer everyday use and the unexplained
  earlier awake-screen drop remain follow-up items. Do not erase that failure
  because a later five-minute trace passed. Payload-free connection tracing is
  the next diagnostic step if it recurs.
- Bounded packet-header capture has finished. Reduced, payload-free timing/status
  evidence is ignored under build/phone-access-016/manual-checks. Never publish
  peer IDs/addresses/auth links/keys/chat text or private qualification backups.
- Preserve settings/chats and other apps/services. Leave Radio in send/receive,
  selected Phone access On and ADB enabled. No production change was made during
  these extended tests; guide/PDF reconnect instructions were refreshed.

## Current task: Cap GPS — 2026-11-23

Read docs/internal-gps-validation.md. Owner's actual outdoor watch ran 321.8s:
app no fix, although daemon log had local coordinates. Exclusive guardian/lease
60s raw diagnostic confirmed 59 fix samples, up to 27 satellites used and zero
checksum failures, then restored Online/send-receive, GPS enabled, sharing Off.
No dongle or receiver firmware change is needed for this cache defect.

0.1.17 candidate adds read-only node-only nonce 69421 snapshots on the existing
API connection (30s interval, 10s completion timeout/backoff). Preserve library
channels/caches, sharing/TX settings and single serial/TCP ownership. No location
RF packet or synthetic timestamp. PositionLite omits satellites; do not fake them.
All 167 backend tests, both source-correlated CI jobs and arm64 package gates
pass for b430b20. Exact 0.1.17 package is installed; 116 file hashes/modes/owners
and all 24 original messages/settings/profiles are verified preserved. The
restarted owner reports gps_refresh current, one TCP connection, Online/send-
receive, GPS ENABLED, location/MQTT sharing Off and ADB active. The owner is asked
to open Map and position outside again; wait for confirmation before starting
the corrected five-minute watch. Outdoor/map/freshness acceptance is pending. Helpers/reduced evidence are ignored in
build/internal-gps-qualification. Never commit raw coordinates, device IDs or
private endpoints. Preserve chats/profiles/phone access/other apps; leave ADB On.
User may bring device indoors while the correction is built. Do not begin the
USB GPS work until this existing receiver/app path is qualified.

Outdoor retest update: first corrected interval encountered native GPS probe
give-up (no detected model, no stream), and was interrupted for diagnosis. A
read-only identity query matches native ATGM336H detection at 115200; periodic
NMEA was absent, consistent with native probe disabling output before failing.
A normal restart recovered and the installed app now reports valid/fresh coords
(age ~19s), proving the cache correction live. A clean watch is running in
outdoor-confirmed-watch.jsonl; do not confuse it with the interrupted
outdoor-corrected-watch.jsonl. Intermittent startup detection remains unresolved.
No receiver firmware/permanent setup change or dongle work.

Clean recovered-state run now passed: 31 samples/321.3s, all valid coords and
radio/firewall/privacy healthy; 21 fresh/10 correctly stale samples and six
freshness transitions as successive source fixes arrive. Physical Map-marker
question is pending. Keep startup reliability unresolved; next investigate
native ATGM336H probe intermittently failing despite a valid direct identity
reply at 115200 (no periodic output after failed probe). Do not change shared
dependencies or permanent GNSS settings speculatively. Wait for Map confirmation
before any further radio interruption, so that check is not invalidated.

Owner confirmed Map marker and reconnected indoors via USB. Root cause measured:
7/8 ATGM identity replies took 613-977ms, exceeding native 500ms probe window.
Prepared pinned upstream 54e0d8d timeout-only 1500ms correction, actual-parser
ASan/UBSan replay (old1/8/new8/8), native arm64 build, real SDK/sim-radio smoke and
pristine package/upgrade/mask checks. Support is in scripts/meshtasticd-gps; native
workflow must export full corresponding source with binary. Nothing patched has
yet replaced the on-device daemon. Current user settings are preserved, GPS
enabled, RF send/receive, sharing Off, ADB On. Next: exact CI native artifact,
qualified dependency package install with rollback, repeated physical GPS starts
and compatibility checks. Do not flash the receiver or silently change settings.


## GPS native startup correction installed — 2026-11-23

Current status supersedes the intermediate pending notes above. Owner confirmed
Map marker after the successful 321.3s outdoor app watch. Exact CI native binary
from support source 759dcbe is now installed as
`2.7.26.61~obs54e0d8d~beta+czgps1`, alongside app 0.1.17. Eight real normal radio
starts pass ATGM detection; private recipient ACK and real phone-gateway SDK
handshake/encrypted recipient ACK pass. All 332 native installed files and original
messages/settings/profiles verify; standalone daemon remains masked/inactive,
one physical API connection, sharing/MQTT Off, Radio send/receive, Phone access
On/ready and ADB On. Full hashes/rollback/source/CI evidence are in
`docs/internal-gps-validation.md`. No receiver flash, new privilege or upstream
submission. Do not identify the patch by its unchanged compiled version banner.
Final combined outdoor fix/freshness is requested; wait for placement confirmation
before starting the five-minute watch. Keep helpers for that check, then clean
only exact task-created temporary paths. USB GPS remains deferred. The remaining
nonfatal receiver config ACK warning and earlier unrelated phone-drop report
are not claimed fixed. Retain backups privately outside Git.

Subsequent indoor status also reports valid/fresh coordinates (source age 65s),
confirming both installed fixes together. This is not a new five-minute outdoor
interval; the requested final placement confirmation remains pending.


## Fitted GPS acceptance complete — 2026-11-23

This supersedes the requested/pending outdoor notes above. Owner confirmed outside
with Map open; exact app 0.1.17/native +czgps1 passed the final SSH watch:
31 samples/321.5s, all valid/self coordinates, three newer fixes, 24 fresh/seven
correctly stale samples, five freshness transitions, final age 84s. Radio remains
Online/send-receive, GPS enabled, refresh current, firewall healthy and sharing/
MQTT Off throughout. Post-watch readback remains fresh; standalone daemon masked.
See docs/internal-gps-validation.md for bounded acceptance and prior eight-start/
RF/phone checks. No settings, coordinate broadcasts or additional reader.
USB GPS can now be addressed as a separate task: identify the dongle and agree
source/priority/fallback; do not assume it is automatically detected or preferred.
Preserve selected Phone access, settings/chats, other apps and ADB On. Bounded
root-owned task temporary helpers remain idle for cleanup at the next USB
maintenance connection; verified private rollback/backups are retained on host.
Do not claim the nonfatal config ACK warning or earlier phone-drop cause fixed.


## Header presentation closeout — 2026-11-23

Mesh 0.1.18 final source/package 2913c01 is installed. Read
`docs/header-presentation.md`: icon removed, numeric percentage retained;
actual partial-framebuffer sibling-text failure was caught and fixed with bounded
header invalidation. Font preloading did not fix it and was discarded. Exact
final package SHA/file verification, 167 backend/four native checks and both CI
jobs pass. Do not identify the earlier internal same-version candidate as final
by version alone. Original 26 messages/settings/profiles are preserved. Launcher
restored, Radio Online/send-receive, GPS enabled, location/MQTT Off, Phone access
On, ADB On, native +czgps1 retained. No new outdoor/handset/cold-power claim.
README/current status, synthetic illustrations, Desktop and existing Drive manuals
and the app builder guide are refreshed; old versioned archives stay historical.
Audit/sync secondary source/backend mirrors with private backups; preserve their
independent hardware README notes. USB GPS is deferred by the owner.

The 0.1.18 USB closeout also removes the bounded GPS/header temporary helpers
and stages (57 exact paths), after verified private host backups. Installed
117 files, 20 device mirror modules and unchanged native GPS hash are reverified.
No cleanup/reinstallation remains pending from those temporary helper notes.


## Store review and GPS guide — 2026-11-23

Read docs/store-submission-readiness-20261123.md before submission work. Current
catalog b6c559a assigns MESH to meshtandem; older CI/preflight against 94eb8d7 is
not current availability. Exact installed final 0.1.18 package has one current
local blocker (code collision); clean-image dependencies, durable native GPS
patch/source downloads, demo and final release/privacy checks remain. CZMS/CMES
are suggestions only, not reserved or applied. No submission/upload/auth or app/
device/metadata changes in this review. Guide GPS recipe reuses official parsing/
ownership/freshness and adds integration/qualification lessons; do not label
Meshtastic's native probe/cache fixes as M5Stack GPS-app defects. Dongle deferred.

## Self-contained distribution candidate — 2026-11-23

0.1.19 packaging work is in progress; 0.1.18 remains installed until exact final
CI artifact qualification. Read docs/distribution.md and the readiness report.
Private native GPS runtime and 15 locked wheels replace global-daemon/PATH and
manual-venv setup. Preserve legacy venv and all owner state. Minimal fresh Debian
container offline cache/import/remove checks and current CZMS Store preflight
pass for the prototype; no final source-correlated acceptance implied.
Owner has no spare card: fresh-image hardware test explicitly deferred, never
erase Lockon. Owner will record video; no Store submission or public release.
Leave ADB enabled, global standalone daemon masked, TX/privacy/GPS/phone choices
unchanged. Do not conflate bundled source completeness with an OS/toolchain image.


## Distribution qualification closeout — 2026-11-23

Mesh 0.1.19 exact runtime/package source 8ada52a is installed; later closeout
changes are documentation only. Read docs/distribution-validation-20261123.md
and docs/current-status.md. Both CI jobs, 175 backend/native/package/upgrade/
firewall and minimal Debian offline cache/QR/remove checks pass. All 255 installed
files match CI. Three real GPS/radio starts, 77.8s monitored stability, actual
header/UI lifecycle, private Recipient ACK and settled real SDK gateway/heartbeat
checks pass. 26 original messages/settings/profiles remain; one test makes 27.
Legacy venv/global native unchanged, global daemon masked; no root/capability
app process and one physical API connection. Keep ADB On, Radio send/receive,
GPS enabled, sharing/MQTT Off, selected Phone access On and other apps untouched.
Runtime first launch is offline; licenses/source paired with installer. Python
Pre-Depends orders preinst; purge works without Python; manual checksums cannot
clobber installer/source checksums. Targeted source/history/archive sensitivity
checks pass in their recorded scope. Current dad9338 catalog policy is unchanged
from pinned b6c559a and CZMS remains unclaimed, not reserved.
Owner has no spare card: fresh-image hardware check explicitly deferred; never
erase current setup. Owner will record video; no public/Store/upstream submission.
USB GPS/SIM, public-peer/IPv6/full keyboard and long-term/earlier phone-drop
limits remain. App/guide public sources and Desktop/Drive/mirrors are refreshed
with independent hardware notes/private backups preserved.
