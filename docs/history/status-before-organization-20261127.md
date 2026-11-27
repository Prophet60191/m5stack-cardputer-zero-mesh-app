# Historical handoff snapshot — 2026-11-27

Superseded snapshot of `docs/current-status.md` before documentation organization, including the
completed Mesh session wrap. Read the [maintained file](../current-status.md)
for current guidance. Relative Markdown links below are rebased to preserve their
original destination; original bytes are retained in the private organization backup.

---

# History/background reliability installed — 2026-11-27

Mesh **0.1.26 is installed** from functional/package source `91308d6`.
[The history/background review](../history-reliability-20261127.md) records storage
failure visibility, transaction rollback, duplicate handling and bounded SDK
callbacks. All 268 package files and original saved data verify. Local gates pass
215 backend tests, seven host sanitizer suites and six arm64/package suites;
three actual config readbacks and a two-minute healthy watch pass. Radio remains
Online/send-receive, ADB On. Both exact-source CI jobs and user-guide CI pass; the subsequent 85ede1b
documentation snapshot also passes both jobs. See HANDOFF.md for session closeout.
Older sections remain historical.

# Reliability follow-up installed — 2026-11-27

Mesh **0.1.25 is installed** from runtime/package source `31d3b43`. It fixes
traceroute results/lifecycle/capacity, empty I2C-read recovery and unsafe node
signal conversion. All 267 packaged files and original saved data verify.
Local checks pass 199 backend tests, seven host sanitizer suites and six arm64
native suites plus package/install/upgrade/firewall checks. See
[the qualification record](../reliability-followup-20261127.md) for exact artifact,
CI/live-check status and scope. Earlier sections remain historical.

# Current radio startup update — 2026-11-27

Mesh **0.1.24 is installed** from runtime/package source `66fd215`.
[The startup recovery record](../radio-startup-20261127.md) documents the NFC pin
conflict, unresponsive Cap recovery, local API readiness fix and exact artifact.
Twelve full Cap starts and the following ten-minute radio/API/firewall watch pass;
all original data/settings remain. Radio is Online/send-receive and ADB On.
This supersedes the earlier keyboard-only status; older sections stay historical.

# Current keyboard update — 2026-11-27

Mesh 0.1.23 is installed from runtime/package source `25ff876`.
Left/Right selects Channels/Direct Messages. Normal-user system keys and
Shift+Tab are implemented. [Keyboard qualification](../keyboard-audit-20261127.md)
records all offline and installed checks and their physical-key limits.
Saved chats/settings are preserved; Radio Off and ADB On. Earlier source/status
entries below are historical. USB GPS/SIM and earlier unrelated limits stay deferred.

# Current status

**Mesh 0.1.22 is installed**, from exact runtime/package source `15092a8`.
[Messages navigation and loading](../messages-navigation.md) records the two tabs,
combined search, recency ordering, preserved shared home carousel, package
identity and qualification. All 265 installed files and original chats/settings
are verified. Long-history rendering is substantially faster; an intermittent
launch delay has not been fully explained (measured about 1 second warm and
2.7 seconds with supervisor startup). Later closeout commits are records only.

The earlier [0.1.19 distribution qualification](../distribution-validation-20261123.md)
remains evidence for the unchanged bundled native/Python runtime and radio/GPS/
phone behavior; those RF checks were not repeated for a UI-only update.

The single installer now includes the qualified GPS-corrected native runtime and
15 locked Python wheels. First launch prepares its private user environment
offline. No separate radio package or pip commands are needed; APT resolves
standard OS libraries. Existing user environments and the masked global daemon
are preserved. See [installation/build/distribution](../distribution.md).

## Everyday-use observation

The owner requested the Messages/loading update after ordinary use. Check the
two tabs and combined search physically, then report any remaining slow launch
with approximate duration and whether the app was already used since boot.
Keep earlier deferred checks explicit.

## Qualified behavior

- For 0.1.19, all 175 backend tests, native sanitizer/regression gates, Debian arm64 package,
  upgrade/firewall, minimal clean-container offline runtime and current pinned
  Store preflight pass. Existing-device launch/header, three radio/GPS starts,
  77.8-second monitoring and a private Recipient ACK pass.
- The 0.1.20 upgrade preserves all 27 messages, three threads, three preferences
  and saved radio/profile/app settings. All 259 installed files match the tested
  package. The Cap is disconnected; **Radio stays Off**. Saved send/receive,
  GPS/privacy and Phone choices are unchanged; ADB stays On.
- [Header qualification](../header-presentation.md) explains the 0.1.18 framebuffer
  correction retained here: percentage is visible without a battery glyph.
- [Fitted GPS qualification](../internal-gps-validation.md) covers the earlier outdoor
  Map/freshness and native startup diagnosis. The correction is now bundled;
  this update repeats receiver identification, not outdoor/cold-power acceptance.
- [Phone qualification](../phone-access-validation.md) covers the earlier official
  iPhone, cellular, recovery and cold-power checks. This update repeats a real SDK
  gateway/heartbeat check, not a handset walkthrough. One earlier awake-screen
  disconnect remains unexplained; do not claim it fixed.
- [Maintenance](../maintenance-pass.md) records measured cleanup and synthetic work
  reduction; no battery-life claim follows from it.

## Submission and deferred checks

[Readiness](../store-submission-readiness-20261123.md) separates completed code,
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
