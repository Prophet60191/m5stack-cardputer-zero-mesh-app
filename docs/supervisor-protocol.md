# UI ↔ supervisor protocol

The contract between the **LVGL UI** (C++) and the **Python supervisor** (`tools/cardputerzero/supervisor.py`). The UI never touches the radio or the Cap; it connects to the supervisor over a **local Unix socket** and exchanges **newline-delimited JSON** — one object per line. This is the extensibility backbone: a new feature is a new intent + event, nothing in the core changes.

- **Socket path:** `$XDG_RUNTIME_DIR/cz-mesh/supervisor.sock` (falls back to `/tmp/cz-mesh-<uid>.sock`).
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
| `channel.share` | `name?` (default `"backhaul"`) | replies `channelshare` with a share URL for **that one channel only** |
| `region.set` | `region` (e.g. `"US"`) | set the LoRa region |
| `owner.set` | `name`, `short?` | set this node's name |
| `config.set` | `path`, `value` | set one config field (`lora.modem_preset`, `lora.hop_limit`, `position.enabled`, `mqtt.enabled`) |
| `message.list` | `channel?` | replies `messages` (optionally filtered to one channel index) |
| `message.send` | `channel`, `text`, `dest?`, `cid?`, `wantAck?` (default `true`) | send a text message; with `wantAck` an ack drives a `delivery` event keyed by `cid` |
| `position.send` | `channel`, `dest?` | send this node's GPS position as a visible message, or reply `error` if there is no fix |
| `position.request` | `node` | ask a node to report its position (updates `nodes` when it replies) |
| `traceroute` | `node` | trace the mesh path to a node; replies `traceroute` with the path or a timeout |

## Events (supervisor → UI)

| Event | Fields |
|---|---|
| `status` | `radio` (`off`\|`starting`\|`online`\|`error`), `region`, `owner`, `port?`, `detail?` |
| `nodes` | `nodes`: `[{id, name, short, snr, lastHeard, hops, self, battery, voltage, lat, lon}]` |
| `channels` | `channels`: `[{index, name, role}]` (role: `PRIMARY`\|`SECONDARY`) |
| `messages` | `messages`: `[{from, fromName, to, channel, text, id, time, outgoing, status, snr?, hops?}]` (a snapshot) |
| `message` | `message`: one `{…}` — pushed asynchronously when a text arrives (received carry `snr`/`hops`) |
| `delivery` | `cid`, `status` (`delivered`\|`failed`), `hops?` — the ack for one of our `message.send`s |
| `device` | `node_id`, `firmware`, `model` — our node's identity |
| `config` | `preset`, `hop`, `position`, `mqtt` — the current LoRa/module config |
| `channelshare` | `name`, `url` — a Meshtastic share URL for one channel (reply to `channel.share`) |
| `traceroute` | `node`, `ok`, `path?` (names, you→…→dest) or `text?` (reason on failure) |
| `notice` | `text` — a human status line (e.g. "location sent") |
| `error` | `text`, `intent?` — an intent failed, with a plain reason (the UI shows it as a toast) |

## Rules

- **Intent carries intent, not mechanism.** The UI asks `radio.start`; it never sends a shell command. The supervisor maps each intent to one fixed, validated action (the least-privilege rule in [publishing-and-trust.md](publishing-and-trust.md)).
- **The supervisor is the source of truth.** The UI renders what it's told via events; it holds no radio state of its own.
- **Async events are broadcast** to every connected client, so the UI stays live without polling.
- **`backhaul` only for our own traffic** — the protocol allows any channel index, but the app sends on the private channel, never the public default.

QR channel-add: the camera capture + QR decode happen in the supervisor (`channel.scan`); the decoded share URL feeds the same `channel.add` intent. The UI just triggers "scan".

QR channel-share (`channel.share`): the supervisor builds a share URL for **only the named channel** (never the whole node — the public primary is deliberately excluded), and the UI renders it as a scannable QR with LVGL's `lv_qrcode`. Channel keys live only on the device; the URL is generated at request time and never written to the repo.

Acks (`wantAck`): `message.send` defaults to `wantAck:true`, so each send gets a `delivery` event (with hop count). Turning acks off in Settings sends `wantAck:false` — less airtime, but no delivery confirmation (the UI shows no pending/▼ state).
