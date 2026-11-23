# Changelog

## 0.1.14 — 2026-11-17 (release candidate)

- Make the channel form its own review, with a fixed 1: Save channel/profile action
  available at every scroll position. Validate privately before one commit; keep
  only specific radio-wide, antenna/TX, public-location and MQTT consent.
- Show named QR/link success first and the named saved result afterward. Keep
  numeric typing normal, prevent duplicate busy actions, preserve local drafts
  and reject stale, malformed or unsupported saves.
- Move routine navigation directions to HELP/user guides. Keep contextual action
  footers and status; remove redundant motion/back/select/close reminders.
- Distinguish firewall verifier failures with fixed, non-sensitive reason labels;
  retain immediate fail-closed behavior. A pre-install .13 check failure recovered
  two seconds later; its exact cause is not recoverable from the old logs.
- Record owner-confirmed .13 physical scan/correct TestQR013 details and the
  confusing success/form/review transition that motivated this change.

Validation: all 124 backend tests, native macOS/Linux and arm64 sanitizer gates,
Debian arm64 build/fresh install/upgrade/firewall and Store preflight pass on
runtime source `e8dadd3`. Exact package installed: all 105 file hashes/modes/owners
match; device keyboard/setup replay and controlled native UI numeric/held Save
pass. Original saved data/settings/profile remain intact; temporary profile is
removed. Final ten-minute RX: 61 healthy samples, zero failures/restarts, maximum
firewall age 2.01s. No new RF send or physical camera/key-map result is claimed;
old .13 firewall failure remains unexplained. Updated guide/PDFs and own .14
candidate accompany this update; no Store/upstream submission.

See [the usability qualification record](docs/channel-setup-usability.md).

## 2026-11-17 — Physical form/discard confirmation

- Owner completed the specified field scrolling, Test/1 name entry, Review/Back
  retention and Esc/Discard check. The test name disappeared only on discard,
  which is the intended unsaved-draft behavior.
- Read-only device verification confirms original radio/channel/profile/identity
  and privacy files remain unchanged, Online/RX, with all 20 messages retained.
- Prepare a harmless synthetic TestQR013 for the next physical camera check.
  No full keyboard sweep or new camera result is claimed; no runtime change.

## 2026-11-17 — Private-peer qualification follow-up

- With owner-confirmed antenna/T-Deck readiness, one private directed test
  message received and persisted Recipient ACK (zero reported intermediate hops).
- Temporarily disable the public secondary and use the known private tuning;
  restore the original receive-only setup/public channel afterward. Preserve all
  19 original messages, three thread rows, profile/settings/INI; the delivered
  test message is the expected twentieth. No peer configuration change.
- Defer physical form/key/camera acceptance at the owner's request. Clarify the
  inventory: one persisted named profile plus an active working setup; earlier
  "both saved profiles" wording conflated those. No runtime code/package change.

## 0.1.13 — 2026-11-17 (release candidate)

- Replace separate add/import/create dialogs with one local channel setup form:
  scan/paste/public/private, hidden key handling, global tuning, privacy, optional
  named profile, complete review and direct conversation opening. Radio remains
  first in Settings; existing slot-edit actions stay available.
- Prepare while Off; explicitly adopt supported QR tuning, preserve calibration
  and show existing fixed-frequency overrides. Profile-only saves leave the
  active setup untouched. Active saves verify persisted/live state and attempt
  restoring the previous setup on failure; partial/uncertain results are explicit.
- Bind/cancel/expire preview requests, reject stale setups and require explicit
  global tuning, antenna, public location and MQTT acknowledgments. No automatic
  transmissions or configuration replay. Preserve local entries on validation
  errors, Escape and review return, with fixed keyboard help and in-place selection.
- Keep the older socket import contract compatible; remove obsolete standalone
  UI branches. Add native and backend setup/cancellation/privacy/recovery tests
  and update the user guide with synthetic screenshots.

Validation: all 119 backend tests, native/arm64 sanitizers, exact-source CI and
Debian package/install/upgrade/firewall/preflight checks pass. The verified CI
package is installed; all 104 files match, owner/API and native UI replay pass,
and existing channels/profiles/INI and all 19 messages/3 thread rows are preserved.
Final ten-minute receive-only test: 61 healthy samples, zero failures/restarts.
Initial closeout excluded RF delivery; the private-peer follow-up above adds
recipient-radio ACK evidence. Physical-key/camera acceptance remains deferred. See
[the implementation/qualification record](docs/channel-setup-flow.md).


## 0.1.12 — 2026-11-17 (release candidate)

- Protect pending drafts/pins through rejected writes, stale replies and
  reconnect snapshots. Add correlated local save acceptance, including no-op
  saves, distinguishing durable storage from RAM-only history opt-out.
- Preserve selected node/message identities through reorder and retention;
  clear vanished targets safely. Update bubble/node selection styles in place.
