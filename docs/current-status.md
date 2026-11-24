# Current status

**0.1.20 UI checkup is in qualification.** The owner requested a full UI review
on 2026-11-24; [findings and tests](ui-checkup.md) track the fixes. The earlier
pause is superseded for this bounded review; deferred hardware work stays deferred.

**Mesh 0.1.19 is installed**, from exact functional/package source `8ada52a`.
The [distribution qualification](distribution-validation-20261123.md) records
CI, hashes, the 255 installed-file checks, data preservation and current limits.
Later closeout commits update documentation only.

The single installer now includes the qualified GPS-corrected native runtime and
15 locked Python wheels. First launch prepares its private user environment
offline. No separate radio package or pip commands are needed; APT resolves
standard OS libraries. Existing user environments and the masked global daemon
are preserved. See [installation/build/distribution](distribution.md).

## Everyday-use observation — 2026-11-23

The owner has closed the current change cycle and will use the app for a while.
No further feature, cleanup, optimization or deployment work is planned without
a new request or concrete issue. If something recurs, record the screen/action,
expected behavior and actual result before deciding on a fix. This stopping point
does not turn deferred checks into passed checks or authorize publication.

## Qualified behavior

- All 175 backend tests, native sanitizer/regression gates, Debian arm64 package,
  upgrade/firewall, minimal clean-container offline runtime and current pinned
  Store preflight pass. Existing-device launch/header, three radio/GPS starts,
  77.8-second monitoring and a private Recipient ACK pass.
- All 26 original messages, three threads/preferences and saved settings remain;
  one expected test message makes 27 messages. Radio is Online/send-receive,
  fitted GPS enabled, sharing/MQTT Off, selected Phone access On, ADB On.
- [Header qualification](header-presentation.md) explains the 0.1.18 framebuffer
  correction retained here: percentage is visible without a battery glyph.
- [Fitted GPS qualification](internal-gps-validation.md) covers the earlier outdoor
  Map/freshness and native startup diagnosis. The correction is now bundled;
  this update repeats receiver identification, not outdoor/cold-power acceptance.
- [Phone qualification](phone-access-validation.md) covers the earlier official
  iPhone, cellular, recovery and cold-power checks. This update repeats a real SDK
  gateway/heartbeat check, not a handset walkthrough. One earlier awake-screen
  disconnect remains unexplained; do not claim it fixed.
- [Maintenance](maintenance-pass.md) records measured cleanup and synthetic work
  reduction; no battery-life claim follows from it.

## Submission and deferred checks

[Readiness](store-submission-readiness-20261123.md) separates completed code,
dependency, source and CI work from publication. CZMS replaces the taken MESH
Store code; the app name stays the same. Installer/source/checksum/video assets
are prepared for review; a draft does not make them publicly downloadable.
The owner will record the required actual-device video. No Store PR or public
release is authorized by this preparation.

No spare card: fresh-image hardware qualification is explicitly deferred; the
working card was not erased. USB GPS and SIM-dependent work also remain deferred.
Live public-peer delivery, physical IPv6, full physical keyboard sweep and
long-term everyday reliability retain their documented limits. Nonfatal GPS
configuration ACK warnings are not claimed fixed.

## Documentation and maintained copies

Current Markdown, generated manual/reference, Desktop and existing Drive PDFs
are maintained for 0.1.19; old versioned archives stay historical. The app builder
PDF links the public GitHub profile on its cover and includes GPS/distribution
lessons. Source/backend mirrors are audited before synchronization and preserve
independent hardware notes. Generated installers/PDFs stay out of Git. Public
release downloads remain labeled with their actual older version until published.
