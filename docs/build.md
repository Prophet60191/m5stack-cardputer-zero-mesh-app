# Building

Two targets share one codebase:

- **Desktop simulator** (SDL) — fast, flash-free UI iteration on your computer. It renders at the device-native `320x170` and composites into a windowed shell.
- **Device build** — the real app on the CardputerZero's framebuffer.

The UI builds on its own. The **radio side** (`meshtasticd` + the `meshtastic` Python library) is GPL-3.0 and is installed separately — see the main [README](../README.md#install--run).

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

Build **natively on the CardputerZero** (simplest), or cross-compile from a host.

**Native (on the device):**

```bash
cmake --preset cp0-native -DCM0_SDK_ROOT=/ -DCM0_ALLOW_FETCH_DEPS=ON
cmake --build build/cp0-native --config Release
./build/cp0-native/Release/cardputerzero-mesh     # run on the framebuffer (stop APPLaunch first, or install to the grid)
```

**Cross-compile (from a host with an `aarch64-linux-gnu` toolchain):**

```bash
cmake --preset cp0-cross
cmake --build --preset cp0-cross-rel
```

## Debian package (optional)

Package on a matching Debian arm64 build environment so `dpkg-shlibdeps`
resolves the actual ELF library dependencies. CMake must be **3.31 or later**;
check your distro's version. Desktop simulator builds are not device packages.

```bash
cmake --preset cp0-native -DCM0_SDK_ROOT=/ -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build/cp0-native --config Release
cd build/cp0-native
cpack -G DEB -C Release
```

Output is `build/cp0-native/dist/cardputerzero-mesh_0.1.5_rev1_arm64.deb`.
The package contains the UI, assets, launcher, supervisor/guardian sources, both
licenses, user service, capability-limited firewall boot service, and fixed sudo
check policy. The launcher starts the supervisor; it does not automatically start
RF. Follow the README for the user's group and pinned venv setup. The optional
system-level UI autostart unit is unrelated to the radio supervisor and stays off.

Inspect before installation:

```bash
dpkg-deb --info dist/*.deb
dpkg-deb --contents dist/*.deb
```

## Regression checks

```bash
python3 -m venv .venv
.venv/bin/pip install 'meshtastic==2.7.11'
.venv/bin/python -m unittest discover -s tests -p 'test_*.py' -v
cmake --preset darwin-arm64 -DBUILD_TESTING=ON
cmake --build build/darwin-arm64 --config Debug
ctest --test-dir build/darwin-arm64 -C Debug --output-on-failure
```

`mesh_regressions [output.png]` renders a wholly synthetic message screenshot
using the real LVGL screen. Python tests use real pinned protobufs with fake radio
and sysfs controls, including real subprocess death; they never power hardware.
`tests/Dockerfile` provides an isolated Debian 13 arm64 build/test environment.
Run `tests/linux-package-check.sh` there with `CZ_MESH_ISOLATED_TEST=1`, read-only
source/LVGL mounts at `/src` and `/lvgl`, and a writable build mount at `/work`. The
firewall tests require NET_ADMIN **inside that container's network namespace**;
do not run them against a live host ruleset.

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
library dependencies, sudoers, units, the capability-only firewall monitor and
backend regressions. It uploads the `.deb`, `SHA256SUMS` and `SOURCE_COMMIT` as a
workflow artifact, without publishing a binary release or starting RF.

These artifacts are candidates. Hardware qualification still requires the
operator's antenna, intended peer, GPS sky view, camera target and actual power
cycle; CI cannot establish those results. The qualified daemon is meshtasticd
2.7.26 (preference schema 24); a different daemon/client version fails before Cap
power-on until explicitly qualified in source and tests.
