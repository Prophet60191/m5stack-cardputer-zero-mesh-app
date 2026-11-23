# UI ↔ supervisor protocol

The contract between the **LVGL UI** (C++) and the **Python supervisor** (`supervisor/supervisor.py`). The UI never touches the radio or the Cap; it connects to the supervisor over a **local Unix socket** and exchanges **newline-delimited JSON** — one object per line. This explicit interface supports independent presentation/resource-owner development. New features still require their validation, state, lifecycle and tests to be designed; adding an intent/event is not automatic isolation or a general plugin mechanism. See [why Mesh uses this modular design](architecture.md).

- **Socket path:** `$XDG_RUNTIME_DIR/cz-mesh/supervisor.sock` (falls back to `/tmp/cz-mesh-<uid>/supervisor.sock`).
- **Framing:** UTF-8, one compact JSON object per line (`\n`-terminated). No embedded newlines.
- **Direction:** the client sends **intents**; the supervisor sends **events** — both as replies to an intent and asynchronously (e.g. an incoming message). Direct replies echo the intent's optional `"id"`; state-update broadcasts do not. Use `state.sync` when a persistence barrier is needed after a broadcast-only operation.

## Intents (UI → supervisor)

| Intent | Args | Effect |
|---|---|---|
| `status` | — | replies `status` |
| `radio.start` | — | power the Cap, start `meshtasticd`, connect; emits `status` as it progresses |
| `radio.stop` | — | disconnect, stop `meshtasticd`, power the Cap down |
| `node.list` | — | replies `nodes` |
| `channel.list` | — | replies `channels` |
| `channel.preview` | `url`, `id` | validate and preview pasted share data without writing channels |
| `channel.add` | `token`, `confirmed:true`, `id?` | import the connection-bound preview as secondary channels after state/expiry validation |
| `channel.scan` | `id` | start one cancellable background camera job; replies `channel.preview` or `error`, never imports |
| `channel.import.cancel` | `request` | cancel this connection's scan/discard preview for that original request; no RF or channel writes |
| `channel.share` | `channel`, `id` | replies `channelshare` with a share URL for **that indexed channel only** |
| `region.set` | `region` (e.g. `"US"`) | set the LoRa region |
| `owner.set` | `name`, `short?` | set this node's name |
| `config.set` | `path`, `value`, `id`, `channel` when enabling position | set one config field (`lora.modem_preset`, `lora.hop_limit`, `lora.channel_num`, `position.gps_mode`, `position.enabled`, `mqtt.enabled`) |
| `message.list` | `channel?` | replies `messages` (optionally filtered to one channel index) |
| `message.send` | `channel`, `text`, `dest?`, `cid`, `wantAck?` (default `true`) | send a text message; with `wantAck` an ack drives a `delivery` event keyed by `cid` |
| `position.send` | `channel`, `dest?` | send this node's GPS position as a visible message, or reply `error` if there is no fix |
| `position.request` | `node`, `channel` | ask a node to report its position (updates `nodes` when it replies) |
| `traceroute` | `node`, `channel` | trace the mesh path to a node; replies `traceroute` with the path or a timeout |

## Events (supervisor → UI)

