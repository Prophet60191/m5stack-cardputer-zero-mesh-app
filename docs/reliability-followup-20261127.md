# Reliability review follow-up — 2026-11-27

This is a focused source and regression review after the 0.1.24 radio fixes,
not a promise that every app path or hardware failure is exhausted. It examined
radio ownership/start/stop, reconnect queues, trace/delivery results, bounded
socket parsing, Cap recovery and node/map/message selection/observer lifetimes.
It does not repeat the outdoor GPS, private-peer, phone or full physical-key checks.

## Confirmed fixes

- **Incorrect trace success:** routing NAK payloads or failed protobuf parsing
  became an empty route and a successful direct path. Only a valid traceroute
  reply from the requested destination now produces success. Mesh ACKs alone
  are insufficient; routing failures and malformed/wrong-source replies fail.
- **Trace lifecycle race:** the thread could send after validation but after
  channel/tuning/TX changes, or present a late result under a newer setup. A
  captured configuration and interface are checked under lifecycle ownership
  immediately before sending and before publishing a result. Stop/configuration
  changes cancel waiting work promptly. The RF wait holds no lifecycle lock.
- **Lost trace capacity:** failure to start a trace thread retained a semaphore
  slot indefinitely; the failure path now releases it.
- **Empty I2C read:** indexing an empty read raised IndexError, bypassing the
  bounded missing-chip recovery. It now raises EIO, which follows that existing
  recovery/restore policy. This is a simulated transport boundary case, not a
  newly observed hardware failure.
- **Unsafe signal conversion:** huge finite telemetry was converted to int before
  clamping, which can produce undefined behavior. Nodes now clamps in floating
  point first and treats a nonfinite value as an unknown/minimum bar. Normal
  signal display is unchanged. Actual LVGL tests cover both themes and extremes.

The pinned Meshtastic 2.7.11 client source was inspected directly: response
handlers receive NAKs as well as application replies; packet dictionaries carry
port names, raw payloads and numeric senders. The changes preserve the official
client and the existing modular authority boundary; no alternate radio owner,
new permission, automatic retransmission or saved-configuration change is added.

## Validation and installation

Mesh **0.1.25-rev1** is installed from runtime/package source
`31d3b437e79b4bccef946ff1a40097c5e886da7f`. Installer SHA256:
`354bf5e470e8e15bc8476989ed8be6558e5b7659171694c6fd27d305d1a0e3a2`.
All **267** packaged regular files, modes and root ownership match (the preserved
system conffile is excluded from package-byte equality). Original saved app files
and all original SQLite rows remain, with database integrity OK.

All **199 backend tests** pass, including eleven new backend cases. Seven host
ASan/UBSan CTest suites and six Debian arm64 CTest suites pass; the isolated arm64
package/install/upgrade/firewall/offline runtime checks pass. Exact-source
[CI run 46018922395](https://github.com/Prophet60191/m5stack-cardputer-zero-mesh-app/actions/runs/46018922395)
at closeout has a successful regressions job; its separate Debian arm64 job is
still building the matching environment. Local arm64 qualification above is
complete. Do not label the unfinished CI job passed. Its artifact has an independent
checksum; the installer hash above identifies the locally installed package only.

The new executable is open cleanly through the actual launcher. The first UI
reopen helper assumed Enter would reopen Mesh; the launcher instead selected
its IP Panel. Returning to the launcher and selecting the visible Mesh icon
corrected the test context. No CLI/terminal execution is used for final UI checks.
A premature byte check ran before APT finished and naturally saw the old UI;
the completed install subsequently passed full byte/mode/owner verification.
Private test evidence retains those harness failures rather than counting them
as app regressions. No other app/package was upgraded or removed.

Live follow-up passes **3/3 full Cap power-off/start cycles**, each with the rail
verified Off first, reaching Online/send-receive in **1.95-3.24 seconds**. The
following **two-minute watch passes all 12 samples** of app 0.1.25,
Online/send-receive, connected API and trusted firewall health. Final independent
readback is also healthy. These are Cap cycles, not whole-device cold boots; the
preceding 0.1.24 twelve-start/ten-minute qualification is a separate record.
No fresh live-peer traceroute/message, outdoor GPS, phone, USB GPS, SIM or
fresh-image hardware result is claimed for this follow-up. Fault cases were
simulated, and the actual device check exercised normal startup/UI operation.
ADB stays enabled and the radio remains Online/send-receive. Exact task-created
device stages are removed after byte-verifying the retained private host backup;
other apps, the separate app-builder guide and deferred work are untouched.
The new backend tests use real protobufs, synthetic interfaces and controlled
thread failure/configuration changes; they open no device/socket or RF path.
