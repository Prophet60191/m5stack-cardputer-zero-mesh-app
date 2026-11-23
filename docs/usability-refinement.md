# Everyday-use refinement — 0.1.15 candidate

Reviewed 2026-11-22. This pass improves clarity, readability and recovery while
preserving the four home destinations and the radio supervisor boundary.
It does not introduce new radio, sharing or automatic-send behavior.

## Changes

- Clear history, Delete conversation and Delete message require a Cancel-first
  confirmation describing what is lost. There is no Undo. Confirmation retains
  a stable target identity even when new messages arrive; other radios' copies
  are never deleted. Saved setup deletion also requires confirmation.
- Delivery labels are Recipient received, Mesh confirmed and Receipt unconfirmed.
  Details explains the evidence and explicitly distinguishes it from a read
  receipt. Failure reasons remain available before a manual resend.
- Settings describes radio operation as Listen only or Send & receive. The
  compact top bar retains RX/TX/RX to fit the physical display; HELP and the
  manual explain both. Unknown and pending states remain truthful.
- Important setting values use normal text contrast. Secondary text is more
  legible in both themes; outgoing bubbles use readable text/background pairs.
  The chosen home carousel retains its existing colors, geometry and motion.
- Add channel remains one form with Channel, Radio connection - all channels,
  Sharing - new channel, and Save destination groups. Headings cannot receive
  focus. The Save shortcut stays visible; only specific radio/privacy effects
  require acknowledgment. Existing channel data and hidden keys stay private.
- Settings distinguishes Saved mesh setups, Manage channels, Add a channel and
  View channels. Applying a saved setup explicitly leaves Radio and sharing Off;
  start Radio to listen, then enable sending if wanted. This is deliberate and
  does not silently restore transmission or location sharing.
- Search ignores ASCII English-letter capitalization in names, saved text and
  drafts. Other Unicode characters still match exactly; full Unicode linguistic
  case folding is not claimed.
- Settings/option lists wrap, skipping non-interactive form headings. A narrow
  overflow-only position marker leaves space for text in settings, forms,
  conversations and node lists.
- Fresh map peers are filled circles; old/unknown positions are hollow, in
  addition to color differences. This changes as positions age. Diagnostics
  says Unknown instead of exposing negative sentinel values.

## Scope decisions

Retain the current fixed text sizes after contrast and content-priority changes;
this pass is not a complete accessibility qualification. A larger-text mode would
need its own layouts and physical readability study rather than simply enlarging
text into a 320x170 area. No new motion or larger-text setting is claimed.
Keep useful task actions visible and routine navigation in HELP. Preserve direct
light/dark switching. Additional configuration steps are justified only by a
specific consequence; group related fields without another review page.

## Verification

Offline validation passed: four native macOS ASAN/UBSAN CTest targets, including
new deletion/cancellation/live-arrival, saved-setup apply/delete and diagnostic
wording checks; 124 backend tests on Debian arm64 (the macOS run skips one
Linux-only case); three Linux device-target CTest targets; Debian 13 arm64
fresh installation, actual 0.1.8 upgrade, package contents, user-service and
isolated firewall checks. No test starts a real radio.

Synthetic renders cover both themes, grouped form navigation, persistent Save,
message results, deletion prompts and saved-setup preview. Documentation source
checks pass for seven chapters and 30 synthetic screenshots. The manual and
one-page quick reference are regenerated; final visual review and actual-device
installation results are recorded separately below. Synthetic checks cannot establish outdoor GPS, public-peer RF,
physical keyboard, sunlight readability or current device stability. Do not
replace those outstanding checks with old observations.

## Installed candidate and closeout

Runtime/package source: `5f0bc3ee80ff3b8bd1cfa6e496999dac6e8edc29`.
[Native and Debian arm64 CI](https://github.com/Prophet60191/m5stack-cardputer-zero-mesh-app/actions/runs/45092949339)
and the [illustrated-guide build](https://github.com/Prophet60191/m5stack-cardputer-zero-mesh-app/actions/runs/45092949349)
passed. Later wording/layout and closeout changes are documentation only.

Installed the exact CI artifact `cardputerzero-mesh_0.1.15-rev1_arm64.deb`, SHA-256
`33d6d36f3d25e35cc37918dba4841947001914c3491294d18074d6d9ba4ce116`.
All 109 regular files match archive hash, mode and root ownership; dpkg verify
is clean. The Store preflight has zero errors and the existing optional
localization advisory. No Store submission or binary release is implied.

The previous supervisor process predated installation and still reported 0.1.14.
It was replaced through the existing unprivileged service restart; the current
service reports 0.1.15, Radio Off, and NoNewPrivileges. Before the owner's manual
check, saved channel/key/tuning/privacy/identity/profile and INI hashes matched
the private backup, and all conversation/draft/preference rows matched exactly.
Only normal daemon transmit-history bookkeeping differed. Backups stay outside
the public repository. No peer was reconfigured and no RF test was requested.

The exact CI channel-form replay passed on the device as UID1000 with
NoNewPrivileges and installed Noto fonts, in both themes. It uses fake commands
and synthetic data, not the live radio, camera or framebuffer. The owner then
confirmed that the installed app's message presentation, Saved mesh setups,
grouped Add channel form and return navigation look right, and that Cancel in
message deletion preserves the message. The live UI runs as the ordinary user.
ADB and its reconnect helper remain active; temporary installation/replay files
were removed. The radio was Off before device work and no saved TX/privacy
preference was changed by the update.

Updated Markdown/synthetic illustrations and rebuilt the 40-page user manual,
one-page quick reference and companion app-development reading edition. Visual
layout and local source/link checks pass. Existing Drive manuals were refreshed
in place, preserving their links and folder placement. Full outdoor GPS, public
peer, physical keyboard sweep and extended Online radio qualification remain
separate from this refinement; no new results are claimed for them.
