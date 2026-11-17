# Cardputer Zero Mesh

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

## Unified channel setup closeout — 2026-11-17

Candidate 0.1.13 is installed from exact runtime/package source `88fd8e5`; its
complete form, verified saves, preservation cases and qualification limits are
in [`docs/channel-setup-flow.md`](docs/channel-setup-flow.md). All 119 backend
tests and native/arm64/package gates pass. Later closeout changes are docs only.
Preserve settings/profiles/chats, keep ADB enabled and leave Surveyor/EN1 service
ownership intact. One private T-Deck message received a persisted Recipient ACK; original radio,
profile and data preservation are verified. The owner subsequently passed the
specified physical form/`Test/1` entry/Review/Discard check; saved state remains
unchanged. Camera TestQR013 is prepared for the next check; a complete physical
keyboard sweep and public-peer checks remain unperformed.
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
