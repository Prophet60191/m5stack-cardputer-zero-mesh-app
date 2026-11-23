# Troubleshooting & FAQ

> For Mesh **0.1.19 (release candidate)** · Updated **2026-11-23** · [Guide index](README.md)

When something doesn't behave the way you expect out in the field, look up the message or symptom you see below.

---

## Installation and Startup

- **First launch takes longer:** the bundled Python environment is being prepared
  offline. Later launches reuse it. Allow setup to finish; do not start manual pip.
- **Bundled runtime files missing/corrupt/unsafe:** reinstall the verified
  complete app package. A damaged managed cache is rebuilt from bundled wheels;
  your chats, channel keys, profiles and old venv are separate and retained.
  User-cache ownership/permission errors need the runbook's reviewed cache-reset
  procedure, not a broad permission change.
- **Package update incomplete:** finish package configuration using the
  [recovery instructions](../install.md#recovery). Do not delete its update marker
  or launch as root to bypass it.
- **Need logs, backup, rollback or removal?** Use the
  [installation runbook](../install.md). Logs/backups may contain private data;
  share the sanitized Diagnostics export instead of raw logs or archives.

## "My Friend and I Are Right Next to Each Other, Why Can't We See Each Other's Messages?"

This is the most common reason two Meshtastic radios won't talk. Walk through the **4-Item Group Sync Checklist** in the [Quick Reference](00-Quick-Reference-Card.md#group-sync-checklist-4-things-that-must-match) on both devices:

1. **Region must match** (for example, both on `US` or both on `EU_868`).
2. **Modem preset must match** (for example, both on `LONG_FAST`).
3. **Frequency slot must match** (if set to `Auto`, both radios calculate the frequency from the Primary channel's name—so if one person renamed Primary slot `0` or uses a custom slot number, both must match!).
4. **Channel name and secret key must match.**

**Important tip about QR codes:** Scanning or pasting fills the unified setup form and keeps current tuning by default. Any tuning changes must be deliberately selected and reviewed before Save. If you scanned a friend's channel and still can't hear them, check that those three tuning settings match theirs!

For two-way messages, confirm both headers show **`TX/RX`** (send/receive).

---

## Radio Won't Start or Shows a Red `error` Dot

| What You See | Why It Happens & How to Fix It |
|---|---|
| **Error mentions NFC or a reserved pin (`GPIO22`)** | You opened the CardputerZero NFC app earlier in the same boot session, and it is still holding a shared pin. **Fix:** Close the apps and power the CardputerZero **completely off and back on** before starting the LoRa radio. |
| **Prompt asks to prepare a profile / First run** | No valid starting radio setup exists yet. **Fix:** Open **Settings → First-run setup** and complete the region and starting channel wizard. |
| **Error mentions firewall health unavailable** | The system firewall check that keeps the local radio port private isn't active or verified. **Fix:** Restart the device (or check **Settings → Diagnostics** to confirm `Firewall: healthy`). |
| **Radio turns `off` or shows `error` after changing privacy settings** | If the radio cannot confirm a change to **Transmit mode**, **Share position**, or **Internet forwarding**, it stops itself as a safety precaution. **Fix:** Turn **Radio** back **On** in Settings and try the change again. |

---

## Messages Show `✗ failed`

- **Radio is in Receive-Only (`RX`) or `off`:** Sending is blocked until you explicitly enable transmission. Check the top-left bar: if it says `RX` or `off`, go to **Settings → Radio** (turn On) and **Settings → Transmit mode → Enable transmission** (`TX/RX`).
- **Nobody acknowledged in 60 seconds:** Highlight the failed bubble with **F**, press **Enter**, and choose **Details** to read the **Reason** (for example, *"No acknowledgment before timeout"* or *"No route to destination"*). Move to higher ground or an open area and select **Resend**.
- **Radio stopped or restarted mid-send:** Messages still `sending…` when the radio stops or the device restarts are marked `✗ failed` and are never replayed automatically. Use **Resend** once `TX/RX` is back on.
- **`Original channel changed; retry cancelled`:** You renamed, rotated the key on, or removed that channel since the message was written. Start a new message on your updated channel instead.

---

## QR Code Scan or Share URL Import Problems

| What You See | What It Means & How to Fix It |
|---|---|
| **`complete first-run setup before adding a channel`** | A valid starting setup is required. Complete First-run setup, then return to Add a channel. The new flow also works with Radio Off. |
| **`No QR code found. Aim and retry.`** | The camera couldn't read a Meshtastic QR code within the 12-second scan window. **Fix:** Hold the device steady 15–25 cm away in good lighting (avoid screen glare) then choose **Add by → Scan QR code → Start scan** again. |
| **`Camera unavailable` or timed out** | Another app may be using the camera, or your device does not have a camera attached. **Fix:** Close other camera apps or use **Add a channel → Paste share URL** instead. |
| **`Preview failed`** (`not a Meshtastic channel share URL` / `invalid channel share data`) | The link you pasted isn't a valid `https://meshtastic.org/e/#...` channel share URL (or was cut off). **Fix:** Check the full link and paste it again to retry. |
| **`Channels already present`** | You already have every channel contained in that QR code or link, so nothing needed to be added. |
| **`channel preview expired; scan or paste again`** | An imported QR/link stays available in this form for up to **10 minutes**; save validation lasts **2 minutes**. Changes elsewhere invalidate stale preparations. **Fix:** Scan the QR code or paste the link again and check the form and press Save again. |
| **`all channel slots are occupied` / `not enough free channel slots`** | All 7 secondary channel slots (`1`–`7`) are occupied—including any **Disabled** slots, which still keep their saved key. **Fix:** Open **Settings → Manage channels** and **Remove** an unused secondary slot first. |

---

## GPS & Map Questions

- **`No position fix yet. Go outside with a sky view and give the GPS a minute.`:** Enable **GPS receiver** in **Settings → Location & Internet**. Automatic location sharing remains a separate choice.
  - Move outdoors with clear sky. **Diagnostics** shows position and age; satellites may stay unknown. If no fix appears, try Radio Off/On. Mesh 0.1.19 bundles the `+czgps1` identification-timeout correction; older candidates needed a separate radio-service update.
- **`GPS fix is stale or its age is unknown; wait for a fresh fix before sending`:**
  **Send my location** in Messages only sends coordinates when your device has a fresh GPS fix (within the last 2 minutes). Step outside with a clear sky view until **Map** shows a green **`you`** dot (or **Diagnostics** shows `Position: fresh`), then try again.
- **Why does `Fix age` say `(stale)` or a dot look hollow on the Map?**
  Positions older than **2 minutes (120 seconds)** become hollow on the Map and show `(stale)` in Node Details so you know the dot reflects where someone *was*, not necessarily where they are right now.
- **Why does my friend's dot look slightly off when using a `[Public]` channel?**
  This app limits its own automatic `[Public]` position sharing to roughly **730-meter precision**. Another radio can use different precision or send exact coordinates publicly. A dot is only the last reported position; its placement does not prove accuracy or freshness. **Send my location** can also send precise coordinates after the public confirmation.

---

## Settings Show `Unknown`, `Pending...`, or `Action timed out`

- **`Turn the radio on before changing settings` / `Unknown`:** Radio-hardware settings (Modem preset, Hop limit, Frequency slot, Share position, MQTT, GPS receiver) show `Unknown` while the radio is turned **Off** and require the radio to be **On** before changing.
- **`Pending...`:** The app is waiting up to 45 seconds for the radio to save and read back your change. Give it a few seconds (tuning changes restart the radio automatically).
- **`Action timed out` or `Working` screen stops:** Background requests wait up to 60 seconds (90 seconds for the complete channel Save). A timed-out Save is uncertain: inspect Channels and Diagnostics before retrying. If the radio was busy or disconnected, press **ESC** and try again—or inspect **Settings → Diagnostics** to verify `API: connected`.
- **`Connection lost; action result unknown. Check settings before retrying.`:** The background radio service disconnected or restarted while a Settings action (such as scanning, importing, or opening Manage channels) was running. **Fix:** Check **Settings → View channels** or **Manage channels** to verify what took effect before trying again.

---

## Quick Frequently Asked Questions

- **If I close the app, will I miss messages?**
  If you leave **Radio** **On** (`RX` or `TX/RX`) before exiting with **Hold ESC**, the background radio service stays running and keeps collecting messages until you power down or turn **Radio** **Off**.
- **Does `Save conversations → Erase saved history; RAM only` erase my channels?**
  No. Turning off **Save conversations** only erases saved message history, drafts, and pinned-chat metadata from storage—your channels, region, and mesh profiles remain intact.
- **Can I still read old messages if I rename a channel or rotate its key?**
  Yes! Past group messages remain readable under **`Archived: ...`** in your Conversations List.

---

[← Settings Menu](05-Settings-Menu.md) | [Quick Reference](00-Quick-Reference-Card.md) | [Introduction →](01-Introduction-and-Getting-Started.md)
