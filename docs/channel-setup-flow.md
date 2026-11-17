# Unified channel setup — implementation and qualification

Implemented and installed, 2026-11-17. Mesh 0.1.13 brings scan/paste, public/private
creation, channel details, shared tuning, privacy, optional named profiles and
opening the resulting conversation into one flow. Radio remains first in
Settings. GPS dongle support remains a separate later task.

## Behavior and ownership

Settings keeps a local draft across field editors, review and validation errors;
no settings change before explicit Save. Fixed headings/help remain visible while
fields scroll. Escape from editors/review keeps entries and an imported token;
closing the entire flow cancels both. Routine receive-only saves apply directly
from Review. Transmission/antenna, public location and MQTT require their relevant
explicit acknowledgments. Repeated Return cannot submit another pending save.

The supervisor validates and owns generated/imported keys, connection-bound
previews (maximum eight, 120-second expiry), source fingerprints, cancellation and
serialized writes. It reserves each preview request before worker preparation,
so cancellation during preparation also wins before any write. Stale, cancelled,
expired and disconnected previews cannot commit. Keys/URLs are not returned in
preview/result events. Pasted URLs necessarily exist in UI memory; masking is
not encryption or secure erasure.

Radio tuning is shared by every channel. QR tuning is an explicit suggestion,
never silently adopted. Auto still depends on the unchanged primary channel name.
Existing fixed frequency/calibration are displayed and preserved; a fixed
frequency may be explicitly cleared, but this form cannot introduce arbitrary
new frequencies. Changing region with an existing fixed frequency is rejected.
Imported fixed frequencies and calibration offsets are not silently copied.

New channel location/MQTT default Off. Multi-channel imports can select exactly
one new automatic-location channel within the form; consent/precision follow its
actual privacy. Choosing that target disables automatic location on other
channels. Existing sharing is otherwise preserved. GPS receiver configuration
is independent. TX defaults to the current mode; enabling it or retuning while
TX requires antenna/region consent. Firmware may broadcast when TX is allowed.

Separate-profile-only saves start RX/location/MQTT Off and leave the live setup
and radio unchanged. Active saves stop/apply once, verify persisted preferences
and restart once only if previously Online, then verify actual radio readback.
An offline save stays Off and reports stored verification. Failed apply attempts
restore previous preferences/consent and stop the radio; failed restoration is
explicit. Optional profile/history follow-up failures report partial success
without implying the channel should be added again. Unknown results require
inspection before retry; configuration and RF actions are never replayed
automatically. This recovery is not a power-loss-atomic transaction across every
file or a guarantee that an interrupted write can always be restored.

The old socket import API remains compatible. Optional `setup:true` permits QR
capture/preview while Off without powering the Cap. Obsolete standalone native
scan/import/create branches were removed. Conversation opening resolves the
new stable identity against current channels, avoiding numeric-slot retargeting.

## Exact source and artifacts

- Runtime/package source and `v0.1.13-rc.1` tag:
  `88fd8e53b55a7247807ae2d7facb550a7ef484df`.
- Installed archive: `cardputerzero-mesh_0.1.13-rev1_arm64.deb`, SHA256
  `1aa987ad6b79ed3e35ac6c8b42d8c532c8c58f698f1d10c7f2857d108647370a`.
- [Exact-source code/package CI](https://github.com/Prophet60191/m5stack-cardputer-zero-mesh-app/actions/runs/44150240654)
  and [guide CI](https://github.com/Prophet60191/m5stack-cardputer-zero-mesh-app/actions/runs/44150240657)
  pass. Later closeout edits are documentation only.
- Reusable draft/scope/cancellation/partial-result lessons are in app-guide
  `a64d24b4e37e5e647c1d211f957751333978dda5`; its 22 tests and matching CI pass.
  No Store or upstream submission was made.

## Verification

| Scope | Observed result |
|---|---|
| Backend | All 119 tests pass, including 17 setup planning, privacy, rollback, staleness, cancellation and multi-location cases |
| Native/offline | Four macOS ASAN/UBSAN CTests, five Linux desktop CI CTests and three Debian arm64 sanitizer CTests pass; real LVGL, keyboard/input, legacy import/edit and lifetime/bounds regressions retained |
| Debian package | Release build, archive/license inspection, fresh install, actual 0.1.8 upgrade, service/policy/firewall checks and non-publishing Store preflight pass (zero errors; optional localization advisory remains) |
| Installed files/security | All 104 regular files match archive hash/mode/root owner; dpkg verify clean; actual supervisor UID1000, capabilities zero, NoNewPrivileges enabled |
| Device owner/API | Cancelled review changes nothing; profile-only preserves running setup; live RX-only add/retune verifies channel identity and tuning; offline synthetic QR import remains Off; original channels/tuning/profiles restored afterward |
| Device UI | Matching CI native setup replay passes in both themes with installed Noto fonts; actual native GUI opens framebuffer and built-in keyboard using the exact installed executable; launcher restored without supervisor restart |
| Preservation | Private consistent SQLite/file backups retained; both original profiles, keys/roles/tuning, identity, TX/GPS/privacy, INI and all 19 messages/3 thread rows/preferences preserved |
| Stability | Final installed source: 61 healthy samples over 600 seconds, zero failures/restarts, one supervisor PID; maximum firewall-verdict age 1.99 seconds |
| Documentation | 28 synthetic screenshots, all links/version/image checks, visually reviewed 40-page manual and one-page reference; exact CI PDFs copied to Desktop with the previous edition archived |

The earlier `c5777c2` candidate also passed owner/API, native UI and a separate
61-sample ten-minute RX check before the final multi-location/consent refinement.
Its archive is not the final installer above. One initial QA attempt connected
before socket readiness; retry after a readiness gate passed, without a service
restart. No implementation failure was inferred from that test-startup race.

Normal profile apply introduced only empty settings/module submessages in
previously disabled slots during the first qualification; every channel value
and identity was independently compared. Normal transmit-history bookkeeping
changed during radio restarts. These are not claims of byte-identical runtime
state. The final installation/qualification used a fresh private backup.

## Limits and official references

No RF message, traceroute or location request was sent. No peer compatibility,
ACK/delivery, physical-key acceptance or new camera scan result is claimed for
this change; the optional physical/peer checks were unanswered during this run.
The RX-only save/readback tests exercise configuration rather than end-to-end RF.
Outdoor GPS, USB GPS priority/fallback and physical IPv6 qualification remain
unperformed. Leak detection for LVGL global caches remains disabled; sanitizer
passes are not a general leak audit. ADB and hotplug remain active/enabled under
the owner's standing instruction. Surveyor, EN1's radio/service, shared drivers,
network policy and the backup timer were not changed.

The qualified firmware's [RadioInterface.cpp](https://github.com/meshtastic/firmware/blob/54e0d8d/src/mesh/RadioInterface.cpp)
shows shared tuning, primary-name Auto hashing, slot modulo semantics and fixed
frequency/calibration precedence. The [official channel documentation](https://meshtastic.org/docs/configuration/radio/channels/)
distinguishes radio-wide modem configuration from channel keys/groups. These
references do not qualify a newer firmware/client combination; this app keeps
its existing pinned 2.7.26/2.7.11 dependencies.
