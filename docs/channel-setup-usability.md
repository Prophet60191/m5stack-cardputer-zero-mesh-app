# Channel setup usability — 0.1.14

Status: implementation/local verification complete; exact CI/package/device
qualification pending, 2026-11-17. The owner approved a complete form with a
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
No device/peer/driver/network or backup policy change is part of the UI cleanup.

## Verification to complete

All four local macOS ASAN/UBSAN CTests and the 119-test backend suite pass
(one Linux-only host test is skipped on macOS; matching Linux/package gates are
separate). New native cases exercise direct Save, persistent footer bounds,
no duplicate busy action, digit ownership, malformed preparation, specific
consent, profile-only save, imported-data retention and late/cancelled replies.
All three Debian arm64 sanitizer CTests (including real evdev numeric input)
and all 22 shared-guide tests in Debian pass. The 40-page manual/one-page
reference were rendered and reviewed. Exact-source package/Store preflight and
private-backed installation/device UI qualification remain pending. No new .14
hardware or RF claim is made here. .13 RF/camera evidence remains in
[the earlier record](channel-setup-flow.md).