| Event | Fields |
|---|---|
| `status` | `radio` (`off`\|`starting`\|`online`\|`error`), `region`, `owner`, `port?`, `detail?` |
| `nodes` | `nodes`: `[{id, name, short, snr, lastHeard, hops, self, battery, voltage, lat, lon}]` |
| `channels` | `channels`: `[{index, name, role, private, position_precision}]` (role: `PRIMARY`\|`SECONDARY`) |
| `messages` | `messages`: `[{from, fromName, to, channel, text, id, time, outgoing, status, snr?, hops?}]` (a snapshot) |
| `message` | `message`: one `{…}` — pushed asynchronously when a text arrives (received carry `snr`/`hops`) |
| `delivery` | `cid`, `status` (`sent`\|`mesh`\|`delivered`\|`failed`), `hops?` — the ack for one of our `message.send`s |
| `device` | `node_id`, `firmware`, `model` — our node's identity |
| `config` | `preset`, `hop`, `position`, `gps_mode`, `mqtt`, `confirmed`, `frequency_slot`, `tx_enabled`, `id?` — the current LoRa/module config |
| `channel.preview` | `id`, `token`, `new`, `available`, `channels:[{name,private,existing}]`, `region`, `preset`, `slot`; no URL/key; `new:0` means already present |
| `channel.imported` | `added`, `id?` — confirmed imported channel count; completion also emits a nonsecret notice |
| `channelshare` | `channel`, `channel_id`, `name`, `url`, `id` — a Meshtastic share URL for one channel (reply to `channel.share`) |
| `traceroute` | `node`, `channel`, `ok`, `path?` (names, you→…→dest) or `text?` (reason on failure) |
| `notice` | `text` — a human status line (e.g. "location sent") |
| `error` | `text`, `intent?` — an intent failed, with a plain reason (the UI shows it as a toast) |

## Rules

- **Intent carries intent, not mechanism.** The UI asks `radio.start`; it never sends a shell command. The supervisor maps each intent to one fixed, validated action (the least-privilege rule in [security.md](security.md)).
- **The supervisor is the source of truth.** The UI renders what it's told via events; it holds no radio state of its own.
- **Async events are broadcast** to every connected client, so the UI stays live without polling.
- **Public and private channels are supported.** All RF actions require an explicit valid enabled channel. DMs choose their channel explicitly; missing or invalid channels never fall back to zero.

QR channel-add opens ready instructions with the Online radio requirement. Return
starts capture and immediately shows Scanning; repeated Return is ignored. Escape
cancels the process group and discards any preview. A decoded or pasted URL
produces a preview of channel names/classifications before a Cancel-first import
confirmation. No channels change until `channel.add` with the preview token and
`confirmed:true`. Tokens are connection-bound, expire after 120 seconds, and are
invalid after channel/tuning changes. Already-present channels report no change.
Committed imports finish under the radio worker and broadcast a completion notice
even if the view was left. Capture and decoded secrets are never saved to disk.

QR channel-share (`channel.share`): the supervisor builds a share URL for **only the indexed channel** (public or private; never all node channels), and the UI renders it as a scannable QR with LVGL's `lv_qrcode`. Channel keys live only on the device; the URL is generated at request time and never written to the repo.

Acks (`wantAck`): `message.send` defaults to `wantAck:true`, so each send gets a `delivery` event (with hop count). Turning acks off sends `wantAck:false`; the UI labels the result Sent (unconfirmed).

## Confirmation, history and bounds

`config.get` performs daemon admin readback and replies `config`. `config.set`
replies only after readback matches the requested effective values. A write/read
failure emits `config.unknown` and `error` (with intent/request ID); clients must
not display an unconfirmed Off. The UI expires pending config after 45 seconds.

`message.clear {thread}`, `message.delete {message_id}` and `message.retention {cap,silent?}`
mutate authoritative history and broadcast a complete `messages` snapshot. A
snapshot replaces local history, including removals. Retention is global server
session policy; each UI reapplies its saved preference on connection (last writer
wins if multiple UIs choose different values). On startup/reconnect, the UI sends
`silent:true`: the policy still applies and trims/broadcasts when changed, but the
reply is `message.retention {cap}` rather than a user-facing notice. Omitted or
false `silent` preserves the explicit-setting confirmation notice. The optional
field must be a boolean; errors are still reported normally, and request IDs are
echoed in either reply. Outgoing message IDs are required;
retrying an ID replaces that message and only the latest attempt's ACK applies.
Outgoing status is `sending`, `sent` (no acknowledgment requested), `mesh` (mesh ACK), `delivered` (ACK source matches the DM recipient) or `failed`. These are transport results, never read receipts. `reason` preserves the failure or uncertainty.
Pending delivery fails after 60 seconds. `error` includes `cid` for failed sends.

