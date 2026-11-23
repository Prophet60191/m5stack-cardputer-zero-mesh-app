# ATGM identification timing compatibility patch

This is a small downstream Meshtastic **Linux daemon** correction, not Cardputer
firmware or a GPS receiver firmware update. No upstream submission is implied.
The UI and radio remain separate processes. From Mesh 0.1.19, the app package
bundles a private corrected runtime; the standalone-package path below is retained
for historical 0.1.18 rollback/qualification. See [distribution](../../docs/distribution.md).

The Cap's ATGM336H-6N/AT6668 answered eight read-only hardware identity queries
in 98, 613, 977, 771, 771, 770, 771 and 771 ms (rounded). The qualified native
source waits only 500 ms for that model. Its probe first disables NMEA output;
failed identification can consequently leave an enabled receiver with no fixes.
The patch allows 1500 ms for this model family, covering a 1 Hz reply cycle plus
margin. It does not change baud rate, constellations, receiver firmware, RF,
privacy, channel configuration, GPS sampling interval or the other probe families.

Source: [official firmware at the qualified commit](https://github.com/meshtastic/firmware/tree/54e0d8d0ab2ff56b3a9ce967e53f79e49af560fb).
The build follows its native-tft Debian target. The tiny patch is GPL-3.0-or-later;
retain upstream licenses/attribution and provide corresponding source with any
redistributed native executable. Do not describe a rebuilt binary as byte-identical
to the original distribution: compiler/dependency versions and hashes are recorded.

## Build and test

Use Linux arm64 Docker, or the `Native GPS compatibility patch` workflow. The
workflow fetches the exact official commit/protobuf submodule, applies only the
patch, compiles and produces the native executable **with a corresponding-source
archive**, build metadata and checksums. It does not install anything on a device,
publish a release, start hardware or transmit RF.

For a local build, clone/check out the pinned commit and its protobuf submodule
in a scratch directory. Build this directory's Dockerfile. Mount the clean
checkout at `/work`, this support directory read-only at `/support`, a persistent
PlatformIO core directory at `/root/.platformio`, and an output directory at
`/out`, then run `sh /support/build.sh`. Do not put private runtime state into the
source directory. The source exporter excludes private Git configuration/remotes/hooks/reflogs,
object build output and caches, and includes resolved dependency sources/licenses.
It retains only public upstream Git HEAD/objects/refs/shallow metadata for the
compiled version banner.

`test_probe.py SOURCE` compiles the **actual upstream response-parser body** with
synthetic delayed input under ASan/UBSan: the old deadline accepts one of eight
measured timings; the correction accepts eight. Over-late, unrelated and overlong
responses remain bounded. `test_daemon.py BINARY` separately checks the real
rebuilt daemon and unmodified SDK against a complete synthetic private RX setup,
including node-only refresh without duplicated channels. Run that test only with
`--network none`, `CZ_MESH_ISOLATED_TEST=1`, and no hardware device mounts.
The full simulator smoke test does not claim a physical GNSS acquisition.

To rebuild from the source archive, restore its `build-dependencies/packages` and
`platforms` under the PlatformIO core directory, use the included Dockerfile/tools,
and build `firmware` with `pio run -e native-tft`. The current archive retains
minimal public Git metadata for the version banner; older archives require
reconstructing it from the recorded public commit, or omit that banner suffix.

## Debian package and installation boundary

`package.py --base-deb ORIGINAL.deb --binary meshtasticd --output PATCHED.deb`
accepts only the qualified arm64 baseline version and AArch64 ELF. Supply an
original verified package. If that old repository artifact is unavailable, the
qualification used a **reconstructed** rollback package made only from installed
payload/conffiles verified against all original dpkg checksums; it is not claimed
to be the original `.deb` bytes. Never repack modified site configuration or
runtime profiles/chats/coordinates into a distributable package.

The helper replaces only `/usr/bin/meshtasticd`, adds two provenance documents,
derives ELF library requirements, updates package version/size/checksums and
retains original service/maintainer/config/web assets. The Debian version gains
`+czgps1`; the compiled banner still names upstream `2.7.26.54e0d8d`. Use package
version and executable hash to identify the patched installation.

Before installing: retain rollback and private state backups; check payload
comparison, dependencies, fresh/upgrade gates and administrator service-mask
preservation in a network-isolated container. Stop Radio normally, verify the
standalone `meshtasticd.service` stays masked, install the exact artifact and
resume through the ordinary unprivileged supervisor. Do not grant new sudo
rights, enable the standalone service, alter a shared tunnel or change radio/
privacy defaults. Verify state preservation and repeated real receiver starts.

The daemon is a shared package: source changes are narrowly scoped, but a rebuild
still needs compatibility/RF qualification. A future upstream package upgrade may
replace the downstream correction; recheck the probe rather than silently pinning
or blocking unrelated security updates. This workflow intentionally uploads the
binary and complete source together; keep them paired if redistributing.
