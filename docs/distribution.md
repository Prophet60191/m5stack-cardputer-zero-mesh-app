# Distribution and dependency bundles

## Install, without manual Python or radio setup

Mesh 0.1.19 targets the official Debian 13 arm64 CardputerZero image. Install
its single `.deb` using `sudo apt install ./cardputerzero-mesh_0.1.19-rev1_arm64.deb`.
APT can download missing standard OS libraries. This is not a completely
self-contained OS image, and a fresh hardware image has not been qualified.

The installer includes a private GPS-corrected native Meshtastic runtime and 15
hash-pinned Python wheels. On first launch the ordinary user's fixed helper
checks those root-owned files and builds a private managed virtual environment
without contacting a package index. Subsequent launches verify/reuse the cache.
An incomplete or invalid cache is rebuilt offline. Legacy user-created `venv/`,
chats, drafts, channels and saved setups are not replaced or removed.

The fixed private binary is used instead of searching PATH. A previously
installed global meshtasticd is not required, removed or unmasked. Keep any
independent global daemon stopped so it cannot contend for the Cap. Install
scripts stop only this app's user supervisor before replacing its files. A
package-update marker blocks reopening during replacement; successful dpkg
configuration clears it. If installation is interrupted, finish it with the
package manager before reopening Mesh; do not manually bypass that marker.

Installation never starts RF. Close Mesh and turn Radio Off before upgrading;
reopen afterward and use the existing transmit/antenna choices. Preserve platform
hardware permissions; do not run the app as root. Camera capture uses the OS's
`rpicam-apps-core`; QR decoding wheels, Pillow and zbar are covered by the package.
USB camera fallback needs optional OpenCV. Phone access needs separately managed
Tailscale/accounts. Credentials and operator configuration are never bundled.

## Build from a clone

With Python 3, Git, Docker and Linux arm64 support on the build host:

```bash
python3 scripts/build-package.py
```

First use downloads build images, pinned native/LVGL source and locked Python
artifacts. It builds the native radio, performs simulated-radio SDK checks,
assembles the private runtime, and runs native, backend, package, upgrade and
firewall checks in isolated containers. Hardware/RF is not exposed. Results are
under ignored `build/distribution/`. This is intentionally a large build; normal
users should install the prebuilt `.deb` instead.

`--native-artifact DIRECTORY` reuses a qualified CI native artifact/source pair
only after hash/source/patch validation. `--prepare-only` prepares dependencies.
`--offline` requires previously prepared caches and Docker images, including the
upgrade-baseline package. It does not promise a fresh build without any downloads
on an unprepared computer. Modified or wrong-revision source caches are rejected
rather than reset. Use a different `--output DIRECTORY` for separate builds.

## Redistribution and corresponding source

CI exports the installer, `SOURCE_COMMIT`, `RUNTIME_BUILD_INFO.json`,
`SHA256SUMS`, install instructions, video checklist and
`cardputerzero-mesh_VERSION-source-and-dependencies.tar.gz` together. The archive
contains committed application source, pinned LVGL, full native firmware/framework
and dependency sources, original Python wheels/source distributions, licenses,
locks and build scripts. For pyzbar, PyPI provides no sdist: its unmodified Linux
pure-Python wheel supplies the Python source and license. Build images/toolchain
and Debian OS packages remain external and retain their own distribution terms.
The native source archive includes public upstream Git objects needed for its
version banner; it excludes remotes/config/hooks/reflogs and owner state.

Never redistribute the `.deb` without its paired corresponding source and
licenses. Check `sha256sum -c SHA256SUMS` before installation or archival. Export
only clean committed source with `scripts/runtime/export_release.py`; the CI
workflow shows exact arguments. A version number alone does not establish which
same-version candidate was built. Record source commit, CI run and package hash.

No release, Store PR, developer-account authentication or video submission is
performed by the builder/exporter. The Store code CZMS is a proposed identifier,
not a reservation. Catalog availability must be checked again at submission.

## Qualification boundaries

Fresh minimal Debian container installation, offline first-run, cache repair,
QR dependency imports and uninstall are automated independently of a previously
prepared device. These checks cannot prove platform permissions, actual camera,
SPI/GPIO timing or boot behavior on a fresh Cardputer image. The owner has no
spare microSD; fresh-image hardware qualification is explicitly deferred. The
existing device's upgrade checks must be recorded separately. The owner will
record the real-device demo using [the checklist](store-demo-checklist.md).
