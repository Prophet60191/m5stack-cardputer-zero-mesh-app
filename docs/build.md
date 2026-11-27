# Building

Two targets share one codebase:

- **Desktop simulator** (SDL) — fast, flash-free UI iteration on your computer. It renders at the device-native `320x170` and composites into a windowed shell.
- **Device build** — the real app on the CardputerZero's framebuffer.

The UI builds on its own. The **radio side** (`meshtasticd` + the `meshtastic` Python library) is GPL-3.0 and is bundled in the device package — see [distribution](distribution.md).

## Dependencies

**Desktop (Debian/Ubuntu):**

```bash
sudo apt install -y build-essential cmake ninja-build \
  libpng-dev libjpeg-dev libfmt-dev libsdl2-dev libfreetype-dev zlib1g-dev
```

**Desktop (macOS, Homebrew):**

```bash
xcode-select --install
brew install cmake ninja libpng jpeg fmt sdl2 freetype dpkg zlib
```

**Windows:** inherited Template presets remain in the tree, but this Unix-socket
client has not been ported or validated on Windows. Verified targets are macOS
desktop and Debian 13 arm64 device builds.

## Desktop simulator

```bash
# Linux
cmake --preset linux-x86-64 && cmake --build --preset linux-x86-64-rel
./build/linux-x86-64/Release/cardputerzero-mesh

# macOS (Apple Silicon)
cmake --preset darwin-arm64 && cmake --build --preset darwin-arm64-rel
./build/darwin-arm64/Release/cardputerzero-mesh
```

Other inherited desktop presets are unqualified.

## Device build

The complete container package route below is recommended for installation.
The following presets build only the UI; they do not prepare/install the bundled
backend by themselves. For manual framebuffer work, use a normal user, coordinate
the launcher's display/input ownership and restore it afterward. Do not run a
second UI against the active launcher or use an assumed `--version` GUI argument.

**Native (on the device):**

```bash
cmake --preset cp0-native -DCM0_SDK_ROOT=/ -DCM0_ALLOW_FETCH_DEPS=ON
cmake --build build/cp0-native --config Release
# Use the complete package route for normal device installation.
# Raw UI execution needs the installed backend/firewall and exclusive display ownership.
```

**Cross-compile (from a host with an `aarch64-linux-gnu` toolchain):**

```bash
cmake --preset cp0-cross
cmake --build --preset cp0-cross-rel
```

## Complete Debian package

Use Docker with Linux arm64 support and Python 3 on the build host:

```bash
python3 scripts/build-package.py
```

This fetches pinned sources, builds the GPS-corrected private radio runtime,
verifies locked wheels, builds the app and runs isolated native/backend/package
checks. It does not access hardware or transmit. See [distribution](distribution.md)
for caches, offline mode, complete source export and release verification.

Raw CMake/CPack builds require `-DMESH_RUNTIME_BUNDLE=/absolute/runtime/path`;
installing a native build without that verified bundle fails deliberately.
Desktop-only UI builds do not need the radio bundle. Keep the platform's
permissions and firewall integration; do not run the UI as root.

## Regression checks

The host venv below is a development test environment, not the installed app's
managed runtime. Device users do not need these pip commands.


```bash
python3 -m venv .venv
.venv/bin/pip install 'meshtastic==2.7.11'
.venv/bin/python -m unittest discover -s tests -p 'test_*.py' -v
cmake --preset darwin-arm64 -DBUILD_TESTING=ON
cmake --build build/darwin-arm64 --config Debug
ctest --test-dir build/darwin-arm64 -C Debug --output-on-failure
```

`mesh_ui_checkup` checks layout, list wrapping, overlays, editors, notifications,
QR, header, search and Unicode at 320×170 in both themes. It uses synthetic data
and no hardware; see [the review record](ui-checkup.md).

`mesh_maintenance_regressions` runs on desktop and Linux device-target builds
with a headless LVGL display, controlled clocks and synthetic state. It opens no
radio socket, framebuffer or camera and is excluded from the installer.

`mesh_regressions [output.png]` renders a wholly synthetic message screenshot
using the real LVGL screen. Python tests use real pinned protobufs with fake radio
and sysfs controls, including real subprocess death; they never power hardware.
`tests/Dockerfile` provides an isolated Debian 13 arm64 build/test environment.
For the complete isolated gate, first prepare the wrapper's inputs/images:

```sh
python3 scripts/build-package.py --output build/release-work --prepare-only
mkdir -p build/manual-package-check
docker run --rm --platform linux/arm64 --network none --cap-add NET_ADMIN \
  -e CZ_MESH_ISOLATED_TEST=1 \
  -e CZ_MESH_UPGRADE_BASELINE=/baseline/mesh-0.1.8.deb \
  -v "$PWD:/src:ro" \
  -v "$PWD/build/release-work/cache/lvgl:/lvgl:ro" \
  -v "$PWD/build/release-work/cache/baseline:/baseline:ro" \
  -v "$PWD/build/release-work/prepared/runtime:/runtime:ro" \
  -v "$PWD/build/manual-package-check:/work" \
  cz-mesh-package-build:debian13 sh /src/tests/linux-package-check.sh
```

