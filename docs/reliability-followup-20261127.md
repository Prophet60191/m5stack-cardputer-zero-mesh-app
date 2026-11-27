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

Pending final package/native validation and installation. Current device stays
on the qualified 0.1.24 until the exact new package is verified and installed.
The new backend tests use real protobufs, synthetic interfaces and controlled
thread failure/configuration changes; they open no device/socket or RF path.
