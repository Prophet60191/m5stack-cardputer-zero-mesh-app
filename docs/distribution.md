# Dependency bundles, source builds and release preparation

For device installation, upgrades, backup, recovery and removal use
[the installation runbook](install.md). This page is for developers and release
maintainers. The qualified installed 0.1.19 artifact remains the exact
[recorded CI build](distribution-validation-20261123.md); documentation updates
or a new local same-version build do not replace that qualification.

## What is supplied

| Layer | Supplied by | Update/ownership policy |
|---|---|---|
| Native UI, supervisor/guardian, fixed service helpers | Mesh `.deb` | Root-owned installed files; runtime runs as the logged-in user |
| GPS-corrected native meshtasticd | Private `runtime/radio/meshtasticd` | Fixed source/patch/binary hashes; never PATH or a competing system daemon |
| Client 2.7.11 and dependencies, QR decoder | 15 original hash-pinned Python wheels | Offline per-user managed environment; old operator venv untouched |
| Python 3.13, venv support, native shared libraries, Pillow/zbar and fonts | Debian package dependencies | APT supplies missing supported OS packages; Python is Pre-Depends for preinst |
| CSI capture provider, optional OpenCV fallback | Device image/operator | `rpicam-still` (`rpicam-apps-core`), optional `python3-opencv` |
| Optional phone VPN/app | Operator and official phone app | Tailscale/account/Serve setup stays separate; no credentials are redistributed |

Wheels and source hashes/URLs are pinned in `packaging/runtime/python-lock.json`.
The supported target is Debian 13 arm64/Python 3.13. OS/build toolchain updates
still need their own maintenance; bundling is not an exemption from security
updates or compatibility checks. It does not create an OS image or USB GPS support.