- Refresh RX/TX mode independently of Online status and show unknown mode
  explicitly. Refresh open node details and GPS age/freshness without new RF
  events, retaining detail identity/scroll and object-bound observer cleanup.
- Attribute historical message details to stored channel identity and privacy,
  with an explicit unknown legacy fallback when identity is missing.
- Suppress unchanged node revisions, update Settings values in place, trim all
  threads in one pass, and remove unused network status polling. Reuse typed
  conversation rows and shared pure geographic formatting/calculation helpers.
- Add offline source/UI/storage regressions and bounded synthetic retention
  measurements. Preserve current features, radio policy, settings migration,
  attribution and the modular UI/supervisor boundary. GPS dongle support remains
  a separate later task.

Validation: all 102 backend tests, native/arm64 sanitizer and package/install/
upgrade/firewall gates, exact-source CI and Store preflight pass. The verified CI
package is installed; all 98 files match, saved settings/profiles/chats survive,
and the owner confirmed navigation/red borders/Esc. Ten-minute receive-only
qualification: 61 healthy samples, no failures/restarts. See the maintenance
record for measurements and limits; outdoor GPS/public-peer/IPv6 remain deferred.

## 2026-11-16 — Deferred second maintenance assessment

- Record seven pending reliability and optimization items, source references,
  failure scenarios and proposed validation in
  [the maintenance backlog](docs/maintenance-pass.md#deferred-second-pass--2026-11-16).
- Prioritize draft synchronization, stable selection, live status/freshness and
  historical channel labels before reducing redraws, retention scans and unused
  network polling. Document measured, feature-preserving simplification goals.
- Defer implementation until the separate Wireless Surveyor work has stopped
  and device access is coordinated; add the resume pointer to `AGENTS.md`.
- Complete a follow-up source/test review: document the unchanged-save
  acknowledgment constraint, five ordered change batches, existing coverage
  versus missing acceptance cases, measurement methods and concrete small
  simplification candidates. Preserve the published protocol until implementation.

Documentation only. No runtime code, app version, shared guide, Surveyor files
or device settings changed. Findings were traced in source and existing tests;
no new runtime tests, builds, benchmarks or hardware checks were performed.

## 0.1.11 — 2026-11-16 (release candidate)

- Bound native input work across each LVGL poll chain to 128 raw read attempts;
  preserve unread input for the next poll, including under modifier/repeat floods.
- Tie pending holds to their input owner. Cancel owned holds on native teardown,
  loss/disconnect and SDL cleanup without synthesizing a short action or clearing
  an unrelated device's hold/release state.
- Add real-reader/editor regressions for flood/resume, normal text spanning the
  budget, stale holds, multiple devices and generic adapter cleanup.
- Document the modular UI/supervisor decision: purpose, background lifetime,
  explicit authority, independent testing, costs, official compatibility and limits.

Validation: native ASAN/UBSAN, the Debian 13 arm64 real-reader sanitizer cases,
package/fresh-install/upgrade/firewall checks and all 98 backend tests pass locally.
Running-code/device verification and exact-source CI are recorded separately.

## 0.1.10 — 2026-11-16 (release candidate)

- Apply saved message-retention preferences silently on startup and reconnect,
  while keeping confirmation for deliberate changes to "Keep per chat".
- Acknowledge silent synchronization without a notice; still enforce history
  trimming/broadcasts and report validation errors. Preserve default replies for
  clients that omit the new optional boolean.
- Add backend and native regressions for quiet startup/reconnect, explicit
  confirmation, actual retention enforcement and visible validation failures.

Validation: native ASAN/UBSAN checks, all 98 backend cases and the Debian 13
arm64 package/install/upgrade, real-reader and firewall/policy gate pass locally.

## 0.1.9 — 2026-11-16 (release candidate)

- Correct Store categories/share code and add a separate 256px Store icon with
  vector source, preserving the visible app identity and runtime icon.
- Rename the launcher wrapper to the package prefix and align its desktop entry.
  Use the Debian version/revision in the installer filename.
- Replace the static user-unit installation with a fixed transient user-service
  controller. Preserve NoNewPrivileges, umask, cgroup termination, failure restart
  and guardian stop recovery. Serialize lifecycle actions; reject root/unsafe
  locks and leave an active supervisor alone on repeated UI launches.
- Add actual 0.1.8 package-upgrade checks and pinned official Store preflight to
  arm64 CI. Record corrected static/transient cleanup-argument behavior and keep
  hardware evidence distinct from package admission.

Native sanitizer checks and all 95 backend cases pass locally, including nine
new service regressions. The actual-device dummy-service probe passes without
touching the existing supervisor. Package/CI and installed-runtime verification
are recorded separately in the release evidence and [readiness record](docs/store-readiness.md).
Local Debian 13 arm64 fresh-install/0.1.8-upgrade, real-reader, firewall/policy
checks and all 95 backend cases pass; Store preflight reports no blocking errors.

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

## 2026-11-18 — Deferred official keyboard-alignment follow-up

- Log the owner's requested future review in docs/keyboard-alignment-follow-up.md
  and project orientation/work notes. Compare current official guidance with app
  behavior; document and justify any adaptation or intentional departure.
- Pending work only: no per-app alignment conclusion, implementation change,
  test execution, device operation or migration is claimed by this log.

## 0.1.15 candidate — 2026-11-22 — Everyday-use refinement

- Add Cancel-first irreversible deletion prompts for history, chats, individual
  messages and saved setups, retaining stable targets through live updates.
- Explain radio modes and delivery results in ordinary language; distinguish
  mesh acknowledgment, recipient receipt and read receipts in message details.
- Improve value/secondary text contrast and outgoing bubbles in both themes;
  preserve the chosen home carousel appearance and motion.
- Group the existing channel form, skip headings during navigation, keep Save
  visible and identify whole-radio and public/internet sharing consequences.
- Clarify saved setups versus channel management/viewing; explain applying a
  setup leaves Radio/sharing Off. Keep all transmission/privacy safeguards.
- Ignore English-letter capitalization in conversation search; add hollow/filled
  map freshness cues and human-readable unknown diagnostic values.
- Add overflow-only list markers, bound long setting values, wrap Settings/option
  navigation, and explain background radio behavior when exiting.
- Update illustrated user guides, synthetic screenshots, quick reference and
  reusable app-guide interaction lessons. See docs/usability-refinement.md for
  exact validation scope and device qualification status.

## 2026-11-22 — 0.1.15 installed refinement closeout

- Install the exact green-CI artifact from runtime source 5f0bc3e; verify all
  109 regular files against the archive and preserve settings and saved data.
- Replace the pre-upgrade running supervisor through its ordinary-user lifecycle;
  confirm 0.1.15/Radio Off, retain TX/privacy settings and leave ADB enabled.
- Pass the synthetic channel-form replay on device in both themes; owner confirms
  message presentation, saved setup/form navigation and Cancel preserving a message.
- Refresh the existing Drive PDFs in place, regenerate the quick reference and
  polish manual wording/layout. Record exact evidence and remaining hardware
  limits in docs/usability-refinement.md. Later edits are documentation only.

## 0.1.16 candidate — 2026-11-23 — Private phone messaging access

- Add an optional protocol-aware gateway for the official Meshtastic phone app,
  behind separately configured Tailscale Serve with PROXY-v2 selected-device
  identity. Preserve the sole meshtasticd connection and its loopback firewall.
- Add Settings → Phone access for selecting a VPN device, toggling access and
  viewing the connection endpoint. Keep radio/channel/privacy changes local;
  no automatic RF startup, transmission or location/MQTT enablement.
- Share message recording/delivery validation between local and phone sends,
  preserve native packet IDs and block duplicate/stale/unauthorized operations.
- Support Apple two-phase snapshots and actual-daemon heartbeats, buffer live
  packets during handshakes, redact host secrets and isolate bounded slow/bad
  clients from radio lifecycle failures. Update user/security/protocol guides.
- Installed qualification is recorded in docs/phone-access-validation.md. Actual
  iPhone acceptance remains pending, distinct from device/protocol/RF checks.
- Offline checkpoint: all 151 backend tests pass, plus native sanitizer/UI
  assertions and the full synthetic screenshot run. Restore the surviving screen
  after a temporary theme capture in the test fixture. Debian arm64 package,
  install/upgrade and firewall gates passed before final CI qualification.

- Native-client compatibility follow-up: preserve the iPhone's Retry behavior
  for matching failed messages after a cooldown, while suppressing pending/
  completed duplicates and rejecting changed targets/payloads. All 153 backend
  tests pass; two added cases cover retry identity and channel changes.
- Strict-boundary follow-up: canonicalize forwarded heartbeat fields and discard
  unqualified future fields from exported snapshots/live records. All 156 backend
  tests pass, including unknown-field command/export regression cases.
- Minimum-export/recovery follow-up: omit inactive channel credentials and retry
  an absent gateway after delayed VPN startup without restarting RF. Healthy
  listeners do not poll. All 158 backend tests pass.

- Installed closeout: source `8cfa3b6`, exact CI arm64 artifact and all 114 installed
  file hashes/modes/owners verified. Original settings/profile/chats preserved;
  private T-Deck recipient ACK and eight actual-daemon heartbeats pass on the
  final gateway. Radio remains Online/send-receive, selected-phone access On,
  ADB On. Updated guide and existing Drive PDF; actual iPhone checks pending.

## 2026-11-23 — Actual iPhone acceptance follow-up

- Owner confirms official App Store Meshtastic connection, loaded nodes/channels
  and successful sending/receiving. Installed-source and documentation-closeout
  CI runs both completed successfully. Extended background/reconnection/lifecycle
  checks are in progress over Wi-Fi; cellular roaming remains unqualified.
- No SIM is required in Lockon for Wi-Fi/Tailscale phone access. Keep the existing
  private setup, send/receive mode and ADB enabled during qualification.

- Extended handset checks: owner confirms foreground recovery after phone lock
  and reconnect/message delivery after a Wi-Fi interruption. Access Off/On
  preserves RF and restores the listener; owner needed a manual reconnect, then
  messaging worked. Device radio stop/start preserves saved configuration and
  profiles; actual handset recovery and longer foreground idle remain pending.
  No production code/configuration changes; leave ADB and send/receive enabled.

- Radio-restart handset follow-up: owner needed a manual reconnect, then the
  private message arrived. Document that limitation in setup/troubleshooting and
  the user guide; rebuild/visually check the PDF and refresh the existing Drive
  manual in place. Five-minute connected idle observation is in progress.

- First idle attempt was interrupted by owner-confirmed phone lock/app switching;
  RF/firewall stayed healthy. Repeat awake-screen idle qualification separately;
  do not count the interrupted attempt as foreground success or failure.

- Owner reports a disconnect with Meshtastic visible and the screen awake: mark
  foreground reliability unqualified and investigate. The start-bounded status
  monitor did not catch that connection; collect synchronized, payload-free TCP
  header/status evidence before assigning a cause or changing production code.

- Controlled follow-up: owner confirms five minutes connected with the screen
  awake; synchronized header trace records at least 317.2 seconds without FIN/
  reset, and 31 status samples remain connected/Online/send-receive with healthy
  firewall and one radio connection. The prior reported foreground drop was not
  reproduced and remains unexplained; no production fix or full qualification
  is claimed. Post-idle message/display checks remain pending.

- Owner confirms post-idle private message/reply delivery in both directions and
  both messages displayed on Lockon. Cellular-path testing is now in progress
  using the iPhone's mobile data; Lockon stays on Wi-Fi without a SIM.

- Cellular path passes: owner confirms private messages/replies over iPhone
  mobile data with Wi-Fi off, connection staying up, while Lockon remains on
  Wi-Fi. Prepare a hash-only settings/selected-phone baseline for cold-power
  recovery. No channel, TX, VPN or automatic-start preferences changed.

- Cold-power platform recovery passes before opening Mesh: owner completed a
  physical power cycle; VPN/Serve/ADB and saved settings/profiles/phone selection
  survived without reauthentication. Transient supervisor absent at launcher and
  automatic radio start Off are expected. Await owner post-boot radio/phone RF
  exchange before marking the full cold-boot test passed.

- Full cold-power acceptance passes: owner opened Mesh/started Radio and confirmed
  two-way private phone/T-Deck messaging without new pairing or VPN sign-in. Final
  readback is Online/send-receive, Phone access connected, firewall healthy and
  one physical radio connection; settings/profile/selected-phone hashes match
  the pre-cycle baseline. ADB remains active, automatic RF startup stays Off.
- Close the planned short handset checks with an acceptance table and remaining
  limits. Preserve the unexplained earlier awake-screen disconnect as follow-up;
  the successful traced repeat is not a claimed repair. No runtime changes.

## 2026-11-23 — Internal/Cap GPS qualification started

- Owner chose outdoor testing of the fitted Cap GPS before USB dongle work.
  Initial diagnostics: GPS NOT_PRESENT, no valid/fresh coordinates. Normal
  config.set/readback enabled the receiver while preserving send/receive and
  location/MQTT sharing Off. No position-send intent or sharing enablement.
- Verify SSH identity against USB-read host key for outdoor monitoring; prepare
  reduced, location-free five-minute diagnostics. Await owner outdoor placement
  before claiming a test interval or fix. See docs/internal-gps-validation.md.

## 0.1.17 candidate — 2026-11-23 — Private local GPS refresh

- Outdoor hardware test confirms the Cap receiver works: 59 fix samples and up
  to 27 satellites used in a bounded direct read, zero checksum errors. The app
  remained without a fix during its 321.8-second watch because it read cached
  client node data while the daemon held fresh GPS coordinates.
- Refresh node-only snapshots over the existing owner API connection while GPS
  is enabled. Preserve RF/location privacy, TX mode, channels and SDK caches;
  bound requests, timeout/backoff and stale-owner completion. Keep satellite
  count unknown where the daemon's snapshot omits it. Add real-SDK, failure,
  lifecycle and privacy tests and user/protocol qualification notes.
- Package/source-correlated installation and outdoor map/freshness retest are
  pending. USB GPS remains deferred until the fitted receiver path is qualified.
