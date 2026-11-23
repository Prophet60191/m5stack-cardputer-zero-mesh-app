# Current app status — 2026-11-23

Mesh 0.1.18 is installed and qualified for its header change: no battery glyph,
with numeric percentage retained. A real-device partial-redraw regression was
caught and corrected. The [closeout](header-presentation.md) identifies the exact
functional source/package, successful checks and preserved data. Radio, GPS,
channel, privacy, message storage and phone behavior are unchanged.

## Verified foundation

- The installed 0.1.17 foundation passed 167 backend tests, native sanitizers,
  Debian arm64 build/install/upgrade/firewall/Store preflight and exact installed
  file verification. The current 0.1.18 checks and all 117 installed files also pass as linked above.
- Public/private channels, grouped channel setup, saved mesh setups, conversations
  and drafts, search, pins, quiet hours and diagnostics are implemented. Private
  peer delivery and the official iPhone messaging/viewing path are exercised.
- [Fitted GPS qualification](internal-gps-validation.md): app 0.1.17 local refresh
  plus the separate native +czgps1 correction passed eight radio starts and the
  final 31-sample/321.5s outdoor watch. New fixes reach the app with location
  sharing Off. Satellite count remains unknown; older fixes are marked stale.
- [Phone qualification](phone-access-validation.md): actual iPhone messaging,
  cellular connectivity, background return, interruption recovery and physical
  cold-power checks passed on the recorded version. One earlier foreground
  disconnect remains unexplained; radio/access restarts can need manual phone
  reconnection. Newer protocol checks do not repeat every handset test.
- [Maintenance](maintenance-pass.md) records completed cleanup, reduced repeated
  work and measured synthetic performance, not a battery-life claim.

## Deferred and remaining

USB GPS work is explicitly deferred by the owner. SIM-dependent device work is
also deferred. Live public-peer delivery, physical IPv6 and long-term everyday
reliability are not established by private-peer/short checks. The nonfatal GPS
configuration ACK warning is not claimed fixed. No new full-system cold-power
qualification is implied by the latest radio restart tests.

## Distribution and documentation

Markdown in docs/user-guide and its generated manual/reference are maintained
for the current candidate. The existing Drive PDFs are refreshed in place;
GitHub release downloads remain explicitly labeled with their older version.
Generated PDFs/build artifacts stay outside Git. Normal source pushes do not
create a release, submit to the Store or send an upstream contribution.

The radio-service GPS patch is a separate Linux package, not receiver firmware,
and is not installed by the app package. Its exact source/artifact hashes and
private rollback handling are in the GPS record. Preserve ADB enabled, saved
chats/channels/profiles and current send/receive and privacy settings.