Installed root-owned runtime files live under `/usr/lib/cardputerzero-mesh/runtime/`:
`manifest.json`, `requirements.lock`, `python-lock.json`, `radio/`, `wheels/` and
original `licenses/`. The root manifest/native/wheels are checked for ownership,
unsafe modes/symlinks and checksums. User environment preparation ignores inherited
pip index/config overrides, uses hashes and `--no-index --no-deps`, takes a bounded
lock and repairs only its fixed incomplete/invalid managed cache. The cache path is
`${XDG_DATA_HOME:-$HOME/.local/share}/cardputerzero-mesh/managed-runtimes/<python_runtime_id>/`.
Python identity is separate from native/UI identity, allowing reuse across those
updates. This cache is not a sandbox against its own user. Saved radio state,
history and app INI are separate; see [backup locations](install.md#backup-before-upgrade-or-rollback).

## Build a complete package from a clone

Host prerequisites: Python 3, Git and a running Docker installation capable of
Linux arm64 containers. Docker Desktop on Apple Silicon or a native arm64 Linux
Docker host is suitable; another host needs working arm64 emulation. Run the
wrapper on macOS/Linux; native Windows is not validated. A host C++
toolchain is not required for this container route. Use a writable checkout and
adequate space for native dependency sources, images, caches and large source
archives. No Cardputer, USB device or antenna is needed for these offline tests.

```sh
git clone https://github.com/Prophet60191/m5stack-cardputer-zero-mesh-app.git
cd m5stack-cardputer-zero-mesh-app
docker info
python3 scripts/build-package.py --help
python3 scripts/build-package.py
```

Run from a stable checkout; do not edit/commit/switch it during a build. For
redistribution start with clean committed source. A dirty developer build can
produce a test installer, but the wrapper does not export its release source set.
The source exporter independently requires clean Git source.

The wrapper builds the Debian arm64 native/app images, fetches pinned
firmware/protobuf/LVGL sources and the checked published upgrade baseline,
builds and tests the native radio, verifies original wheel/source downloads,
assembles the runtime, and builds/tests/installs the `.deb` in an isolated network
namespace. Full online runs also test minimal Debian dependency installation,
followed by offline first launch, cache repair and removal. No device/RF access
or deployment is performed. Root container outputs can need ownership adjustment
on a Linux host; inspect your own output directory, not broad system paths.

### Options and caches

| Option | Meaning |
|---|---|
| `--output DIRECTORY` | Separate build/cache root; default `build/distribution`. An output lock rejects overlapping builds into the same root. |
| `--native-artifact DIRECTORY` | Reuse a trusted native CI binary/source artifact set after integrity/provenance checks, rather than recompiling it. Hashes are integrity checks, not proof of authenticity of arbitrary third-party inputs. |
| `--prepare-only` | Prepare/verify dependencies and simulated-native smoke check; copy runtime and Python source into `OUTPUT/prepared/`. Does not build the UI package. |
| `--offline` | Require preloaded images and caches; no network for build/test runs. Skips the separate fresh APT dependency-download check. |

Typical commands:

```sh
# Prepare once with networking, then build from those same caches offline.
python3 scripts/build-package.py --output build/release-work --prepare-only
python3 scripts/build-package.py --output build/release-work --offline

# Optional alternative: trusted native artifact/source set already downloaded.
python3 scripts/build-package.py --output build/release-work \
  --native-artifact /path/to/verified-native-artifact --prepare-only
python3 scripts/build-package.py --output build/release-work \
  --native-artifact /path/to/verified-native-artifact --offline
```

Use either preparation route, not both with differing artifacts in the same
prepared output. Repeat `--native-artifact` in later calls when you used an
external artifact; the wrapper does not remember that argument automatically.
That directory needs `meshtasticd`, `NATIVE_VERSION`, `BUILD_INFO.json`,
`corresponding-source.tar.gz` and their `SHA256SUMS`. Source, patch, ELF architecture,
version and hashes are checked before its simulated execution.

Caches include native outputs (or that external artifact), `cache/lvgl` as the
pinned clean Git checkout, `cache/python` original downloads, `cache/baseline`
and preloaded `cz-mesh-native-build:debian13`/`cz-mesh-package-build:debian13` images.
Online native builds also retain `cache/pio`. `--offline` is not a zero-download
first build on an unprepared computer. It still exercises offline user runtime
preparation in the package-test container. Minimal fresh OS/APT resolution needs
the online/CI run; it is not silently claimed by the offline path.

Wrong/modified caches fail instead of being reset. An incomplete native output
or changed `prepared/` manifest also fails. Inspect/preserve the old output and
choose another `--output` for a clean preparation; never delete app user data
as a build-cache remedy. Do not run two different output builds expecting shared
Docker image tags to provide independent toolchain versions.

### Outputs

Under the chosen output root:

| Path | Purpose |
|---|---|
| `package-check/linux/dist/*.deb` | Built test installer |
| `prepared/runtime/`, `prepared/python-corresponding-source.tar.gz` | Retained by prepare-only; can supply raw CMake/manual export |
| `artifacts/` | Installer/runtime metadata, Python source archive, and the full release source set when the source checkout is clean |
| `cache/`, `native/` | Reusable build material, not user data or release assets by themselves |

The full source set contains `cardputerzero-mesh_VERSION-source-and-dependencies.tar.gz`,
`RUNTIME_BUILD_INFO.json`, `SOURCE_COMMIT`, `SHA256SUMS`, `INSTALL.txt` and
`VIDEO-CHECKLIST.md` with the installer. Local Python-only source extras are not
a substitute for the full archive. Nothing is uploaded or published by the wrapper.

## Rebuild from the supplied source/dependency archive

Verify the downloaded checksum set first, then unpack into a new scratch directory:

```sh
mkdir mesh-source
cd mesh-source
tar -xzf ../cardputerzero-mesh_0.1.19-source-and-dependencies.tar.gz
```

The archive has `app/`, `dependencies/lvgl/`, `dependencies/runtime/`,
`dependencies/native-corresponding-source.tar.gz`, `dependencies/native-BUILD_INFO.json`
and `dependencies/python-corresponding-source.tar.gz`. The app `SOURCE_COMMIT`
identifies the original code. Python originals include wheels plus source
archives; pyzbar has no PyPI sdist, so its unchanged Linux pure-Python wheel
provides its Python source and license. Native source includes resolved
framework/platform/library sources and build support, with minimal **public** Git
metadata for the banner, excluding remotes/config/hooks/reflogs and owner state.

This archive is not a preloaded Docker/toolchain/OS cache, and LVGL is a source
snapshot rather than the wrapper's Git cache. To reuse its supplied native/wheel
bundle and LVGL without downloading them again, use raw CMake in the matching image:

```sh
mkdir rebuild
docker build --platform linux/arm64 -t cz-mesh-source-rebuild \
  -f app/tests/Dockerfile app
docker run --rm --platform linux/arm64 --network none \
  -v "$PWD/app:/src:ro" -v "$PWD/dependencies/lvgl:/lvgl:ro" \
  -v "$PWD/dependencies/runtime:/runtime:ro" -v "$PWD/rebuild:/work" \
  cz-mesh-source-rebuild sh -e -c '
    cmake -S /src -B /work/linux -G Ninja -DUSE_DESKTOP=OFF \
      -DCM0_SDK_ROOT=/ -DCM0_ALLOW_FETCH_DEPS=OFF \
      -DFETCHCONTENT_SOURCE_DIR_LVGL=/lvgl -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=/usr -DMESH_RUNTIME_BUNDLE=/runtime
    cmake --build /work/linux -j 4
    ctest --test-dir /work/linux --output-on-failure
    cd /work/linux
    cpack -G DEB
  '
```

Image creation can download OS/build dependencies. The subsequent container is
network-disabled and mounts no hardware. This route compiles the UI and packages
the supplied radio/wheels; it does **not** recompile the radio or repeat the full
upgrade/firewall/fresh-install gates. Its default CPack output is under
`rebuild/linux/dist/`. Building is not proof of identical bytes or new hardware
qualification. Use [the full test route](build.md#regression-checks) for release work.

To rebuild the native radio itself, unpack its nested source archive separately,
restore `build-dependencies/packages` and `platforms` into the PlatformIO core
volume, and follow [native source rebuild instructions](../scripts/meshtasticd-gps/README.md).
That source already has the correction applied; do not apply the patch twice.
Integrate a changed binary by assembling a new verified runtime/package, not by
replacing the installed executable or editing its manifest in place.

## Manual assembly and source export

The wrapper/CI is the recommended complete route. For controlled tooling work,
run the assembler inside the documented Debian 13 arm64 image:

```sh
# Container paths: /src = checkout; /native = trusted artifact set;
# /cache = wheel/source cache; /out = writable staging parent.
python3 /src/scripts/runtime/prepare.py --native /native --cache /cache \
  --output /out/runtime
```

`/out/runtime` must not already exist; use a new staging parent for each assembly.
The sibling `python-sources/` staging directory must also be absent. Add
`--offline` only when its original downloads are already cached. Preparation
produces the runtime, Python source archive and licenses; it never accesses hardware.
Use `-DMESH_RUNTIME_BUNDLE=/out/runtime` for a native CMake install/package.

Export a release from clean Git source in a Debian environment with `dpkg-deb`:

```sh
python3 scripts/runtime/export_release.py \
  --deb /path/to/cardputerzero-mesh_0.1.19-rev1_arm64.deb \
  --native /path/to/verified-native-artifact \
  --runtime /path/to/prepared/runtime \
  --python-source /path/to/python-corresponding-source.tar.gz \
  --lvgl /path/to/pinned-clean-lvgl-git-checkout \
  --output /path/to/new-release-assets
```

The package must be built from that same unchanged source; do not label an old
same-version installer with a new checkout's commit. Export validates package
identity and paired native inputs, but does not independently reconstruct the
binary's entire compilation history. CI's fixed checkout/build/export sequence
provides that provenance. Keep CI run, source commit and exact package hash.

## Release review and publication

1. Pass CI/native/backend/package and fresh-container gates. Record actual
   device checks separately and preserve private rollback/data backups.
2. Audit exact installer/source assets, notices, screenshots and video for owner
   keys, chats, coordinates, endpoints and identifiers. All 15 Python license
   texts and native/full corresponding source must accompany redistribution.
3. Verify `SHA256SUMS`. Attach manuals with their separate `GUIDE_SHA256SUMS`
   and guide manifest; optional review evidence can have its own checksum set.
   Never overwrite installer/source checksums when refreshing documentation.
4. Recheck current Store policy/catalog/code availability. CZMS is selected, not
   reserved. The app name/package stay unchanged. Follow [Store preparation](store-submission-readiness-20261123.md).
5. Prepare a draft with installer, paired source, manifests/checksums, complete
   install instructions, manuals and [video checklist](store-demo-checklist.md).
   Keep the source archive available with any distributed GPL native binary.
6. The owner records the actual-device video. Review it and obtain publication/
   submission instruction before publishing or running the non-dry-run `czdev publish`.
   A draft is not publicly available, and an expiring CI artifact is not durable hosting.
7. After authorized publication, verify public downloads/checksums and later the
   Store listing/install outcome; a source push or merged PR alone is not that proof.

The [qualified existing-device record](distribution-validation-20261123.md)
retains the no-spare-card **fresh-image hardware deferral**. Minimal Debian
containers cannot establish fresh device permissions, camera/SPI/GPIO behavior,
physical cold power or every handset/network path. USB GPS/SIM and the other
explicit acceptance limits remain deferred. Builders/exporters never publish,
start RF or sign into an account automatically.
