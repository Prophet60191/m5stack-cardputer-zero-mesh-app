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

## Phone access candidate — 2026-11-23

0.1.16 adds an opt-in, selected-device PhoneAPI gateway behind private Tailscale
Serve. Read docs/phone-access.md and docs/phone-access-validation.md. Keep the
raw daemon port 47400 loopback-confined; never forward the Unix control socket
or allow phone admin/location/MQTT writes. Preserve single radio ownership,
TX/privacy/channel identity checks and actual-daemon heartbeat behavior.
VPN account/Serve setup is a separate administrator action, not an app privilege
grant. Keep real peer IDs, addresses/auth links and qualification backups private.
Keep offline checks separate from the actual-iPhone results recorded below. Preserve settings/chats and leave ADB enabled.

Phone candidate installed from functional source `8cfa3b6`; all 158 backend tests,
Debian arm64 package gates and exact 114-file installed verification pass. Native
sanitisers pass locally. Private T-Deck recipient ACK and real-daemon heartbeat
checks passed on installed code through protocol fixtures. The owner now confirms actual App Store iPhone connection, node/channel
loading and two-way messaging. Extended lifecycle/roaming acceptance remains
pending; do not conflate protocol fixtures with handset observations. The current
setup is Online/send-receive, Phone access On, and ADB On. Read the qualification
record for remaining checks and source-correlated CI status.

Extended handset qualification (2026-11-23): owner confirms recovery after an
approximately two-minute lock/background interval and successful reconnect/send
after toggling iPhone Wi-Fi. Phone access Off/On preserves the live radio; the
owner needed a manual reconnect afterward, then messaging worked. Normal radio
stop/start also passed on device with saved configuration/profile hashes unchanged;
its handset recovery and the foreground idle check are still pending. Current
checks are logged in docs/phone-access-validation.md; do not infer cellular
handover from the Wi-Fi interruption. Lockon itself does not need a SIM.
