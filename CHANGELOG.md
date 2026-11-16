# Changelog

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
  offline CI and regression checks. Remove the historical personal chat image
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
