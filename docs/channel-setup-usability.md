# Channel setup usability — 0.1.14

Status: implemented, installed and qualified within the scopes below, 2026-11-17.
The owner approved a complete form with a
persistent Save action and clear scan success, without a repeated review page.

The .13 physical walkthrough confirmed that Return captured the synthetic QR,
that the form showed one imported channel/hidden key, and that its details were
TestQR013 [Private]. The scanned form did not clearly communicate success and
its subsequent review repeated the same information. The owner also confirmed
Test/1 entry, Review/Back retention and that the draft disappears only on Discard.
These are .13 observations, not physical acceptance of the new .14 interface.

## Resulting behavior

- A successful QR/link load is named at the top of the form, with its channel
  classification and hidden-key description. The add method follows the details.
- A fixed `1: Save channel` or `1: Save profile` action remains outside scrolling
  fields. There is no mandatory generic review page or duplicate Save list row.
- Save runs owner-side validation/preparation, then one commit. Specific changes
  to active shared radio tuning, TX/antenna, public location or MQTT require only
  their relevant acknowledgment. Commit flags reflect the accepted choices.
- Invalid/unsupported and stale setups remain blocked; fields/imported data
  survive validation errors. Lost/unknown replies remain explicit; no automatic
  configuration or RF retry. Separate profiles leave the active setup untouched.
- Digit 1 remains text in editors. Busy/finished/sub-picker state cannot turn a
  held shortcut into another save. The existing input adapter consumes one
  application activation per physical hold and ignores raw autorepeat.
- Routine navigation/close hints move to HELP and user guides. Contextual actions
  and useful status remain; unnecessary footer rows are omitted. HELP documents
  the new action, and successful saves name the added/included channel.

The supervisor remains the only radio owner. Its token/fingerprint/cancellation,
validation, readback and recovery rules are preserved. Software cannot reliably
detect antenna attachment, so legitimate TX requires a narrow acknowledgment.
No peer configuration, shared driver/network or backup policy change is part of
the UI cleanup.

## Exact source and verification

- Runtime/package and `v0.1.14-rc.1` source:
  `e8dadd35e1ea44b83f54c2cae778ac8f9a5abc5c`. Later closeout changes are docs only.
- Installed archive: `cardputerzero-mesh_0.1.14-rev1_arm64.deb`, SHA256
  `a1ccf5a51ac066520e10f0f2718c55962e3c5b4cfbb4f1a049d3b95d5b157761`.
- Matching [code/package CI](https://github.com/Prophet60191/m5stack-cardputer-zero-mesh-app/actions/runs/44217619873)
  and [illustrated-guide CI](https://github.com/Prophet60191/m5stack-cardputer-zero-mesh-app/actions/runs/44217619351)
  pass. The 40-page manual/one-page reference were rendered and visually reviewed.
- Reusable form/action/diagnostic lessons: app-guide `c6f8ad0323615dc6447179134c767eeb37d058a8`;
  [22 guide/preflight tests pass](https://github.com/Prophet60191/m5stack-cardputer-zero-app-guide/actions/runs/44218862089).
  No Store or upstream submission is included.

| Scope | Observed result |
|---|---|
| Backend | All 124 tests pass in Linux/arm64 CI; macOS skips one Linux-only case. Five added tests distinguish failed verifier causes and prevent raw-output disclosure. |
| Native/offline | Four macOS and five Linux desktop sanitizer CTests; three Debian arm64 sanitizer CTests. Direct Save, persistent footer bounds, digit ownership, repeated keys, malformed preparation, specific consent, profile-only save, draft/import retention and late/cancelled replies pass. |
| Package | Debian 13 arm64 release build, fresh install, 0.1.8 upgrade, archive/license/service/firewall checks and non-publishing Store preflight pass: zero errors, optional localization advisory only. |
| Installed files/security | All 105 regular files match archive hash/mode/root owner; dpkg verify clean; obsolete Review screenshot absent. Live socket peer UID1000, zero capabilities and NoNewPrivileges. ADB/hotplug remain active/enabled at the owner's request. |
| Device replay | Exact CI keyboard and setup replay pass as the normal user with NoNewPrivileges, official symbol fixture and installed Noto fonts; both themes and 320x170 bounds exercised. |
| Running native UI | Controlled real evdev injection types ui014 (digit 1 remains text), scrolls the form with fixed Save visible, and holds/repeats 1 to save exactly one separate synthetic profile directly. The actual framebuffer shows named completion. Test profile deleted; launcher restored. This is automated device input, not a new physical-key sweep. |
| Preservation | Original one named profile plus active working setup, keys/roles/tuning/TX/GPS/privacy/identity/INI preserved; all 20 messages, three thread rows and one preference row match the private SQLite backup exactly. Only normal daemon transmit-history bookkeeping changed. No RF send was requested. |
| Final stability | Ten minutes, 61 healthy samples, one supervisor PID and zero restarts/failures. Online/RX-only, API connected, firewall healthy; greatest verdict age 2.01s against the unchanged seven-second limit. This is bounded observation, not a guarantee against recurrence. |

Active-setup checks compare the actual fingerprint/tuning/radio/TX/MQTT/slot
capacity; request correlation and remaining profile capacity can change during
a profile-only save. The temporary profile and all owned test executables were
removed after qualification; private backups/evidence are retained outside Git.

No new .14 RF send, peer configuration, physical camera capture or manual key-map
sweep is claimed. .13 owner-confirmed camera/TestQR013 and private T-Deck Recipient
ACK are separately recorded in [the earlier record](channel-setup-flow.md).
Outdoor GPS, USB GPS source/priority/fallback, public-peer RF and physical IPv6
remain unqualified. The UI cleanup does not implement USB GPS.

## Pre-install radio observation

Before installing .14, the .13 supervisor was found in Error after its firewall
monitor reported one unavailable check at 20:13:59 local time and verified again
at 20:14:01. The supervisor stopped at 20:14:00 with no service restart. Old logs
do not establish whether nft timed out, failed, or returned different rules. The
current check is healthy. This is pre-existing .13 evidence, not a .14 regression
or a proved firewall-policy change. .14 adds fixed allowlisted failure reasons
for timeout, command failure, execution/data errors and rule mismatch. It does
not echo raw output, add retry/grace, relax freshness or automatically restart
the stopped radio. Fail-closed shutdown is preserved. The underlying isolated
failure remains unexplained; a new
health/stability run must distinguish diagnostics from a claimed root-cause fix.
