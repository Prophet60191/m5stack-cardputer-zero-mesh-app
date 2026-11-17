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
package enables a boot service to establish it. That monitor service runs
as `cz-mesh-firewall` with only CAP_NET_ADMIN; the app and supervisor run as the
logged-in user. A root-owned helper accepts only fixed `ensure`, `check`, `monitor`, and
`remove` actions. The app installs no sudo grant. `firewall.py health` reads the
trusted verdict without elevated privileges. Startup
fails before powering hardware if the rule is absent/changed or its trusted health record
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
set and available capacity before writing; deduplicate by name, key and explicit channel ID. Imports
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

Native keyboard readers separately limit each device poll chain to 128 raw read
attempts, including ignored/modifier/repeat records. Unread events resume on the
next poll so a sustained stream cannot make that chain drain indefinitely.
Pending key holds are tied to their source device and canceled on owned cleanup,
without synthetic short-click actions or canceling an unrelated device's hold.

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
- Use the permitted regional band. Offline regressions never transmit; deliberate
  private-peer hardware messages are recorded separately in the verification log.
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

## 0.1.2 operational safeguards

The app no longer runs a sudo health check every two seconds. A separate system
service runs as `cz-mesh-firewall`, with only CAP_NET_ADMIN, and checks its exact
nftables table. It atomically publishes a verdict in its owned runtime directory.
The supervisor validates owner/type/mode, boot identity, true verdict and an age
of at most seven seconds before power-on and while running. Stale/missing/false
verdicts stop the radio. As of 0.1.6, the obsolete passwordless sudo check grant
is removed; `firewall.py health` is the unprivileged manual health command.
The normal supervisor user service uses NoNewPrivileges.

This addresses an observed device failure: `subprocess.run` timed out while
waiting for sudo, then attempted to kill an elevated process it could not signal.
That generated EPERM and a generic radio Error even though a subsequent firewall
check succeeded. A boot capability service removes that privileged child-process
lifecycle from the application; health verification still fails closed.

Fresh setup generates a private key locally or selects the known public default,
with TX, automatic location and MQTT Off. Explicit TX enabling requires antenna
confirmation; software cannot detect the antenna. Saved mesh profiles contain
keys and are private local files, never diagnostic exports. Applying a profile
requires radio Off and resets TX/location/MQTT consent. The supported daemon/client
versions are checked before Cap power, because a schema mismatch can make firmware
replace stored preferences with factory defaults.

Conversation storage is 0600 SQLite in the owned 0700 state directory, with WAL,
FULL synchronous commits and a 2,000-message global bound. Drafts/pins have a
256-thread bound. It is not encrypted at rest; a user with device/root access can
read it. Disabling saved history deletes disk messages, drafts, conversation identities
and pins, checkpoints the SQLite WAL, and suppresses future writes; RAM history remains until shutdown. Deletion is logical, not a
promise of forensic erasure from flash, WAL, filesystem snapshots or backups.
Re-enabling saved history saves the current RAM messages, drafts and pins as well as
future updates; the confirmation describes saving the current session.
No message is automatically resent after restart.

Opaque local channel identities bind routing to effective name/key and any
explicit nonzero channel ID. Reused slots archive old conversations and cannot silently retarget retries,
drafts or an open DM selection. Diagnostic export is built from an explicit
whitelist and excludes raw logs, channel/owner names, IDs, keys, URLs, coordinates,
message text and filesystem paths. Do not substitute raw runtime logs for it.


Channel imports (0.1.6) require a preview and explicit confirmation. Camera work
has one cancellable background job and never writes channels. Escape, disconnect
or screen teardown cancels capture/discards the connection-bound preview. Tokens
expire after 120 seconds and bind the exact current channels and LoRa settings;
changed state requires another preview. URLs/keys are bounded and kept only in
RAM, never returned in preview events. Confirmed imports remain serialized, with
actual readback; their completion is announced even if their view was dismissed.
Local draft/history writes use the state lock independently of radio admin waits.


## Store-compatible user service (0.1.9)

The package launches the supervisor as an on-demand **transient user service**,
using fixed `systemd-run --user` arguments in `user_service.py`. It installs no
static user unit or generated privileged files. The UI wrapper is named
`cardputerzero-mesh-launch`; the visible application identity is unchanged.
The supervisor still has its own control group, KillMode=control-group,
NoNewPrivileges, UMask=0077, failure restart and guardian ExecStopPost recovery.
It continues receiving when the UI exits. A second UI launch leaves an active
supervisor alone. Restart/stop are explicit user operations and stop the radio;
service-control arguments never come from an intent or a socket peer.

An owned private runtime lock serializes lifecycle operations and refuses
symlink/hardlink or permissive lock files/directories. The helper refuses root.
Upgrade requires a completed stop, replacement of files, then a fresh start;
check the reported live version/PID and settings afterward. The first 0.1.9
upgrade removes the old package-owned wrapper and static unit, rather than
leaving aliases, configuration links or installer-created hidden units.
Store validation of the archive does not validate dynamically created service
properties; these are covered by dedicated tests and a separate device probe.

## Local conversation acceptance (0.1.12)

Pending drafts/pins are retained until correlated supervisor acceptance, including
unchanged saves; transport enqueue alone is not persistence. Acceptance identifies
RAM-only history opt-out versus durable storage. Retry is bounded and limited to
local conversation state, never RF sends. Multiple clients use last accepted
write wins, with pending edits protected from older snapshots/results; this is
not collaborative text merging. Graceful exit waits within a shared two-second
save/sync deadline and reports unconfirmed state in the local log. See the
[protocol](supervisor-protocol.md) and [maintenance evidence](maintenance-pass.md).

## Unified channel setup (0.1.13)

The supervisor owns authoritative radio state and decoded/generated keys in
connection-bound plans. A pasted share URL necessarily contains key material
in UI memory as well as the supervisor; it is masked while editing and never
logged. Masking does not encrypt or securely erase process memory. Eight-plan and 128 KiB snapshot bounds apply;
expired/replaced/cancelled/disconnected/stale reviews cannot commit. Keys and URLs
never appear in plan/result payloads. Same-user socket access remains a control
trust boundary, not isolation from other programs owned by that user.

A full review is required before persistence. Global tuning/TX and public
location/MQTT consequences are explicit. Existing TX is preserved unless chosen
otherwise; new forwarding/location start Off. Only the newly selected position
channel receives precision when sharing is enabled, with existing public consent
validated and new public precision capped. Profile-only changes never alter the
running setup. Failed active application stops RF and attempts restoration; a
power interruption can leave the existing roll-forward recovery transaction,
and startup still removes unapproved public location before power. Software
cannot guarantee recovery from failed storage/hardware or power loss. Stored
verification is distinguished from live readback and from actual peer delivery.