The `/runtime` mount is required. The gate verifies the published 0.1.8 baseline
checksum before its upgrade test, builds/runs native regressions, installs the
real `.deb`, exercises firewall changes, offline managed runtime and backend tests.
NET_ADMIN belongs **only to that isolated container's network namespace**. Never
use host networking, privileged mode or a live host ruleset for these tests.
If your container runtime cannot provide that isolation, use the matching CI job.

The wrapper's normal full online invocation additionally performs a minimal
Debian fresh-install/offline-first-run/remove check. That image deliberately has
no pre-existing SDK/global radio/user venv; the matching build image does contain
build/test dependencies. Both are separate from physical fresh-image acceptance.

## Project layout

```
src/            C++/LVGL app (MIT)
  app/          lifecycle, config, screen management
  viewmodel/    state + intents to the supervisor
  view/         screens and widgets
  platform/     framebuffer, evdev input, the supervisor socket client, JSON
supervisor/     Python radio supervisor (GPL-3.0) + firewall-setup.sh
assets/         fonts, images, audio
cmake/          device packaging + toolchains
docs/           this documentation
```

## Automated Debian arm64 artifact

The `debian-arm64` GitHub Actions job runs on a native `ubuntu-24.04-arm`
runner. It builds a Debian 13 container, then builds/installs/tests the package
with networking disabled in an isolated namespace. LVGL is fetched at the fixed
9.5.0 commit; the radio client is pinned to 2.7.11. The job validates ELF
library dependencies, absence of app sudo grants, units, the capability-only firewall monitor and
backend regressions. It also builds the private native radio from pinned firmware with the GPS
correction, prepares locked wheels/licenses and tests a minimal fresh Debian
installation followed by offline runtime/cache/removal checks. It uploads the
`.deb`, full corresponding-source/dependency archive, runtime manifest,
`SHA256SUMS`, `SOURCE_COMMIT`, complete install/video instructions and preflight
JSON. See [the asset inventory](distribution.md#outputs). It does not publish a
binary release, install on a Cardputer or start RF.

These artifacts are candidates. Hardware qualification still requires the
operator's antenna, intended peer, GPS sky view, camera target and actual power
cycle; CI cannot establish those results. The qualified daemon is meshtasticd
2.7.26 (preference schema 24); a different daemon/client version fails before Cap
power-on until explicitly qualified in source and tests.


The authoritative app version is `app-builder.json`. CMake reads it for the UI and
Debian package; the supervisor reads the same file from source or its installed
copy. There is no independent C++ fallback or Python version literal.


## Optional maintenance measurements

Configure with `-DMESH_BUILD_BENCHMARKS=ON`, build target `mesh_benchmarks`, then
run it as an ordinary user. It uses synthetic history and a headless LVGL display;
it opens no radio socket, framebuffer or camera. The isolated package gate also
compiles/runs it and checks that the binary is excluded from the installed package.
Use `mesh_benchmarks --maintenance` to restrict a device run to retention,
conversation navigation and unchanged-thread measurements. Run before/after
sequentially, with identical data/build flags and no other benchmark process.
Timing results are informational, without timing thresholds in regression tests.
See [the measurement record](maintenance-pass.md) for the actual-device baseline.

## Keyboard regressions

CTest includes `mesh_keyboard_regressions` on the desktop host. On Linux it
also builds/runs `mesh_keyboard_evdev_regressions` with the production device
reader, synthetic pipe records and a focused LVGL textarea. The device package
gate invokes that test and verifies neither helper is installed. See the
[keyboard contract](keyboard.md) for the official map, reuse and qualification
boundaries. No keyboard test injects into live `/dev/input` or sends RF.

## Fitted GPS native correction

The [native GPS support workflow](../scripts/meshtasticd-gps/README.md)
builds exact upstream source with the ATGM identification timeout correction,
tests the actual parser and simulated daemon, and exports the executable with
full corresponding source and checksums. It does not deploy or publish a release.
The [device qualification](internal-gps-validation.md) records the separate app
cache fix, native package identity, real receiver starts and remaining limits.

## Deployment boundaries

The complete wrapper creates artifacts but never deploys them. Transfer and
install the verified file using [the installation runbook](install.md). The
inherited `deploy.sh` is a low-level cross-preset **build-and-copy helper**, not
the bundled-runtime release builder: it needs a correctly prepared CMake/runtime
configuration, selects a local package and copies it with scp. It does not verify
remote installation, stop the radio or install dependencies. Do not use it as a
replacement for the documented checked package workflow or publish its outputs
merely because the copy succeeded.

`mesh_messages_regressions` exercises the two lists, combined message search,
identity-safe result selection and bounded history rendering with 2,000 synthetic
messages in both themes. `mesh_messages_load_benchmark` is an optional
`MESH_BUILD_BENCHMARKS=ON` target for headless load timing with no socket or RF.
Neither executable is installed by the Debian package.
