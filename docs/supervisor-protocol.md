# UI ↔ supervisor protocol

The contract between the **LVGL UI** (C++) and the **Python supervisor** (`supervisor/supervisor.py`). The UI never touches the radio or the Cap; it connects to the supervisor over a **local Unix socket** and exchanges **newline-delimited JSON** — one object per line. This is the extensibility backbone: a new feature is a new intent + event, nothing in the core changes.

- **Socket path:** `$XDG_RUNTIME_DIR/cz-mesh/supervisor.sock` (falls back to `/tmp/cz-mesh-<uid>/supervisor.sock`).
- **Framing:** UTF-8, one compact JSON object per line (`\n`-terminated). No embedded newlines.
- **Direction:** the client sends **intents**; the supervisor sends **events** — both as replies to an intent and asynchronously (e.g. an incoming message). A client may send `"id"` on an intent; the matching reply echoes it.

## Intents (UI → supervisor)

| Intent | Args | Effect |
|---|---|---|
| `status` | — | replies `status` |
| `radio.start` | — | power the Cap, start `meshtasticd`, connect; emits `status` as it progresses |
| `radio.stop` | — | disconnect, stop `meshtasticd`, power the Cap down |
| `node.list` | — | replies `nodes` |
| `channel.list` | — | replies `channels` |
| `channel.add` | `url` | import a channel from a Meshtastic share URL (adds as secondary) |
| `channel.scan` | — | scan a channel QR with the camera (device only), then import it |
| `channel.share` | `channel`, `id` | replies `channelshare` with a share URL for **that indexed channel only** |
| `region.set` | `region` (e.g. `"US"`) | set the LoRa region |
| `owner.set` | `name`, `short?` | set this node's name |
| `config.set` | `path`, `value`, `id`, `channel` when enabling position | set one config field (`lora.modem_preset`, `lora.hop_limit`, `lora.channel_num`, `position.enabled`, `mqtt.enabled`) |
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
| `delivery` | `cid`, `status` (`delivered`\|`failed`), `hops?` — the ack for one of our `message.send`s |
| `device` | `node_id`, `firmware`, `model` — our node's identity |
| `config` | `preset`, `hop`, `position`, `mqtt`, `confirmed`, `frequency_slot`, `id?` — the current LoRa/module config |
| `channelshare` | `channel`, `name`, `url`, `id` — a Meshtastic share URL for one channel (reply to `channel.share`) |
| `traceroute` | `node`, `channel`, `ok`, `path?` (names, you→…→dest) or `text?` (reason on failure) |
| `notice` | `text` — a human status line (e.g. "location sent") |
| `error` | `text`, `intent?` — an intent failed, with a plain reason (the UI shows it as a toast) |

## Rules

- **Intent carries intent, not mechanism.** The UI asks `radio.start`; it never sends a shell command. The supervisor maps each intent to one fixed, validated action (the least-privilege rule in [security.md](security.md)).
- **The supervisor is the source of truth.** The UI renders what it's told via events; it holds no radio state of its own.
- **Async events are broadcast** to every connected client, so the UI stays live without polling.
- **Public and private channels are supported.** All RF actions require an explicit valid enabled channel. DMs choose their channel explicitly; missing or invalid channels never fall back to zero.

QR channel-add: the camera capture + QR decode happen in the supervisor (`channel.scan`); the decoded share URL feeds the same `channel.add` intent. The UI just triggers "scan".

QR channel-share (`channel.share`): the supervisor builds a share URL for **only the indexed channel** (public or private; never all node channels), and the UI renders it as a scannable QR with LVGL's `lv_qrcode`. Channel keys live only on the device; the URL is generated at request time and never written to the repo.

Acks (`wantAck`): `message.send` defaults to `wantAck:true`, so each send gets a `delivery` event (with hop count). Turning acks off in Settings sends `wantAck:false` — less airtime, but no delivery confirmation (the UI shows no pending/▼ state).

## Confirmation, history and bounds

`config.get` performs daemon admin readback and replies `config`. `config.set`
replies only after readback matches the requested effective values. A write/read
failure emits `config.unknown` and `error` (with intent/request ID); clients must
not display an unconfirmed Off. The UI expires pending config after 45 seconds.

`message.clear {thread}`, `message.delete {id}` and `message.retention {cap}`
mutate authoritative history and broadcast a complete `messages` snapshot. A
snapshot replaces local history, including removals. Retention is global server
session policy; each UI reapplies its saved preference on connection (last writer
wins if multiple UIs choose different values). Outgoing message IDs are required;
retrying an ID replaces that message and only the latest attempt's ACK applies.
An empty delivery status means no ACK was requested; delivered/failed are terminal.
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
