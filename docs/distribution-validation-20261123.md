# Mesh 0.1.19 distribution qualification — 2026-11-23

## Exact candidate

Functional/package source: `8ada52add1b9fb19524002daba5adf1d054ade97`.
Later closeout changes are documentation only. The exact downloaded CI installer
is installed; do not substitute another same-version local development package.

- [Both regression/package CI jobs passed](https://github.com/Prophet60191/m5stack-cardputer-zero-mesh-app/actions/runs/45269883900).
- Installer: `cardputerzero-mesh_0.1.19-rev1_arm64.deb`, 7,844,182 bytes.
  SHA-256: `feb0579ef8517b3629d10c8ac6e098751e9c85c6c02727e0bccbdfd190057a61`.
- Paired source/dependency archive SHA-256:
  `8eae08a06ea6bdda691ee526a47f428d3e73a97ccbeeb0705be420c326fbf1ee`.
- Package-private native binary SHA-256:
  `fd9db93217f0b305246c27aab6752ba2947fb07007b743cf778807e5ceed0537`.
  Qualified upstream `54e0d8d0ab2ff56b3a9ce967e53f79e49af560fb`,
  identification-timeout patch SHA-256
  `33620bd494e4de69eff9944a9ae3d98418dada08f008355ee78947c215e64036`.
- Exact installer and source assets pass their downloaded `SHA256SUMS`.
  All 255 installed regular files match CI bytes, modes and ownership.

## Results

| Check | Result and limits |
|---|---|
| Backend/native checks | All 175 backend tests and native sanitizer/regression gates pass; real native/SDK simulated-radio checks run without network or hardware. |
| Package closure | Matching Debian arm64 build/install, old-version upgrade, dependency metadata, real firewall tampering/recovery and current pinned Store preflight pass. |
| Minimal Debian | Clean Debian 13 arm64 container resolves OS dependencies, prepares/reuses/repairs the ordinary-user runtime offline, rejects corrupt wheels, loads QR dependencies and removes the app while retaining user data/legacy environment. This is not a fresh device image. |
| Offline builder | Documented cached build path completes without networking; it explicitly skips the separate fresh APT download rehearsal. Full source/dependency export completes. |
| Installer ordering/removal | Python is Pre-Depends because preinst needs it. Separate isolated-container purge with the Python executable unavailable passes; postrm uses Essential shell tools. |
| Licenses | Original wheel/source license texts retained for all 15 Python distributions, including pyserial's source-distribution license; native licenses and paired full corresponding source retained. |
| Existing-device upgrade | Verified private, consistent SQLite/settings backups and qualified rollback retained. The installer stopped the existing user supervisor before replacement; successful configuration removed its update marker. |
| Real runtime | Managed user environment prepared from bundled wheels; the old user environment and global native executable are unchanged. Global standalone service stays masked/inactive. All active requirements of the 15 bundled distributions are checked separately from unrelated inherited Debian pip metadata. |
| Radio/GPS | Three normal starts identify the fitted receiver. A further seven-sample 77.8-second watch passes with one private native process, one physical API connection, the same supervisor PID, healthy firewall and preserved send/receive/GPS/privacy/Phone settings. No new outdoor or cold-power claim. |
| Display/lifecycle | Normal device launch, isolated real framebuffer header capture and return to the launcher pass. Header text and percentage remain visible without a battery icon. Opening/closing the UI did not restart the monitored radio owner. |
| Private RF | One new private test message receives Recipient ACK. All 26 original messages remain; the expected test message is the 27th. Three thread and three preference rows and saved radio/profile/app settings remain intact. |
| Phone protocol | Real SDK loads the existing gateway configuration and passes heartbeat/settling checks without sending RF. This trusted-loopback fixture does not repeat iPhone/Tailscale cellular acceptance. Its first immediate teardown raced SDK startup; waiting for background heartbeat completion produces a clean repeat. No app failure is inferred from that fixture cleanup error. |

A device-wide pip check reports inherited system metadata warnings; those are
not treated as bundled dependency failures and no unrelated OS/app packages were
changed. The 15 app distributions are present in the private environment and
their active dependency requirements are satisfied.

## Publication and sensitivity

CZMS replaces the taken MESH Store lookup code; the app/package name is unchanged.
CI preflight uses reviewed catalog `b6c559a`. A final upstream check at
`dad933870a2b2de6bd59f11172b0b2ad2e3b54e8` found only unrelated RF Heatmap updates,
unchanged policy bytes and no CZMS mention. This is not a reservation; check again
at submission. The preflight's only recommendation is optional listing localization.

Targeted tracked-tree/history and final source-archive text scans found no known
owner system identifiers, credentials or private share links in their inspected
scope. Synthetic test keys and public upstream material are distinct from owner
state. Private backups, raw device logs, endpoints, keys and chats remain outside
public source/assets. Review the owner's video separately before submission.

The paired installer/source assets, runtime manifest, checksums, install text and
video checklist are prepared for release review. Manual checksums have their own
asset name so release documentation uploads cannot replace installer verification.
No public release, Store submission, developer-account login or upstream PR is
performed by the build/export tooling. A prepared draft is not public availability.

## Deferred and not established

The owner has no spare microSD and explicitly defers fresh-image hardware testing;
the working card was not erased. The owner will record the first-submission
real-device video using [the checklist](store-demo-checklist.md). Publication and
Store submission still need their own instruction/review. USB GPS and SIM work
remain deferred. Earlier phone foreground-drop cause, full keyboard sweep,
live public-peer delivery, physical IPv6 and long-term reliability remain at
their documented limits. Do not convert prior checks into newly repeated claims.

ADB remains enabled. Launcher is restored; Radio is Online/send-receive, GPS
receiver enabled, automatic location/MQTT Off and selected Phone access On.
