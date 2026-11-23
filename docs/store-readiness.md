# Store packaging corrections — 0.1.9 candidate

> Historical/versioned record. For 0.1.19 bundled-runtime installation use the
> [current runbook](install.md); for current source/package evidence see
> [qualification](distribution-validation-20261123.md). Earlier standalone
> daemon/manual-venv and pending-state instructions below describe their named version.


The 0.1.8 package failed the official Store metadata/path checks in six places.
This candidate addresses those issues while keeping the application name, UI,
radio policy and user storage namespaces unchanged.

| Area | Correction |
|---|---|
| Categories | `Radio & Comms`, `Hardware & IoT`, from the exact official enum |
| Share code | `MESH`; checked against the reviewed catalog, not reserved until accepted |
| Store icon | Separate 256×256 PNG rendered from the included SVG; the runtime launcher icon is unchanged |
| Wrapper | `/usr/bin/cardputerzero-mesh-launch`, with a matching desktop Exec; old `mesh-launch` is removed on upgrade |
| User service | Fixed transient service through the user's systemd manager; no static unit in a rejected shared directory |
| Filename | Debian control version/revision used in the canonical `.deb` filename |

The transient supervisor retains NoNewPrivileges, private umask, control-group
termination, failure restart and guardian stop recovery. Its fixed cleanup path
uses the kernel UID: static `%t` specifiers did not expand in transient
ExecStopPost during the device probe. The helper serializes lifecycle actions,
refuses root and invalid runtime locks, and does not restart an active service
when another UI opens. No unit/command/property is chosen by a socket intent.
See [security.md](security.md#store-compatible-user-service-019).

The [package workflow](../.github/workflows/regressions.yml) checks out pinned
Store policies and the [non-publishing checker](https://github.com/Prophet60191/m5stack-cardputer-zero-app-guide/tree/d414f602e219b3e40d2630022f3996bc8b9c4fcd/tools).
It builds the package on Debian 13 arm64, checks fresh installation and an actual
0.1.8 archive upgrade, verifies obsolete paths are gone, and runs preflight on
the exact uploaded package. Artifacts include its SHA-256, source commit and
`store-preflight.json`. Policy/catalog revision:
`94eb8d7ad524e8ee01a52658f31871e6377c7544`; both policy hashes match the
previous review. Recheck current catalog availability before Store submission.

## Verification and limits

The new service tests cover idempotent launches, explicit stop/restart ordering,
manager failures, root refusal, lock permissions/link attacks and bounded lock
contention. Existing backend, LVGL/keyboard and sanitizer checks remain required.
An actual-device dummy-service probe exercises the user manager, UID,
NoNewPrivileges, umask, cgroup child termination, crash restart and cleanup without
opening a radio, camera or display; it leaves the running supervisor untouched.

External prerequisites remain explicit: the qualified meshtasticd build,
meshtastic 2.7.11 in the app-private environment, and optional camera/QR packages.
The installer does not download a Python environment or silently enable RF.
A matching clean Debian container validates packaging; it does not reproduce the
stock M5Stack image, external radio dependencies or physical peripherals.
See [the hardware qualification record](review-remediation.md) for remaining
outdoor GPS/public-peer/physical-network checks. A local Store pass is not Store
approval or a guarantee that every possible bug is absent. No `czdev` login,
upload or Store PR is part of these fixes.

The SVG is the maintained Store artwork. Render it with
`rsvg-convert --width 256 --height 256 --output packaging/store-icon.png packaging/store-icon.svg`.
The icon uses the existing green/white network identity and contains no device
data or image metadata.
