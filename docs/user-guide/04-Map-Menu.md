# Map Menu

> For Mesh **0.1.17 (release candidate)** · Updated **2026-11-23** · [Guide index](README.md)

The **Map** screen is an offline, north-up radar centered on **you**, showing the direction and relative distance of every node that has shared a GPS position.

From the Home Carousel, roll to **Map** with **Z** / **C** and press **Enter** to open it.

![Offline Compass Radar Map](images/map-radar.png)

---

## Keyboard Shortcuts on Map

The Map is a glance-and-go live display—dots cannot be selected individually here. Use [Nodes](03-Nodes-Menu.md) whenever you want to inspect a specific radio's coordinates, signal, or battery.

| Key | Action |
|---|---|
| **ESC** | Return to the Home Carousel |
| **HELP** | Show Map shortcuts |
| **PRSC** | Save a PNG screenshot of the radar map |

---

## How to Read the Radar Map

The Map works offline, using two distance rings instead of street-map tiles. Positions update as they arrive; freshness markers age even without new packets.

### 1. You Are Always at the Center

- The dot at the center is your device, labeled **`you`** (when your GPS fix is under 2 minutes old) or **`you (last known)`** (when your fix is older than 2 minutes).

### 2. Directions Match a Compass

- **Up** (top of the screen) = **North**
- **Right** = **East**
- **Down** (bottom of the screen) = **South**
- **Left** = **West**

The map stays north-up; it does not rotate with the direction you are holding the device.

### 3. Auto-Scaling Distance Rings

- Two rings surround you: an **outer ring** and a **halfway inner ring**.
- The outer ring automatically adjusts so the **farthest positioned node** sits right on the outer ring (or defaults to `1.0 km` if no other nodes are farther away).
- Read the top-left label to check the outer ring's scale—for example:
  - **`Filled=fresh / ring: 2.1 km`** means the outer ring is 2.1 km from you, and the inner ring is roughly 1.05 km from you.

### 4. Node Labels & Freshness Colors

Each positioned peer appears as a dot labeled with its short callsign (such as `BASE`, `RDG`, or `SCT`, or its full name if no short name is set). Radios without a GPS fix (`Position: -` in [Nodes](03-Nodes-Menu.md)) are simply omitted from the radar map:

- **Filled Dot:** Fresh GPS fix updated within the **last 2 minutes**.
- **Hollow Dot:** Older ("last known") position from **more than 2 minutes ago**, or one whose timestamp is missing, unknown or in the future.

*Privacy Note:* This app limits its own automatic public position sharing to roughly 730-meter precision. **Other radios choose their own precision**, and a public position can be precise. The map displays the received coordinates; it does not blur them or guarantee a privacy radius.

---

## Common On-Screen Messages

- **`"No position fix yet. Go outside with a sky view and give the GPS a minute."`**
  A fresh location has not reached the app yet. Check that **GPS receiver** is enabled in [Settings → Location & Internet](05-Settings-Menu.md#location--internet), then step outdoors with an open sky view for a minute.
- **`"No other nodes have shared a position."`**
  Your device knows where *you* are (`you` appears at the center with a default `1.0 km` outer ring), but no other nodes have sent coordinates yet. Many radios keep location sharing off by default—see [Messages → Thread Options](02-Messages-Menu.md#thread-options-o) if you want to ask a friend directly using **Request position**.

---

The Cap GPS can work with automatic location sharing **Off**. Mesh refreshes its
local position about every 30 seconds while the receiver is enabled; you do not
need to broadcast your coordinates to use the map. A satellite count may remain
unknown with the current radio API even when the position is fresh.

## Step-by-Step Example: Spotting Which Way Camp Is

1. Take your CardputerZero outside and open **Map** from the Home Carousel.
2. Once your GPS locks on, your center dot displays **`you`**.
3. You see filled **`BASE`** toward the top-right and hollow **`SCT`** below-right. The ring label reads **`Filled=fresh / ring: 2.1 km`**.
4. **BASE** is northeast with a fresh position; **SCT** shows an older reported location. Directions are north-up, regardless of how you hold the device.
5. For coordinates, distance or fix age, press **ESC**, open **Nodes**, and press **Enter** on a node.

---

[← Nodes Menu](03-Nodes-Menu.md) | [Quick Reference](00-Quick-Reference-Card.md) | [Next: Settings Menu →](05-Settings-Menu.md)
