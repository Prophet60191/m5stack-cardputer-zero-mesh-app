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
