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
