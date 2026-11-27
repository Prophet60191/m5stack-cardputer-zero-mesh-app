# Quick Reference Card

> For Mesh **0.1.23 (release candidate)** · Updated **2026-11-27** · [Guide index](README.md)

## Hardware Safety Rules

1. **Antenna attached before Radio On.**
2. **Radio Off before removing or swapping the LoRa Cap.**
3. After using NFC, **power the device completely off and back on** before LoRa.

## Keyboard Shortcuts

**Browsing:** **F / X** = Up / Down; **Z / C** = Left / Right; **Enter** = Select; **ESC** = Back. No Fn needed.

**Typing:** F / X / Z / C type letters; **Fn + F / X / Z / C** move the cursor. **Shift** = Capitals; **Sym** = Blue symbols. **Enter** = Send / Save; **ESC** = Cancel (keeps the message draft).

**HELP** = Shortcuts; **PRSC** = Screenshot to `Pictures/Screenshots`.
**Home:** Z / C choose a screen; Enter opens it; **Hold ESC** exits (an active radio keeps running).

| Screen | Extra keys |
|---|---|
| **Messages list** | **Left/Z** Channels, **Right/C** DMs; **Tab** switches; **/** Search both (A/a match); **ESC** clears search, then returns Home |
| **Inside a chat** | **Enter** Write / act on selected bubble; **q** Quick message; **o** Thread options; **F / X** Select bubbles |
| **Nodes** | **Enter** Details; **s** Favorite; **m** DM; **Tab** All / Favorites / Active |
| **Map** | North-up; dots are not selectable. Use Nodes for details. **ESC** returns Home. |

**Channel form:** **1** saves the channel or separate profile from any scroll position. In typing boxes, `1` remains a number.

## Top Status Bar Decoder

| Indicator | Meaning |
|---|---|
| Grey **off** | Radio powered down |
| Yellow **starting** | Startup and safety checks |
| Green **RX** | Listening only; sending blocked |
| Green **TX/RX** | Sending and receiving enabled |
| Green **Online ?** | Online, but receive/transmit mode is unconfirmed; check Diagnostics |
| Red **error** | Check the notice or Settings → Diagnostics |
| Grey **unknown** | Background service state not confirmed |
| **✉ N** | Unread count for unmuted chats |

## Message Status Decoder

| Status | Meaning |
|---|---|
| **sending…** | Queued / sending; awaiting confirmation (up to 60 s) |
| **Mesh confirmed** | Another mesh radio acknowledged the packet |
| **Recipient received** | Recipient radio confirmed the DM; `direct` / `2h` describes hops |
| **Receipt unconfirmed** | Sent with Delivery confirmations Off |
| **✗ failed** | Timeout, rejection or radio stopped. Select the bubble → Enter → Details / Resend |

**An ACK is not a read receipt.** A timeout can still mean the message arrived; a retry may repeat it. Failed sends are not replayed automatically.

## Group Sync Checklist (4 Things That Must Match)

1. **Region:** Same permitted regional band.
2. **Modem preset:** Same preset on both radios.
3. **Frequency:** Same effective slot; Auto also depends on the primary channel name.
4. **Channel:** Same name and secret key. Keep private QRs/URLs secret.

**QR keeps current tuning by default; review any chosen changes before Save.** For sending, enable Settings → Transmit mode after checking the antenna; look for **TX/RX**.

---

[Guide index](README.md) · [Getting started](01-Introduction-and-Getting-Started.md) · [Troubleshooting](06-Troubleshooting-and-FAQ.md)
