# Changelog

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

Validation: 62 backend regressions (one Linux-only case skipped on macOS),
real LVGL keyboard/lifetime/parser/transport checks with ASAN/UBSAN, synthetic
320x170 renders and Debian arm64 package validation. See the implementation
checklist for pending actual device, peer, GPS, camera and cold-power evidence.

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
