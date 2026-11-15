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

**Windows:** install CMake + Ninja + an MSVC or MinGW toolchain; dependencies come through vcpkg (`vcpkg.json`). Launch CMake from a Developer prompt for MSVC.

## Desktop simulator

```bash
# Linux
cmake --preset linux-x86-64 && cmake --build --preset linux-x86-64-rel
./build/linux-x86-64/Release/template_app

# macOS (Apple Silicon)
cmake --preset darwin-arm64 && cmake --build --preset darwin-arm64-rel
./build/darwin-arm64/Release/template_app
```

(Use `darwin-x86-64` on Intel Macs, `win32-msvc` / `win32-mingw64` on Windows.)

## Device build

Build **natively on the CardputerZero** (simplest), or cross-compile from a host.

**Native (on the device):**

```bash
cmake --preset cp0-native -DCM0_SDK_ROOT=/ -DCM0_ALLOW_FETCH_DEPS=ON
cmake --build build/cp0-native --config Release
./build/cp0-native/Release/template_app     # run on the framebuffer (stop APPLaunch first, or install to the grid)
```

**Cross-compile (from a host with an `aarch64-linux-gnu` toolchain):**

```bash
cmake --preset cp0-cross
cmake --build --preset cp0-cross-rel
```

## Debian package (optional)

CPack produces a `.deb` into `dist/`. The package installs the app, its assets, the APPLaunch launcher entry, and the `LICENSE`/`NOTICE`. The optional systemd service is **off by default** (the app launches from the APPLaunch grid as the logged-in user); if you enable it, you must pass a non-root `APP_SERVICE_USER`:

```bash
cmake --preset cp0-cross \
  -DAPP_INSTALL_SYSTEMD_SERVICE=ON -DAPP_SERVICE_USER=<youruser>
cmake --build --preset cp0-cross-rel
cpack --preset cp0-cross-deb
```

The `.deb` does **not** bundle `meshtasticd` or the `meshtastic` library (they are GPL-3.0 and installed separately) and does **not** start the radio supervisor for you — see the README for the supervisor setup and the loopback firewall step.

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
