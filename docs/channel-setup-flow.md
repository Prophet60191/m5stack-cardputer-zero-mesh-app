# Unified channel setup — implementation record

Status: in progress, 2026-11-17. Owner approved one guided place for scan/paste,
public/private creation, channel details, shared tuning, privacy, optional named
profile and opening the resulting conversation. Preserve existing profiles/chats,
Radio first, keyboard behavior and modular ownership. GPS dongle work stays later.

Implementation uses a local Settings draft (`settings_channel_setup.cpp`) across
field editors/review, with no writes before explicit Save. The supervisor owns
connection-bound, bounded/expiring previews, hidden generated/imported keys,
strict validation and a single complete preference transaction. Tuning applies
to every channel. Separate-profile-only keeps the live setup/radio untouched
and starts RX/location/MQTT Off. An active apply restarts once if already Online;
offline apply stays Off and distinguishes stored verification from live readback.
Failed apply attempts restore previous preferences and stop the radio; failed
recovery is explicit. Optional profile failure must never claim it was saved.

Existing channel.preview/scan accepts optional setup:true to prepare while Off,
without powering hardware. Legacy preview/import behavior stays compatible. QR
radio suggestions are optional, never applied implicitly. New location/MQTT
choices are Off; TX defaults to current mode and enabling TX or retuning with TX
needs explicit antenna/region acknowledgment. Public location/MQTT effects get
separate confirmations. Missing, expired, cancelled or stale previews cannot save.

Pending verification: focused backend and real-LVGL form/lifetime/bounds tests,
existing regressions/sanitizers, Debian arm64 package/install/upgrade/policy,
source-correlated CI, private device backup/install and actual UI checks. Use a
compatible peer only if needed; inspect current peer/project policy first and
never repurpose EN1 or enable its deprecated transport. Keep ADB enabled. No RF
or hardware test has yet been performed for this change.

Custom frequency handling: show an existing fixed frequency/calibration offset,
preserve it exactly or explicitly clear the fixed-frequency override to use
slots. Arbitrary new fixed frequencies are not accepted by this flow. Changing
region while retaining a fixed frequency is rejected. Imported fixed frequency
and device calibration are never copied silently. Imported Auto still depends
on this device's primary name; the review explains when an explicit matching
slot is needed. The tested device currently has no override/offset.

First working checks: all 102 pre-existing backend tests pass; 12 new planner/
commit/privacy/rollback cases pass after correcting rollback of an originally
absent consent file. Four native sanitizer CTests pass after migrating legacy
QR/public-creation UI cases to the unified flow and retaining cancellation,
correlation, keyboard, privacy and existing manager-edit coverage. Header/footer
bounds and light/dark synthetic renders reviewed; no live test/install yet.

Review follow-up: cancelled queued previews use a separate connection/request
reservation so cancellation during preparation wins before any write. Escape
from review/field editors preserves the imported QR token and local entries;
closing the whole form cancels both. An unrelated form-validation error does
not discard a still-valid imported key. New picker navigation changes selection
in place; obsolete standalone QR/import/create UI branches were removed while
the older socket API remains supported.

Final local gates: all 118 backend tests, four macOS ASAN/UBSAN CTests and three
Debian arm64 sanitizer CTests pass, together with Release/package/fresh-install/
actual 0.1.8-upgrade/firewall checks. No claim of a general leak audit (LVGL
global-cache leak detection remains disabled). The reviewed guide builds with
28 synthetic screenshots, 40 manual pages and a one-page quick reference; all
local links/version/image checks pass and rendered pages were inspected. Shared
guide changes pass all 22 tests. Device and exact-source CI remain pending.

Implementation references: the qualified firmware's [RadioInterface.cpp](https://github.com/meshtastic/firmware/blob/54e0d8d/src/mesh/RadioInterface.cpp)
shows shared tuning, primary-name Auto hashing, slot modulo semantics and fixed
frequency/calibration precedence. The [official channel documentation](https://meshtastic.org/docs/configuration/radio/channels/)
distinguishes radio-wide modem configuration from channel keys/groups. Current
documentation is not a qualification of a newer firmware/client combination;
this app keeps its existing pinned 2.7.26/2.7.11 dependencies.

First device candidate `c5777c2` passed exact-source code/docs CI and was installed
from archive SHA256 `b68e6a2e90fbee5a540dc60392f7e8b150f136212690d0b0c8fb451ce4dad7de`.
All 104 regular package files match hashes/modes/owners; native framebuffer/keyboard
startup and the exact CI UI replay pass on the device. Actual owner-API tests
verify cancellation, profile-only preservation, one live RX-only retune/channel
save and offline save, then restore original channels/tuning/profiles. INI, keys,
GPS/TX/privacy and all19 messages/3 draft-pin threads/preferences survive. Normal
profile apply introduced only empty settings/module submessages in disabled
slots; all channel values match. Transmit bookkeeping changed during restarts.
An initial test attempted the socket before service readiness; retry after the
readiness gate passed, with no supervisor restart. No test RF message sent.

Final review adds in-form selection of one automatic-location target for
multi-channel QR imports, so that case also stays in one place. Consent/precision
use the actual chosen channel. Native/backend coverage is extended accordingly;
this final source needs matching CI/artifact installation and qualification.
Pasted URLs are explicitly recognized as secret-bearing UI memory; masking is
not encryption or secure erasure. Remaining physical/RF checks are recorded
separately from source, native replay and owner readback.

Ordinary receive-only saves now apply directly from the reviewed summary. The
extra acknowledgment page is shown only when TX/antenna, public location or MQTT
consent is actually needed; routine local/profile saves do not add a second
generic confirmation. The summary still defaults to Back, and repeated Return
while saving cannot duplicate the operation.
