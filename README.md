<p align="center">
  <img src="docs/screenshots/icon.png" alt="M5Stack Cardputer Zero Mesh App icon" width="96">
</p>

<h1 align="center">M5Stack Cardputer Zero Mesh App</h1>

<p align="center">
  <b>A Meshtastic&reg; client for the M5Stack CardputerZero</b><br>
  A native LVGL messaging app + a Python radio supervisor, built to be robust enough to trust.
</p>

<p align="center">
  <img src="docs/screenshots/carousel.png" alt="Home carousel" width="320">
</p>

<p align="center">
  <img src="https://img.shields.io/badge/device-M5Stack%20CardputerZero-red?style=flat-square" alt="device">
  <img src="https://img.shields.io/badge/radio-Meshtastic%20%7C%20LoRa%20SX1262-2A9FD6?style=flat-square" alt="radio">
  <img src="https://img.shields.io/badge/UI-C%2B%2B%20%2B%20LVGL%209.5-064F8C?style=flat-square" alt="ui">
  <img src="https://img.shields.io/badge/app-MIT-green?style=flat-square" alt="license">
  <img src="https://img.shields.io/badge/supervisor-GPL--3.0-blue?style=flat-square" alt="supervisor license">
</p>

---

## What this is

The **M5Stack CardputerZero** is a credit-card-sized Debian Linux computer (Raspberry Pi CM0) with its own
keyboard, screen, battery and a **LoRa SX1262 "Cap"** radio. This project turns it into a real
**[Meshtastic](https://meshtastic.org) handheld** — send and receive text on public or private channels, see who's
out there, and share a channel by QR — with an interface designed for the tiny 320&times;170 screen and the
46-key keyboard.

Messages now separates **Channels** and **Direct Messages** (Tab switches),
sorts each by recent activity, and searches both in one newest-first result list.
Long histories load in small windows while retaining all saved messages.
See [Messages navigation and loading](docs/messages-navigation.md).

The current source is **0.1.22, a release candidate** with public/private channel
support, clearer delivery results, grouped setup forms, readable values and
Cancel-first deletion. The header shows battery percentage without a battery
icon. The [latest status](docs/current-status.md) separates current changes from
previous hardware qualification. The [UI checkup](docs/ui-checkup.md) records
layout, navigation, notification, QR and editor corrections. The [GPS record](docs/internal-gps-validation.md)
covers outdoor fixes, the Map marker and the native-radio startup correction,
now included in the 0.1.19 package. [Phone access](docs/phone-access-validation.md) covers the official
iPhone app, cellular messaging and reconnect limits. Live public-mesh delivery
and other remaining checks stay explicit in the [verification record](docs/review-remediation.md).

> **Meshtastic&reg; is a registered trademark of Meshtastic LLC.** This is an independent, community project and
> is not affiliated with or endorsed by Meshtastic LLC or M5Stack.

Store submission is still preparation work. The [current readiness report](docs/store-submission-readiness-20261123.md)
records the resolved code/dependency work, passed installation checks and remaining
video/publication steps. The fresh-image hardware check is explicitly deferred.
Passing package checks is not Store acceptance.

## Screenshots

These are native LVGL renders using synthetic data, not personal messages or device identities.

| Messages | Nodes | Map | Settings |
|:--:|:--:|:--:|:--:|
| ![Messages](docs/screenshots/messages.png) | ![Nodes](docs/screenshots/nodes.png) | ![Map](docs/screenshots/map.png) | ![Settings](docs/screenshots/settings.png) |

## User guide & quick reference

**[Read the illustrated user guide](docs/user-guide/README.md)** or go straight to
the **[Quick Reference Card](docs/user-guide/00-Quick-Reference-Card.md)**.
The guide covers first setup, keyboard controls, messages, nodes, the map,
settings and troubleshooting. [Versioned PDF downloads](docs/user-guide/README.md#offline-downloads)
are available for offline use and printing.

## Features

- **Compact keyboard UI** — fixed conversation/channel header, complete footer help and grouped Settings with Radio first.

- **Public and private channels** — clearly labeled group conversations and explicit DM channel selection; no silent channel fallback.

- **Threaded messaging** — a conversation per channel and per node, with mesh versus recipient acknowledgments, unconfirmed sends and failure reasons and **resend** for the ones the mesh drops.
- **First-run setup** — select a region and public/private primary channel; initialize receive-only, then explicitly enable TX with antenna confirmation. Uses the bundled qualified meshtasticd 2.7.26 build and client 2.7.11.
- **Durable chats** — bounded private SQLite history, identity-bound drafts, pinned chats, English case-insensitive conversation search (`/`), and local-time quiet hours. Saved history can be disabled and erased.
- **Complete channel setup** — scan/paste/create in one form with tuning, privacy, optional named profile, grouped fields and verified Save. Prepare while Off or save a separate profile without retuning; open the resulting conversation directly.
- **Manage channels and saved mesh setups** — create/rename/rotate/enable/disable/remove channels with actual readback; preview and apply named local profiles while Off. Profile application leaves TX, location and MQTT Off.
- **Diagnostics** — current connection, firewall, versions and GPS age; optional sanitized export excludes keys, chats, IDs and coordinates.
- **Quick messages** — one-key canned replies ("OK", "On my way", "Need help"&hellip;), editable in Settings.
- **Nodes & Map** — who's on the mesh, signal, battery, last-heard, distance; a you-centred radar map.
- **Mesh-native tools** — share your **location**, request a node's position, **trace the route** a packet
  takes, and **share a channel as a QR code** (only the selected channel, public or private).
- **Per-thread controls** — mute, mark-read, clear history, delete a conversation; an unread badge and a
  notification toast when a message arrives while you're elsewhere.
- **Settings that persist** — region, modem preset, frequency slot, hop limit, add-a-channel (QR or paste), node name,
  delivery-ack and retention preferences, theme — all saved across restarts.
- **Keyboard-first** — the bare **F / X / Z / C** keys are the d-pad everywhere except while typing; hold
  **Fn** for arrows inside a text box. Press **HELP** on any screen for its keys.

## Build your own

Want to build an app for this device? This repo is the worked example for a full guide:
**[Building Apps for the M5Stack Cardputer Zero](https://github.com/Prophet60191/m5stack-cardputer-zero-app-guide)**
— the platform, architecture choices, the keyboard, packaging, and publishing.

## Architecture

Two cooperating processes with a clean boundary — the UI never touches the radio:

We chose modularity so the interface, radio authority and resource lifecycle can
evolve and be tested independently, while reception continues after the UI closes.
This also lets the native UI use the existing Meshtastic Python client through a
documented interface. The [architecture decision](docs/architecture.md) explains
the purpose, benefits, costs and relationship to the official components; a
separate supervisor is a Mesh choice rather than a requirement for every app.

```
  ┌────────────────────┐   newline-JSON    ┌──────────────────────┐   TCP (loopback)   ┌────────────┐
  │  C++ / LVGL app     │  intents  ───────▶│  Python supervisor   │ ─────────────────▶ │ meshtasticd │──▶ LoRa Cap
  │  (what you see)     │◀─────────  events │  (owns the radio)    │   meshtastic lib   │  (daemon)   │    (SX1262)
  └────────────────────┘   Unix socket     └──────────────────────┘                    └────────────┘
```

- The **app** (MIT) renders screens and sends **intents** (`radio.start`, `message.send`, `traceroute`&hellip;)
  over a local Unix socket; it holds no radio state of its own.
- The **supervisor** (GPL-3.0, because it uses the GPL `meshtastic` library) powers the Cap, runs `meshtasticd`
  through a guardian, with TCP **firewall-confined to loopback**, and translates intents into radio actions. The full contract is in
  [docs/supervisor-protocol.md](docs/supervisor-protocol.md).

## Connect the official Meshtastic phone app

Optional **[Phone access](docs/phone-access.md)** lets the selected phone view
mesh information and send messages alongside the Cardputer UI over Tailscale.
It requires a separately installed/signed-in VPN and private Serve forwarder;
radio/channel changes stay on the Cardputer. Access defaults Off and never changes
TX or location settings. The daemon API remains confined to loopback.

## Hardware you need

- An **M5Stack CardputerZero** running its Debian image.
- The **Cap LoRa-1262** (SX1262 + antenna). **Attach the antenna before powering the radio** — transmitting
  without it can permanently damage the chip.
- A Meshtastic mesh to talk to (another node, e.g. a T-Deck), on a region you're licensed for.

## Install & run

**Start with the [complete installation and recovery instructions](docs/install.md).**
They cover obtaining/verifying the candidate, transferring it, first launch,
upgrades, private backups, rollback, troubleshooting and removal.

0.1.22 is a **candidate**, not a public release or Store listing. Its
[source CI run](https://github.com/Prophet60191/m5stack-cardputer-zero-mesh-app/actions/runs/45500604820)
and exact installed package are distinguished in the installation guide and
[UI qualification record](docs/ui-checkup.md). Older published
candidates use the earlier dependency arrangement; do not mix their instructions.

On the supported Debian 13 arm64 image, with the verified installer on the device:

```sh
sudo apt install ./cardputerzero-mesh_0.1.22-rev1_arm64.deb
```

The one installer includes the GPS-corrected private native radio and 15 pinned
Python wheels. **No separate meshtasticd installation or manual pip setup.**
APT may download missing standard OS libraries. First launch prepares a private
user runtime offline; later launches reuse it. Legacy environments and saved
chats/channels/profiles/settings are preserved.

For an upgrade, turn Radio Off, close Mesh and back up first. Installation stops
this app's old supervisor before replacing files and never starts RF. A separate
global meshtasticd, if already installed, must stay stopped/masked. Run Mesh from
the normal launcher as your regular user, never with sudo. Attach the antenna
before Radio On. New first-run setups begin receive-only; existing settings stay
as saved. Choose transmission and location sharing deliberately.

Closing the UI leaves an enabled radio receiving in the background. Stop Radio
before removing/swapping the Cap. The fitted GPS correction is bundled; USB GPS
selection is not implemented. Camera capture uses the image's `rpicam-still`
provider; phone access separately needs Tailscale/account setup. See the
[optional-feature requirements](docs/install.md#optional-camera-and-phone-features)
and [phone instructions](docs/phone-access.md).

Public and private channels are both supported, including imports, messaging,
DMs, position requests, traces and selected-channel QR sharing. New message and
Channels show **Public/Private** labels. Choose a DM's channel explicitly; no
missing/disabled channel falls back to primary. The label describes the channel
key, not a guarantee of the firmware's per-message DM encryption.

Imported channels start with location and MQTT forwarding off, and do not retune
the radio from the share URL. All channels share one region, modem preset and
frequency slot. Match those to the mesh you want to join; Settings includes
**Frequency slot** (0 = Auto, derived from the primary channel's name).

Public messages and firmware identity/telemetry can be visible to others.
Automatic public location starts off, requires a separate confirmation and is
limited to 15-bit precision (roughly 730 m). A manual public location send can
contain precise coordinates and has its own confirmation. Only the selected
channel is enabled for automatic location; Off disables it on every channel.
Unapproved public location defaults are disabled on disk **before** firmware can
broadcast. A matching, confirmed public-location choice survives restarts.

Use the appropriate regional band and a valid preconfigured profile. Persisted
configuration must use the qualified firmware's preference schema 24;
incomplete/other versions fail closed. See [the security model](docs/security.md).

## Build

For the complete Debian arm64 installer and dependency/source assets, install
Python 3, Git and Docker with Linux arm64 support on your build computer, then:

```sh
git clone https://github.com/Prophet60191/m5stack-cardputer-zero-mesh-app.git
cd m5stack-cardputer-zero-mesh-app
python3 scripts/build-package.py
```

First use downloads build tools and pinned dependencies automatically. This is a
large developer build; ordinary users should use the prebuilt installer. The
[distribution guide](docs/distribution.md) explains all options, output paths,
cache/offline requirements, source-archive builds and release preparation.
[Build and test details](docs/build.md) cover the desktop simulator, raw CMake,
regressions and CI. Offline build mode needs prepared caches/images and skips
the separate fresh APT dependency-download check. Build/test containers do not
access device hardware or transmit RF.

## Documentation

- [Documentation index](docs/README.md) — current instructions and historical evidence.
- [Install, upgrade, backup, recovery and uninstall](docs/install.md).
- [User guide and quick reference](docs/user-guide/README.md).
- [Build/dependency/source distribution](docs/distribution.md) and [development tests](docs/build.md).
- [Current status](docs/current-status.md), [qualified package](docs/distribution-validation-20261123.md) and [Store preparation](docs/store-submission-readiness-20261123.md).

## Security

This release candidate includes fixes from an independent code and sensitivity review;
see the remediation record for verification limits.
Highlights of what's hardened:

- The `meshtasticd` TCP API is **confined to loopback** (it binds all interfaces by default).
- Sends require the **selected, enabled channel**, public or private; a missing
  channel never falls back to primary. Public location requires deliberate opt-in.
- Privacy settings show Pending/Unknown until effective radio settings are confirmed.
- The control socket is owner-only (`0600`, ownership-checked); the UI verifies the supervisor is yours.
- Input and work are bounded (frames, JSON, queued bytes, connections, operations, traces) and the supervisor survives hostile/stuck clients.

See [docs/security.md](docs/security.md) for the full list. Found something? Please open an issue.

## License

- **App** (`src/`, this README, the CMake build) — **MIT**, &copy; 2026 Jose Di Lenola. Originally based on the
  MIT-licensed M5Stack CardputerZero Application Template; that attribution is preserved in [NOTICE](NOTICE).
- **Supervisor** (`supervisor/supervisor.py`) — **GPL-3.0-or-later**, because it links the GPL `meshtastic`
  library. See [supervisor/LICENSE.GPL](supervisor/LICENSE.GPL).
- Bundled fonts/assets keep their own licenses (see [NOTICE](NOTICE)); `meshtasticd` and the `meshtastic`
  library are GPL projects redistributed with their licenses and paired corresponding source.

See [LICENSE](LICENSE) and [NOTICE](NOTICE).

---

<p align="center"><sub>Built for the M5Stack CardputerZero • <a href="https://github.com/Prophet60191">github.com/Prophet60191</a></sub></p>

## Radio resource conflicts

If startup reports that the NFC SPI-CS2 overlay reserves GPIO22, close the NFC
app and cold-restart the device before using LoRa. The official NFC app uses
GPIO22 as SPI-CS2 and documents that its runtime overlay stays until reboot;
LoRa uses that pin as BUSY. Mesh checks ownership before power-on and does not
unbind another app's kernel driver. See the [upstream NFC hardware notes](https://github.com/CardputerZero/Cap-CC1101-NFC#hardware).
