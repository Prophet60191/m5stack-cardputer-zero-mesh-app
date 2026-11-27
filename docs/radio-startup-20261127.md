# Radio startup recovery — 2026-11-27

## Observed failures

Mesh correctly refused startup because an unused runtime NFC SPI-CS2 overlay
held LoRa's BUSY GPIO22. No NFC SPI process was open. The installed BSP's official
GPIO-forwarder supports runtime release: after removing only the sole, unused
`spi0-spidev2-gpio22-overlay`, GPIO claims cleared and ordinary-user preflight
passed. This was an administrator repair, not permission for Mesh to remove
another app's overlay or take over an active GPIO owner.

The next actual start failed because the Cap's I2C control chip at 0x43 did not
answer. A leased, ordinary-user, one-second off/one-second on Cap cycle restored
a valid 0xA2 identity, followed by Online/send-receive startup. The precise cause
of the initial unresponsive chip is not established; do not label it a firmware
or keyboard defect. No settings or channel keys were changed.

## Bounded recovery in 0.1.24

Inside the existing guardian lease and switch-recovery record, the power module
now retries a missing I2C identity once after a one-second off/on settling cycle.
It closes the I2C descriptor before cycling, probes before any RF configuration
write, and restores original power/mux on failure or interruption. Healthy startup
has no additional power cycle or delay. A wrong chip, permission problem, GPIO
conflict or RF-configuration write failure is not blindly retried. A missing Cap
still fails after two identity attempts. There is no automatic overlay removal,
unlimited retry, profile rewrite or weaker firewall/privacy check.

Seven focused tests cover normal success, one successful recovery, persistent
absence, wrong identity, interrupted recovery and power-only use. Actual installed
startup cycles and stability qualification are recorded after package validation.

## Official sources and scope

- [M5Stack GPIO forwarder](https://github.com/m5stack/m5stack-linux-dtoverlays/blob/a3fd3e83c0247e16e194e7e40f7fc1f5be933dd4/modules/CardputerZero/gpio-forwarder.c): device-tree notifier and backing-GPIO release.
- [Raspberry Pi runtime overlay lifecycle](https://www.raspberrypi.com/documentation/computers/configuration.html#part3.4): runtime overlays can be removed, in reverse order; do not disturb unrelated overlays.

Restarting the device remains the straightforward recovery when switching Caps
and the overlay's ownership/driver support is unknown. Runtime release observed
on this BSP is not a universal promise for older kernels. Keep the correct Cap
and antenna attached before radio power; close the other hardware owner first.

## Additional cold-start race found during qualification

With full Cap power-off between starts, the third 0.1.24 intermediate start
failed with connection refused: SX1262 initialized before the daemon bound its
local TCP API. The supervisor had treated radio initialization as complete API
readiness. It now waits up to ten seconds for that listener, retrying only a
refused connection to the fixed loopback endpoint. Daemon exit and service stop
abort the wait; handshake/configuration failures are not replayed. The pinned
Meshtastic client closes failed constructors, and callbacks are subscribed only
after the one successful connection. Six tests cover readiness, one fixed
deadline, daemon death, stop/cancel and non-retryable failures. The intermediate
installed artifact is superseded by final qualification below.

## Final installed qualification

Final Mesh **0.1.24-rev1** is installed from runtime/package source `66fd21519592bc6f25996751c9055f4ddf8f2efd`.
Installer SHA-256: `0e08deb6d884283ebac06c52bb90af7d6a538ebb3c66186a7ad487416a634869`. All 266 regular packaged files, modes and root
ownership match the extracted package. Earlier same-version 8eb0349 was an
intermediate candidate and is superseded; do not identify artifacts by version
alone. Source-correlated [CI run 46014468424](https://github.com/Prophet60191/m5stack-cardputer-zero-mesh-app/actions/runs/46014468424)
passes both jobs; local native/arm64 package gates pass six CTest suites and
all **188 backend tests**, including 13 new recovery/readiness cases.

- Real Cap test: inject only the first identity failure, then perform the actual
  recovery cycle and verify a real 0xA2 response and original switch restoration.
  This is fault injection, separate from the earlier naturally failing I2C read.
- **12/12 full Cap power-off/start cycles pass**, with the rail verified Off
  before each start. Online/send-receive takes **1.23–3.28 seconds**. These are
  Cap power cycles, not full-device cold boots. No API-refusal event recurred in
  this bounded run; mocked timing cases separately exercise the retry branch.
- The following **ten-minute watch passes all 60 samples**: Online, send/receive,
  control connection and trusted firewall healthy. Final status is also Online.
- The updated UI opens cleanly from the actual launcher, with a visible TX/RX
  header; closing/reopening it and restarting the launcher do not interrupt the
  separately owned radio. A CLI-launch test had terminal drawing over the UI;
  the final check uses the supported launcher context instead.
- Original app files, saved radio/channel/profile choices and all existing
  SQLite rows are preserved, with integrity OK. No tuning, privacy, auto-start
  preference, firmware, native GPS patch or channel-key change. No explicit
  message/peer, outdoor GPS, USB dongle, SIM or fresh-image test is claimed.
- ADB stays enabled, Mesh stays open and the radio stays Online in send/receive.
  Private backups/raw evidence remain outside Git; temporary test files are
  removed after verification. Another chat's app-guide edits are untouched.

This bounded qualification fixes the observed startup defects; it is not a claim
that future hardware absence, another active owner or every long-term failure is
impossible. Mesh still refuses conflicting GPIO ownership rather than taking it.
