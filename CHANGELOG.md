# Changelog

## 0.1.8 — 2026-11-16 (release candidate)

- Replace the incorrect partial Sym table with all 32 official symbols, using
  the trusted system map with a complete validated fallback. Correct `/`, `:`
  and `-`; recognize driver-selected symbols independently of physical Sym state.
- Add a reusable decoder with per-device modifier/release state and separate
  built-in/USB profiles. Handle same-batch focus changes, rollover, event loss
  and disconnect; keep non-text Fn actions out of drafts.
- Consume shortcuts before LVGL text delivery. Keep Help/Screenshot available
  during editing, with Help owning input until dismissed. Preserve draft/cursor.
- Add independent vendor-map and raw-event-to-editor regressions, including the
  real device reader on Linux. Correct reusable keyboard guidance and user docs.

Validation is recorded in [the keyboard contract](docs/keyboard.md). Automated
event replay is distinct from an operator pressing physical keys.
Native ASAN/UBSAN decoder/UI checks and Debian arm64 real-reader, package/install,
policy and all 86 backend cases pass locally. User-guide PDF layout is checked.

## 2026-11-16 — User documentation for 0.1.7

- Add seven illustrated user guides, a guide index and a prominent README link.
  Review the text against current controls; clarify channel/DM privacy, incoming
  position precision, byte limits, acknowledgments and disk-history erasure limits.
- Generate the versioned offline manual and one-page quick reference from the
  same Markdown. Keep generated PDFs outside Git and the device package; ship
  Markdown/screenshots for offline reference. Add documented builds and CI checks.

Validation: all seven chapters, 24 images and local links/anchors checked;
37-page manual and one-page quick reference rendered and visually inspected.
Debian arm64 package/install/policy checks and all 86 backend cases pass locally.

## 0.1.7 — 2026-11-16 (release candidate)

- Prepare typed conversation rows in one pass, index history search once and
  sort once. Pins, archived channels and draft precedence remain intact;
  equal-time chats keep first-seen order and refreshes preserve selection by ID.
- Suppress unchanged thread-state redraws, SQLite replacements and broadcasts
  after strict validation. Genuine changes, pending drafts, history opt-out and
  correlated request completion retain their existing behavior.
- Remove unused template navbar/icon-button demos and their Hello World/counter/
  bold/info state. Preserve current navigation, app identity, attribution and
  deliberate legacy-settings migration.
- Add an optional synthetic headless benchmark, exercised by the isolated package
  gate and excluded from the installed app. Record actual-device before/after
  measurements in [the maintenance record](docs/maintenance-pass.md).

Validation: native ASAN/UBSAN and 86 backend cases pass locally. Synthetic
measurements show reduced list/search work, no redraw for unchanged draft echoes,
and no duplicate draft writes; these are not battery-life measurements. Final
Debian package/CI and installed-runtime verification are tracked separately.

## 0.1.6 — 2026-11-16 (release candidate)

- Verify the supplied 13-item report against the actual code; record corrections
  and qualifications in [the follow-up](docs/report-followup.md).
- Separate cancellable camera capture from confirmed channel import. Both scan
  and paste show a preview, handle duplicates without writes, and require a
  Cancel-first confirmation bound to the current connection/channels/tuning.
- Keep local drafts/history writes responsive during radio admin waits, send one
  greeting history snapshot, cache dependency probes outside greeting lifecycle
  locking, and guard status reads through incomplete/detached library state.
- Delete saved conversation identities/pins as well as messages/drafts when
  history is disabled, including metadata left by the old policy.
- Use canonical version metadata, separate delete targets from request IDs,
  expire generic service waits, and remove the obsolete app sudo grant in favor
  of an unprivileged health command. Preserve the red message-selection border.

Validation: native ASAN/UBSAN, real parser/store/process/worker regressions and
Debian arm64 package checks. Final CI and installed-device evidence are tracked
separately; offline tests do not perform RF or physical camera/GPS checks.

## 0.1.5 — 2026-11-16 (release candidate)

- Mark the selected message bubble with a rounded two-pixel red border in light
  and dark mode. Draw inside the bubble so parent containers cannot clip the line;
  preserve the bubble's text and layout bounds when selection changes.

Validation: existing native ASAN/UBSAN checks and Debian 13 arm64 build/install/
policy checks pass, including all 68 backend cases. Synthetic selection renders
cover light and dark mode; device installation is recorded separately.

## 0.1.4 — 2026-11-16 (release candidate)

- Wait for Return on Scan QR code before starting the camera, and show Scanning
  immediately until the matching supervisor result arrives. Ignore repeated
  Return while busy; show completion, already-present channels or a retryable
  failure. Remove the device/simulator note from the scan screen.
- Correlate scan replies by request ID; dismissed/old replies cannot replace a
  newer view. A lost connection shows an uncertain result rather than waiting
  indefinitely. Escape leaves the view without claiming to cancel an import.

Validation: native ASAN/UBSAN checks cover ready/busy/result transitions, retry,
Escape, repeated keys, unsolicited updates, stale replies and connection loss.
Real-font keyboard hints fit at 320x170. All 68 backend cases pass in Debian 13
arm64, including scan reply correlation without exposing the decoded URL;
package/install/policy checks pass. Both CI jobs pass. The exact checksummed CI
package is installed on the development device: ten runtime hashes match, current
settings/keys, saved conversations/drafts and app preferences are preserved, and
radio/API/firewall readback passes. No new physical QR import was performed for
this UI update; camera capture itself is unchanged.

## 0.1.3 — 2026-11-16 (release candidate)

- Group Settings by radio, channels/node, location/Internet, messaging,
  notifications and application setup, keeping Radio first. Stable action keys
  are independent of the display order; group headers are not navigation stops.