Channel share replies must match **both** the selected channel and request ID;
dismissing the pending view cancels acceptance of late replies. URLs remain secret.

See [security.md](security.md) for exact frame/queue/work/history bounds and the
same-UID trust model. The supervisor never accepts an executable or shell string.

## Public channels and location consent

`channels[].private` classifies the channel key, not per-message DM encryption.
The UI labels both types and includes all enabled channels in its pickers.
`position.send` on a public channel requires `public_confirmed:true`; it sends
precise coordinates as text. Enabling automatic public location via `config.set`
also requires this boolean and explicit `channel`; precision is capped at 15 bits.
Only successful readback persists consent. An unmatched public position profile
is disabled on disk before startup. Off zeroes precision on all enabled channels.
A failed position setting stops the radio and emits Unknown/error.

`lora.channel_num` accepts integer 0..255 (0 = Auto); actual configuration replies
include `frequency_slot`. All channels share this slot, preset and region.
Imports accept public/unnamed or private channels, reset location/MQTT forwarding
and leave current radio tuning unchanged even when the URL carries LoRa settings.

## 0.1.2 identities, storage and application services

Upgrade UI and supervisor together. Every operation that targets an existing
channel includes `channel_id` from `channels[].identity`, in addition to its
integer `channel`. This includes messages, position sends/requests, traces, QR
sharing and enabling automatic location. A stale/missing identity is refused
before RF. Each identity is an opaque local HMAC of effective name, normalized
key and any explicit nonzero channel ID; slot/role changes preserve it while
key/name/explicit-ID changes create an archived thread.
The HMAC secret and channel keys never enter ordinary events. A broadcast thread
is `ch:<identity>`; old messages without identity are isolated as `ch:legacy:<slot>`
and cannot send. DMs still require explicit channel selection; their drafts
add `@<channel identity>` to the peer thread key. No numeric parsing of opaque
thread IDs is permitted.

Messages carry `channel_id`, saved `channel_name`, `channel_private`, and optional
`reason`. Nodes add `position_time`, `position_age`, `satellites`,
`channel_utilization` and `air_utilization`. Status adds `mode` (`off`,
`receive-only`, `transmit`) and `last_rx`. Node/telemetry updates are coalesced;
status refreshes every two seconds, node ages at least every ten seconds.
Fresh GPS coordinates must be finite/in range with age at most 120 seconds;
unknown/future/stale fixes cannot be sent silently. The UI waits for a fresh fix.
`config.set` with `position.gps_mode` accepts `ENABLED` or `DISABLED` and controls
the receiver independently of location-sharing consent. Diagnostics include
`position_valid` and `position_fresh`; a timestamp alone is not a position fix.

| Intent | Args and result |
|---|---|
| `setup.status` | `setup`: profile readiness, region/mode, dependency versions and firewall health; greeting includes it |
| `setup.initialize` | `region`, `kind` public/private, private `name` (1..11 UTF-8 bytes), `confirmed:true`; refuses existing preferences and initializes TX/location/MQTT Off |
| `radio.tx` | strict boolean `enabled`; enabling also requires `antenna_confirmed:true`; actual readback online, durable profile update offline |
| `channel.slots` | all slots including disabled/configured state, role, classification and opaque identity; no keys |
| `channel.manage` | `confirmed:true`, `action` create/rename/rotate/disable/remove/enable; existing slot requires channel/identity, create requires kind/name; actual admin readback |
| `profile.list` | `profiles` summaries with opaque `id`, name, region/preset/slot/count; max 16 |
| `profile.save` | printable `name` (1..48 UTF-8 bytes); save current radio/channel settings privately |
| `profile.preview` | `profile` ID; summary and channel names/classifications, no keys |
| `profile.apply` | profile ID plus `confirmed:true`, radio must be Off; TX/location/MQTT forced Off; never starts RF |
| `profile.delete` | profile ID plus `confirmed:true`; never accepts a filesystem path |
| `diagnostics.get/export` | whitelisted health/version/counts, no chats/keys/IDs/coordinates; export writes fixed private `diagnostics-export.json` in the radio state directory |
| `thread.state` | `thread`, `state:{draft,pinned}`; draft at most 200 UTF-8 bytes, max 256 saved states; broadcasts `threads` |
| `thread.delete` | forget thread state; use `message.clear` separately for messages |
| `history.set` | strict boolean `enabled`; disabling deletes saved messages/drafts/conversation identities/pins and retains RAM; enabling saves the current RAM messages/drafts/pins and future updates; replies and broadcasts `threads` |

