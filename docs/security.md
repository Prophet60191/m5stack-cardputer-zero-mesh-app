# Security model

This is a radio you carry, so the design keeps the risky parts small, local, and
auditable. Before release the whole app went through an independent code and
privacy review; this documents the model and what was hardened.

## Trust boundaries

- **The UI never touches the radio.** It only sends named *intents* over a local
  Unix socket and renders the *events* it gets back. It holds no radio state and
  can issue no shell commands — every intent maps to one fixed, validated action
  in the supervisor.
- **The control socket is local and owner-only.** It lives in an owner-only
  (`0700`) directory with mode `0600`, and the UI verifies (via peer
  credentials) that the supervisor it connects to is owned by the same user, so
  another local user cannot impersonate it.
- **The radio daemon is loopback-only.** `meshtasticd` binds its TCP API on all
  interfaces with no option to restrict it, so the supervisor pins it to one
  fixed port and `supervisor/firewall-setup.sh` confines that port to loopback.
  **Run that script (as root) once** — it is the one privileged setup step, and
  the Debian package's install hook does it for you.

## What the review hardened

- **No wrong-channel sends.** A conversation is keyed by its channel, and a send
  refuses an unconfigured channel — a "private" thread can never fall through to
  the public default channel.
- **Position sharing really turns off.** "Share position: Off" sets each
  channel's position precision to zero (no location is transmitted), rather than
  only changing the broadcast interval.
- **Robust against bad input.** The supervisor bounds line length, JSON nesting
  depth and connection count, rejects non-object intents, and uses per-client
  bounded write queues so one slow or hostile local client cannot stall it.
- **Clean process ownership.** Radio start/stop are serialized; a crashed
  previous supervisor's orphaned daemon is reaped on startup; a watchdog powers
  the Cap down if the daemon dies unexpectedly.
- **No SIGPIPE / no UI freeze.** The UI's socket writes are SIGPIPE-safe and
  time-bounded, so a stuck supervisor can't crash or hang the app.

## What is intentionally *not* here

- **No secrets in the repo.** Channel pre-shared keys, share URLs and node
  identities live only on your device; the simulator's sample data is wholly
  synthetic.
- **No bundled GPL binaries.** `meshtasticd` and the `meshtastic` library are
  installed by you; the repo ships only the MIT app and the GPL-3.0 supervisor
  that imports the library.

## Operational notes

- **ADB, when enabled, is an unauthenticated root shell** over USB. Turn it off
  (Settings → Developer) when you're done using it.
- **Antenna before power.** Powering the LoRa Cap without its antenna can
  permanently damage the radio. This one is on you — the software can't check it.
- **Private channel, licensed band.** Use your own private channel and a region
  you're licensed to transmit on; never the public default channel for your own
  traffic.

Found an issue? Please open a GitHub issue.