- Keep the active conversation/channel in a fixed header above messages. Reserve
  the footer for keyboard help and compose input, including with long peer names.
  Channel identity/classification remains visible while composing and scrolling.
- Support keyboard scrolling through long Settings information screens; About
  links directly to this app's repository.
- Refresh message-action help when selection changes, and show that a changed DM
  channel needs choosing again instead of labeling its replacement as the route.

Validation: native ASAN/UBSAN passes all 26 settings navigation targets, existing
privacy/configuration actions, real-font footer bounds and compose/scroll/list
transitions. Both CI jobs pass, including 67 backend cases and Debian arm64
package/install checks. The exact CI artifact is installed on the development
device; all ten runtime hashes, saved messages, current keys/settings, radio
online and firewall health were verified. No test texts, position requests or traceroutes were sent.
The operator confirmed Radio is first and the channel/header plus footer help
are both visible on the actual device.

## 0.1.2 — 2026-11-15 (release candidate; hardware qualification in progress)

- Replace repeated sudo health probes with a capability-only firewall monitor;
  preserve exact stop causes. A device traceback confirmed timeout cleanup of an
  elevated helper caused EPERM and idle radio shutdowns.
- Normalize the vendor driver's 255-valued GPIO brightness during guardian
  recovery; stranded recovery records no longer prevent subsequent starts.
- Detect occupied LoRa GPIOs before power-on, including the NFC overlay
  reserving GPIO22, and report the required reboot instead of a daemon assertion.
- Handle tuning changes through planned stop/persist/reconnect and readback.
- Add receive-only first-run setup, explicit confirmed TX control, schema/client/
  daemon compatibility checks and dependency diagnostics.
- Bind routing, history, drafts and retries to opaque channel identity; archive
  replaced slots and reject stale selections before RF.
- Add bounded private SQLite conversations/drafts/pins, no replay after restart,
  disk-history opt-out, search and local-time quiet hours.
- Add confirmed channel management, named private local profiles, live node/
  telemetry/GPS ages, acknowledgment-source checks and preserved failure causes.
- Add diagnostics with an explicit safe export whitelist; fix CSI camera capture
  through rpicam-still and report missing decoder/camera failures.
- Add native arm64 Debian 13 build/install/namespace validation and checksummed
  workflow artifacts in CI, using fixed LVGL/action commits.
- Exercise real empty-profile startup; explicitly enable the local console/API
  default, handle firmware's empty disabled slots and compare semantic readback.
- Include explicit channel IDs in identity/import deduplication; preserve draft-only
  and cleared conversations and reject malformed saved peer identities.
- Separate GPS receiver control from sharing, mark stale map locations, and count
  actual remote receive packets with tested parent/child pubsub signatures.
- Re-enabling saved history also commits the current RAM drafts without another
  edit; history-preference changes reach every connected UI.

Validation: 67 backend regressions (one Linux-only case skipped on macOS),
real LVGL keyboard/lifetime/parser/transport checks with ASAN/UBSAN, synthetic
320x170 renders and Debian arm64 package validation. Native arm64 GitHub CI and
downloaded artifact checks passed. Actual device checks passed for private-peer
ACK/reply, CSI QR decoding, cold power/firewall order, empty setup, channel/profile
readback and crash cleanup/durable drafts. The installed radio completed 30m12s
with 119 healthy samples and zero failures/restarts; physical settings/Esc checks
passed. The final storage-only follow-up passed CI and that exact artifact was
installed with runtime hashes, saved data and original settings verified; ADB
was disabled afterward. Outdoor GPS remains deferred
because the operator cannot provide sky view now; see the implementation checklist.

## 0.1.1 — 2026-11-15 (release candidate)

- Support public and private channels throughout startup, imports, group/DM
  messaging, position requests, traceroutes and selected-channel QR sharing.
- Label channels Public/Private, require explicit DM channel selection, and
  refuse missing/disabled selections instead of falling back to another channel.
  Imports support unnamed defaults and preserve existing keys and radio tuning.
- Add confirmed frequency-slot control. Public automatic location requires a
  separate confirmed opt-in; unapproved public location defaults are disabled
  before power-on. Manual precise public location has a Cancel-first confirmation.
- Harden radio lifecycle with a hardware-owning guardian, abrupt-death cleanup,
  dependency-service conflict checks and boot-persistent API firewall confinement.
- Confirm privacy changes through actual admin readback; show Pending/Unknown
  through failures and disconnects, and stop on unconfirmed position writes.
- Correct ACK/retry ordering, history retention/deletion synchronization, partial
  socket writes, malformed JSON handling and frame/queue/work/allocation bounds.
- Isolate app settings with private atomic saves and deliberate migration; align
  package identity/version/assets and resolve actual native library dependencies.
- Include full attribution and third-party licenses, synthetic current screenshots,
  offline CI with a pinned Node 24 checkout action and regression checks. Remove
  the historical personal chat image
  from publication history while preserving a private recovery backup.
- Update the companion app builder guide with reusable lifecycle, routing, privacy,
  transport, storage, packaging, licensing and publication lessons.

Validation: 36 backend regressions on Debian arm64 and the device with fake radio/
sysfs controls; native LVGL keyboard/parser/transport and ASAN/UBSAN checks; Debian
package installation and policy checks. Real private-profile startup/readback,
process-death recovery, firewall refusal and warm-reboot persistence passed. The
operator's private profile is unchanged; no public RF test messages were sent.
See [the verification record](docs/review-remediation.md) for remaining peer,
GPS/camera/navigation, physical IPv6 and cold-power-cycle qualification limits.