Greeting always includes history and `threads`, even with the radio Off.
History/drafts live in private bounded SQLite storage in the radio state directory;
there is no replay queue. After a restart, pending sends become failed with an
explicit interrupted-acknowledgment reason. Draft writes are debounced; a process
crash can lose the last 650 ms of edits. Confirmed supervisor acceptance clears the
matching draft; rejected sends preserve it. Quiet hours are UI preferences in
its private INI, using device local time and supporting windows across midnight.

Changes to region, modem preset, frequency slot or GPS receiver mode use a planned stop/persist/start
cycle, followed by actual readback. Firmware-initiated tuning reboots are not
misclassified as unexpected radio loss. Other disconnections preserve the actual
failure reason and power down through the guardian.

`state.sync {id}` is a serialized persistence barrier and replies `state.synced`. The UI flushes drafts before graceful exit and waits up to two seconds for this barrier; an unavailable/busy supervisor can still prevent the final save.

Channel share replies validate the original opaque identity as well as request/slot; changing the active channel closes an already visible QR. Automatic-location pickers retain the identity shown when opened and reject changed slots at either confirmation step. Retries resolve their original identity after a slot move instead of targeting a replacement. Draft-only, pinned and cleared conversations remain in the list independently of saved messages; deleting a conversation removes its saved state too.


Local `thread.state`, `thread.delete` and `history.set` operations are serialized
under the state lock rather than queued behind camera/admin work. `state.sync`
still waits for all preceding radio operations and local commits from the stream.
Greetings send one history snapshot; the UI does not request a duplicate. Changing
retention can legitimately send a newly trimmed snapshot. Daemon version queries
are cached for 60 seconds and greetings warm that cache outside the lifecycle
lock; each radio start forces a fresh compatibility query. Firewall verdicts stay
fresh on every check. Status uses a captured interface and reports Unknown if the
library node is incomplete. Message targets use `message_id`; `id` is exclusively
request correlation. Generic UI service waits expire after 60 seconds (90 for a
confirmed import), report uncertainty, and ignore late replies after dismissal.


Unchanged `thread.state` updates are idempotent and need not broadcast another
`threads` snapshot. Validation still runs before comparing state. Use `state.sync`
for an explicit persistence barrier rather than waiting for an unchanged echo.

Since 0.1.12, `thread.state` may include an `id` to request a small
`thread.saved {id, thread, durable}` acceptance reply, including for an unchanged
save. `durable:true` means the private store accepted the state with disk history
enabled; `false` means RAM-only acceptance under the current opt-out preference.
It is unrelated to RF delivery. Requests without an ID retain their previous
behavior. Unchanged requests cause no disk write or full-state broadcast.

The UI keeps local draft/pin changes protected until the matching save is
accepted. A newer edit, deletion or connection change invalidates an older
request; stale replies cannot clear newer pending work. Pending local state is
retried at a bounded rate, with validation/storage failures retained locally and
reported instead of repeatedly retried. A new edit or connection permits retry.
Snapshots from other clients update confirmed state; still-pending local edits
remain protected and the last accepted write is authoritative. There is no
cross-client editing/merge protocol and no RF replay. Upgrade/restart the
supervisor with the UI so the new acceptance reply is supported.

## Unified channel setup (0.1.13)

