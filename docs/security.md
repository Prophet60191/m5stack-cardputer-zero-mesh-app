# Security model and verification limits

The C++/LVGL UI is MIT; the radio supervisor and guardian/firewall helpers are
GPL-3.0-or-later. Their full license is shipped alongside the Python sources.
The external Meshtastic daemon/library are installed separately.

## Trust boundaries

The local intent socket is a same-user control API, not a sandbox against other
programs running as that user. Such programs can turn the radio on, change
settings, delete session history, and request a channel share URL. Never forward
this socket over TCP. The supervisor accepts named actions, not shell commands,
filenames, executable paths, or arbitrary config paths.

The socket directory must be a real directory owned by the current UID (0700);
the socket is 0600 and the Linux client verifies SO_PEERCRED. A held `flock`
prevents a second supervisor replacing the first one's socket. The UI's reader
and writer run off the LVGL thread; only `poll()` invokes UI handlers.

`meshtasticd` binds all interfaces. A dedicated nftables `inet cz_mesh` table
drops TCP 47400 on every ingress interface except `lo`, including IPv6. The
package enables a boot service to establish it. That short-lived service runs
as `cz-mesh-firewall` with only CAP_NET_ADMIN; the app and supervisor run as the
logged-in user. A root-owned helper accepts only fixed `ensure`, `check`, and
`remove` actions. The app user's sudo grant permits **only `check`**. Startup
fails before powering hardware if the rule is absent/changed or the helper
cannot verify it; the watchdog rechecks while online. A privileged administrator
can still change networking or remove rules between checks. Loopback clients
can access the unauthenticated daemon API; this is not isolation from local users.

The separate system-wide `meshtasticd.service` must be masked or disabled and
inactive. Mask it **before** dependency installation so its package cannot start
an unrelated API listener (normally TCP 4403) outside this app's firewall and
lifecycle. Radio startup checks for this conflict before powering the Cap.

## Radio and privacy policy

Public and private channels are both supported. Every RF action requires an
explicit enabled channel with a supported key length (0, 1, 16 or 32 bytes).
Missing/invalid/disabled channels never fall back to primary. Destination IDs
must be unicast `!` plus eight hex digits; a DM is not a validation bypass.
Empty/single-byte keys, all-zero keys and expanded published-default keys are
labeled Public. A custom key is only private while its owner keeps it secret.
The channel label does not certify the firmware's separate DM encryption.

Imports accept public/default/unnamed and private channels. Validate the complete
set and available capacity before writing; deduplicate by name plus key. Imports
always disable location and MQTT forwarding on the new channels and confirm
actual readback. URL radio parameters do not silently retune the node. Channel
QR sharing serializes exactly the requested channel; other keys are excluded.

Automatic location requires selecting a channel. Only that channel receives
nonzero precision; Off zeroes every enabled channel. Public automatic location
requires `public_confirmed:true` and is capped at 15 bits (roughly 730 m). After
successful admin readback, a private, atomically saved consent digest binds that
choice to its channel index/name/key/precision. Changed or unapproved public
location is disabled in persisted channels **before power-on**, avoiding firmware
factory-default location broadcasts. Private-channel location is preserved.
If a position write/readback/consent save fails, stop the radio and show Unknown.
Manual `position.send` sends precise coordinates as text; a public channel needs
its own confirmation, independently of automatic sharing. Position requests ask
the peer for its position and do not populate our own coordinates.

Both preference files must use qualified schema 24 and required radio/channel
data. Empty/old protobufs can parse but firmware can discard them and install
factory defaults; unsupported schemas fail closed before power-on. These checks
permit public traffic: firmware identity/telemetry and relayed traffic can be
public. This is an intended public-mesh mode, not a promise of RF silence.

All messaging channels share the radio's region, modem preset and frequency slot.
The frequency-slot setting is range checked and read back, including Auto (0).
Changing a chat channel does not tune a second radio or automatically change the
mesh frequency. Preserve operating settings when importing a channel.

Settings show **Unknown** while disconnected/unconfirmed and **Pending** during
writes. Privacy booleans change only after actual daemon admin readback; timeout,
rejection, or mismatched readback leaves them unknown. “Off” describes automatic
position broadcasts, not a ban on a later explicit “Send my location” action.
MQTT Off refers to this radio's MQTT module, not a guarantee that another node
will never forward RF traffic to the Internet.

Channel sharing carries channel index plus a request ID through the entire path.
The QR contains only that channel's settings and LoRa config. Dismissed, stale,
or mismatched replies cannot open a share overlay. Treat the QR/URL as a secret.

## Lifetime and resource bounds

A separate guardian owns Cap power and the daemon. The supervisor holds its
stdin pipe; EOF after supervisor death (including SIGKILL) causes child shutdown
and reaping **before** switch restoration. A hardware lease prevents concurrent
guardians. The user service's control group and ExecStopPost recovery record
cover simultaneous process termination. A kernel parent-death signal, configured
only by the single-threaded guardian, stops the daemon if the guardian itself is
killed; the supervisor then recovers the saved switch values. A Python `finally` alone cannot handle
SIGKILL. Prior switch values are restored; a pre-existing powered rail can
therefore remain powered. Software cannot guarantee restoration after physical
power loss, kernel failure, or unavailable sysfs controls.

Intent frames: 64 KiB, nesting 16; at most eight clients, 16 queued operations,
and two active traces. Each client has at most 64 outbound frames and 4 MiB of
queued bytes; oversized events or slow clients are disconnected. Event frames
at the UI are capped at 2 MiB; JSON depth/value/string limits and a 128-event,
16 MiB estimated allocation queue bound parsing and buffering. UI writes use a
bounded background queue and absolute deadline. Any incomplete write closes the
stream; failed operations are never replayed on reconnect. Polling handles at
most 32 events per UI tick. History is capped at 2,000 messages globally and
trimmed immediately on per-thread retention changes.

ACK state is stored before broadcast. Retries update one message and ignore old
attempt ACKs; malformed replies never imply success. No-ACK sends have no pending
ACK state. Pending delivery times out as failed after 60 seconds. Deletion and
retention snapshots reach all clients; saved retention is reapplied on reconnect.

App preferences use an app-specific config path and 0600 temporary files, an
exclusive unique temporary name per save, file sync, atomic rename and directory
sync. INI values cannot inject new sections through line breaks. File size and
numeric parsing are bounded. This improves storage durability without claiming
protection from every physical storage failure. Legacy `template-app` preferences
are read if the new file is absent, with RF auto-start disabled until explicitly
re-enabled; subsequent saves use the new app-specific path.

## Publishing and operation

- Attach the correct antenna **before powering the Cap**; software cannot detect it.
- Use the permitted regional band. No RF transmissions were made by these tests.
- ADB, when enabled, exposes an unauthenticated root shell over USB. The app does
  not enable it or require it at runtime; disable it after development.
- Keys, location, contacts and radio state remain sensitive on the device. Do not
  publish its state directory, logs, settings, captures, or backups.
- Published message screenshots use synthetic names, IDs and text. Copyright and
  project-author attribution are intentional public identities, not device data.
- Uninstall leaves the dedicated drop rule in the running kernel to avoid exposing
  an active daemon. After stopping every radio process, an administrator can remove
  it with `sudo /usr/lib/cardputerzero-mesh/firewall-setup.sh remove` before uninstall,
  or let the next reboot clear it after package removal.

See [review-remediation.md](review-remediation.md) for the regression evidence and
remaining device qualification. Code tests are not proof of RF or reboot behavior.
