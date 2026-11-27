# Current status

**Mesh 0.1.22 is the tested Messages/loading candidate.** The pre-update
installed package is 0.1.21-rev2 with the shared home carousel. This change
preserves that layout and adds two conversation tabs, recency ordering, combined
message search and bounded rendering. See [Messages navigation and loading](messages-navigation.md)
for measurements and final device qualification. The previous [UI checkup](ui-checkup.md)
remains historical evidence for the 0.1.20 fixes retained here.

The earlier [0.1.19 distribution qualification](distribution-validation-20261123.md)
remains evidence for the unchanged bundled native/Python runtime and radio/GPS/
phone behavior; those RF checks were not repeated for a UI-only update.

The single installer now includes the qualified GPS-corrected native runtime and
15 locked Python wheels. First launch prepares its private user environment
offline. No separate radio package or pip commands are needed; APT resolves
standard OS libraries. Existing user environments and the masked global daemon
are preserved. See [installation/build/distribution](distribution.md).

## Everyday-use observation

The owner requested this bounded UI checkup after the previous pause. With the
fixes qualified, return to ordinary use and concrete issue reports rather than
speculative feature/optimization work. Keep deferred checks explicit.

## Qualified behavior

- For 0.1.19, all 175 backend tests, native sanitizer/regression gates, Debian arm64 package,
  upgrade/firewall, minimal clean-container offline runtime and current pinned
  Store preflight pass. Existing-device launch/header, three radio/GPS starts,
  77.8-second monitoring and a private Recipient ACK pass.
- The 0.1.20 upgrade preserves all 27 messages, three threads, three preferences
  and saved radio/profile/app settings. All 259 installed files match the tested
  package. The Cap is disconnected; **Radio stays Off**. Saved send/receive,
  GPS/privacy and Phone choices are unchanged; ADB stays On.
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

Current Markdown and the 42-page manual/one-page reference are refreshed for
0.1.20, including the existing Drive manual. The former Desktop guide folder is
absent at this check and was not recreated; old versioned archives stay historical. The app builder
PDF links the public GitHub profile on its cover and includes GPS/distribution
lessons. Source/backend mirrors are audited before synchronization and preserve
independent hardware notes. Generated installers/PDFs stay out of Git. Public
release downloads remain labeled with their actual older version until published.
