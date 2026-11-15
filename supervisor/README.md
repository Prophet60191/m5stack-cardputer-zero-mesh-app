# CardputerZero app toolkit — first pieces

The start of our reusable app toolkit (design: [../../docs/software/reusable-library.md](../../docs/software/reusable-library.md)) and the **radio-foundation test** for the Meshtastic client ([builds #2](../../docs/builds/README.md)). These run **on the device** (Linux `/sys` + `/dev`), not on the Mac.

| File | What it is |
|---|---|
| `cap_lora.py` | The lowest layer: power the Cap LoRa-1262 the right way and enable its RF switch, with a `powered_cap()` context manager and classified errors. Reusable by any app that drives this Cap. Recipe: [../../docs/software/cap-radio-bringup.md](../../docs/software/cap-radio-bringup.md). |
| `meshtasticd-cardputerzero.yaml` | Our `meshtasticd` hardware config (pins only — no region, no channel, no secrets). |
| `radio_up.py` | Powers the Cap, runs `meshtasticd` against the SX1262, and reports whether the radio came up. The foundation test. |

## Prerequisites

- The **Cap LoRa-1262 is seated and its antenna attached** (never power the radio without the antenna).
- Your user is in the `spi`, `gpio`, `i2c` groups (usually already the case).
- `meshtasticd` installed (one-time, needs sudo):
  ```bash
  sudo apt-get install -y ~/meshtasticd-pkg/meshtasticd.deb
  ```

## Use

Safe check first — powers the Cap, verifies the 0x43 RF-switch expander, powers back down. No transmit:
```bash
python3 cap_lora.py --probe
```

The foundation test — powers the Cap and brings `meshtasticd` up on the radio:
```bash
python3 radio_up.py            # reports RADIO ONLINE or a classified failure, then powers down
python3 radio_up.py --keep     # leave it running so a client can connect (Ctrl-C stops + powers down)
```

A success looks like `[ok] radio initialized: …` then `RADIO ONLINE — meshtasticd is serving on 127.0.0.1:<port>`. After that, the next step is to set **Region US** and import the **`backhaul`** channel with the `meshtastic` client against that port, and message the T-Deck Pro both ways.

## Safety

- The Cap is always powered back **down** on exit (even on error or Ctrl-C), so it doesn't drain the battery.
- `radio_up.py` runs `meshtasticd` as a short-lived **child process**, not a system service — the kernel frees SPI/GPIO the moment it stops.
- US band + `backhaul` channel only; never the public LongFast channel.
