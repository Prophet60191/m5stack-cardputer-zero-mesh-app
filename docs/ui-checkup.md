# UI checkup — 2026-11-24

The owner reopened the change cycle for a full UI review. This 0.1.20 candidate
keeps radio ownership, channel identity, privacy and storage behavior unchanged.
The installed 0.1.19 qualification remains separate until installation is checked.

## Confirmed defects and corrections

- Nodes, conversation titles, sender names and Map tags could exceed horizontal
  bounds. List captions now fit their rows, full node headings/values wrap in
  scrollable details, sender names wrap within bubbles and Map tags shorten.
- Nodes, conversations, recipient pickers and message action menus stopped at
  their ends while Settings wrapped. These selectable lists now wrap both ways;
  Shift-Tab also reverses the Nodes filter. Message chronology and scrollable
  detail views retain their existing boundaries.
- An invalid frequency slot closed its editor. Rejected input now stays focused
  with its typed value intact; valid input still applies normally.
- Quote/edit assigned text to a hidden textarea when channel selection prevented
  composing. Text is now inserted only after a composer actually opens.
- Arbitrary hashes in notifications were interpreted as LVGL color commands.
  Payload is literal; highlighted notifications containing hashes use plain text.
  Long notifications now fit below the header instead of growing off screen.
- Channel-share title plus the 126px quiet-zone box exceeded the 140px content
  area and clipped the top. A 110px box with 100px code and bounded one-line title
  fits together; rendering failure produces a readable error instead of a blank.
- A long region, 256-node caption and 2,000-unread badge could overlap the battery
  area. The node caption now takes the remaining width and shortens when needed.
- Search claimed case sensitivity despite the existing English-letter-insensitive
  matching. Its prompt is corrected, the active query stays visible and an empty
  result has an explicit explanation. Esc clears the filter.
- Byte-truncated conversation/incoming previews could split UTF-8 characters;
  both now use a shared boundary-safe preview helper.

## Verification

`mesh_ui_checkup` uses real LVGL at 320×170, synthetic state, ordinary-user
execution and a fake command sink. It covers the above layout, navigation,
validation, quote, toast, QR/header, Map, search and Unicode cases in both themes.
It opens no hardware or network connection. It is excluded from installation.
Optional screenshots: `mesh_ui_checkup all /absolute/output/directory`.
The focused checks reproduce nine independent failures against pre-fix source;
Unicode boundaries have additional direct checks. Raw logs/screenshots stay in
ignored build output, and contain only synthetic data.

All five host CTest targets pass under ASan/UBSan, and the backend suite passes
175 tests (one platform-specific skip). Debian arm64 package checks are required
before device installation; exact source/package and installed results will be
recorded here after completion. Rendered-code software decoding is
separate from an actual camera/phone scan. Fresh-image hardware, USB GPS, SIM,
public-peer/IPv6/full physical keyboard and earlier unexplained phone disconnect
limits remain as documented in [current status](current-status.md).
