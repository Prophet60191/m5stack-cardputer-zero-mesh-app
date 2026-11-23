# Header presentation closeout — 2026-11-23

Mesh 0.1.18 removes the battery glyph and retains numeric charge percentage when
available. The battery status source, radio ownership, channel/privacy settings,
message storage and phone access are unchanged. The removed icon label, icon
selection helper, unused glyph constants and font load are gone.

## Actual device finding and correction

The first candidate's desktop renders and native tests passed, but isolated
framebuffer capture showed missing left status text on the device. The previous
0.1.17 binary displayed it. Restoring the old font load did not correct it; that
probe was discarded. Invalidating the complete bounded header strip restores the
unchanged text alongside the percentage. Apply that invalidation on radio, node,
unread and battery refreshes, using the existing five-second battery timer.
No hidden placeholder widget, extra timer, root app or full-screen refresh loop.
This is measured redraw behavior, not a claim to have explained every driver
internal or improved battery life.

The final exact installed UI starts as the normal user through its launch wrapper
with exclusive framebuffer ownership. Actual header capture shows radio TX/RX,
region, node count and percentage together, without the glyph. The launcher user
service is restored afterward. An initial probe incorrectly targeted a system
launcher unit; no such unit existed, no service changed, and the corrected probe
used the actual user service. Hardware captures stay private/ignored; public
screenshots are wholly synthetic. Physical device capture uses the existing dark
theme; light/dark synthetic/native fixtures are separate evidence.

## Source, package and checks

Functional/package source: `2913c01b754e895b306abf514a98b3daefcc16f0`.
[Both CI jobs](https://github.com/Prophet60191/m5stack-cardputer-zero-mesh-app/actions/runs/45256156778)
pass: all 167 backend tests, native sanitizer/regression checks and Debian arm64
build/fresh-install/upgrade/firewall/non-publishing Store preflight. The four
local native checks also pass after the correction. No Store submission or
release is created.

Exact installed `cardputerzero-mesh_0.1.18-rev1_arm64.deb` SHA-256:
`b17b59743d3b48e997b60dbabb4fd0d47bfac980740fc453c9e6d6010dac55d4`.
An earlier internal 0.1.18 candidate lacked the redraw correction; version alone
is not evidence that it contains this fix. All **117 installed regular files**
match the final archive bytes, modes and ownership. All **26 original messages**,
three threads and three preferences remain, with no added rows. App/radio/module/
channel/profile/identity/privacy files match the consistent private backup.
Radio returns Online/send-receive, GPS ENABLED, location/MQTT Off, Phone access
On; ADB stays active and the standalone daemon remains masked. The separate
native +czgps1 package is preserved. This UI change does not repeat outdoor GPS,
physical key, cold-power or actual handset qualification.

## Documentation and maintained copies

- Update README/current status, guide text/versions and stale package examples.
  Preserve historically scoped verification records and versioned release assets.
- Refresh five public README screenshots and 21 user-guide illustrations from
  synthetic real LVGL renders. Remaining older illustrations are labeled as
  historical, including their old battery glyph. No private capture is published.
- Validate all seven chapters, 31 images, links/anchors and version markers.
  Render/review all 42 manual pages and the one-page reference. Manual SHA-256:
  `0a2613ec39d4fbb6c86ad4edac10051ed6d5aaabb39c50f019dcf4c1228ba789`;
  one-page reference SHA-256:
  `44ad03be684e65209806ed5e56cd2a1f49c5ac85baf710114bb2f48d860dc892`.
- Refresh the Desktop's current Markdown/images/PDFs, retaining versioned archives.
  Replace the existing Mesh manual in Drive in place; readback confirms its same
  ID/folder and 408058 bytes. No separate reference file existed there to replace.
- In the app builder guide, remove battery from the generic status example,
  record the personal no-icon preference, and add actual-framebuffer sibling/
  bounded-redraw guidance. The 102-page reading edition is reviewed and replaced
  in the same Drive file (633102 bytes); guide/preflight CI passes. Official
  platform component descriptions remain factual, not rewritten as preferences.
- Audit older app/shared-backend mirrors against public source history before
  syncing. No independent code edits found; preserve their independent hardware
  README notes and unrelated files. Back up host/device mirrors privately first.
  Rebase relative protocol-document links and update the hardware work records.
  Only canonical release source is pushed; mirrors are secondary copies.

USB GPS is explicitly deferred. Existing phone reconnect limits, the nonfatal
GPS configuration ACK warning and broader qualification limits remain visible in
[current status](current-status.md); this presentation change does not resolve them.

Final maintenance cleanup removes 57 exact task-created temporary paths after
verifying both private state archives and mirror backups on the host. Installed
files, 20 device backend mirror files and native GPS binary hash are reverified
after cleanup; app/radio/privacy/Phone/firewall readback and launcher/ADB pass.
Private host rollback/data backups remain retained outside Git.