`channel.setup.context {id}` reads the authoritative setup (also while Off),
returning a nonsecret `base` fingerprint, tuning, current TX/radio state and
capacity. `channel.preview` / `channel.scan` accept optional `setup:true` to
prepare an import without powering the radio; that connection's imported URL
stays private in the supervisor for up to ten minutes. Supported QR tuning is
a separate `suggested_tuning`, never implicitly adopted. Fixed frequencies and
device calibration are not silently imported. The legacy import path is unchanged.

`channel.setup.preview {id,base,source,name?,import_token?,tuning,tx,position,mqtt,
profile_only,profile_name,position_target?}` validates a complete local draft and returns
`channel.setup.preview {id,token,channels,tuning,tx,position,mqtt,profile_only,
profile_name,tuning_changed,tx_confirmation,public_position,position_channel,warnings}`. Source is
public/private/import; generated keys stay in the supervisor. Tuning contains
region/preset/use_preset/slot/hop and an existing override_frequency or zero to
clear it. Calibration is preserved. Keys/URLs are absent from replies.

A preview is connection/request-bound, lasts two minutes and checks the entire
current setup fingerprint again before writing. Cancellation, replaced requests,
disconnect, changed setup and missing consent reject the commit.
`channel.setup.cancel {request?}` cancels that connection's pending review,
including queued preparation; it cannot undo a commit already performing writes.

`channel.setup.commit {id,token,confirmed:true,tuning_confirmed?,antenna_confirmed?,
public_position_confirmed?,mqtt_confirmed?}` consumes the review once. Tuning
changes need their own acknowledgment; enabling TX or retuning an enabled TX
needs antenna/region confirmation; public location and MQTT effects need explicit
consent. A separate-profile-only save requires a name and RX/location/MQTT Off;
it does not stop or change the active setup. Active saves persist the complete
configuration/channel/module pair, restart once if already Online and compare
actual readback. Offline saves stay Off and verify stored bytes only. Failures
stop the radio and attempt restoring the prior pair/consent; failed recovery is
reported explicitly and does not restart the radio.

Success returns `channel.setup.saved {id,added,verified,profile_only,profile,
profile_error?,channels:[{index,identity}]}`. Verified is `radio` or `stored`, not
a delivery/connectivity guarantee. An optional profile failure after a confirmed
channel save is explicit partial success. An unknown/timed-out result must be
inspected before trying a fresh setup; no automatic configuration/RF replay.
There are at most eight setup plans, each based on at most 128 KiB of preferences.
No arbitrary path, command, executable or shell argument is accepted.

For multi-channel imports, `position_target` is the zero-based ordinal among
newly added channels (default 0), validated against the token-bound import order.
Only that channel gets automatic precision; its actual privacy classification
determines public-location consent and precision.


### Form Save presentation (0.1.14)

The native channel form is its own user review. Its persistent `1: Save channel`
(or Save profile) action runs `channel.setup.preview` as private save validation;
a normal valid save then sends one correlated commit without a second visible
review page. Only changed active tuning, TX/antenna, public location or MQTT
requires specific consent. Commit flags represent the actual accepted choices.
Owner-side validation, connection binding, expiry, stale-state checks and
recovery remain mandatory; removing a UI page does not remove them. A numeric
shortcut stays text while composing and cannot replay a busy write.

## Optional phone access

`phone.get` returns `event:phone` with `enabled`, `connected`, `ready`, `peer`,
`peers`, `endpoint` and a human-readable `detail`. `peer` is the selected stable
Tailscale node ID; each peer contains `id`, display `name`, `online` and VPN
`addresses`. These are private runtime data, not sanitized diagnostics.
`phone.select` requires `peer` from the current VPN peer list and disables access.
`phone.set` requires a boolean `enabled`; enabling requires a valid selected peer
and private Serve PROXY-v2 setup. Both reply with the current phone payload and
normal request correlation. Neither starts RF nor changes TX/privacy/configuration.
See [Phone access](phone-access.md) for the separate standard protobuf endpoint.
