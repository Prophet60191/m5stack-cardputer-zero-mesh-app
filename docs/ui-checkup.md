# UI checkup — 2026-11-24

The owner reopened the change cycle for a full UI review. This 0.1.20 candidate
keeps radio ownership, channel identity, privacy and storage behavior unchanged.
Mesh 0.1.20 is installed; the earlier 0.1.19 RF qualification remains separate.

## Confirmed defects and corrections

- Nodes, conversation titles, sender names and Map tags could exceed horizontal
  bounds. List captions now fit their rows, full node headings/values wrap in
  scrollable details, sender names wrap within bubbles and Map tags shorten.
  Visual review also caught auto-height captions wrapping despite DOT mode; list
  caption height is now explicitly one line and checked in both themes.
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
  both now use a shared boundary-safe, one-line preview helper. Line breaks and
  control characters become spaces in previews; full message text is unchanged.

- Device-font checks found Help's fixed 16px rows/12px footer shorter than the
  Noto line metrics, and a 16px composer interior for a 17px line. Help now sizes
  its rows/footer/panel from font metrics; a 1px input border preserves the 22px
  typing bar while leaving enough room for the text. Both themes and actual
  device fonts are covered by independent geometry checks.

- Cancelling the recipient picker retained its node index as the conversation
  index, so Enter could open a chat different from the visible highlight. Closing
  the picker now restores New message and refreshes chats received while covered.

## Verification

`mesh_ui_checkup` uses real LVGL at 320×170, synthetic state, ordinary-user
execution and a fake command sink. It covers the above layout, navigation,
validation, quote, toast, QR/header, Map, search, Unicode, Help/composer font fit and recipient cancellation
cases in both themes.
It opens no hardware or network connection. It is excluded from installation.
Optional screenshots: `mesh_ui_checkup all /absolute/output/directory`.
The first focused checks reproduce nine independent failures against pre-fix
source; recipient cancellation has a separate before/after regression. Actual
device-font probes expose the two clipping issues missed by host fallback fonts.
Unicode/control-character boundaries have additional direct checks. Raw logs/screenshots stay in
ignored build output, and contain only synthetic data.

All five host CTest targets pass under ASan/UBSan, and the backend suite passes
175 tests (one platform-specific skip). Final Debian arm64 package and installed-device results are recorded below. Rendered-code software decoding is
separate from an actual camera/phone scan. Fresh-image hardware, USB GPS, SIM,
public-peer/IPv6/full physical keyboard and earlier unexplained phone disconnect
limits remain as documented in [current status](current-status.md).

## Final package and device result

- Exact runtime/package source: `5776364d99e2dae9b2565bd4cef452939a783e3a`.
- Installer: `cardputerzero-mesh_0.1.20-rev1_arm64.deb`, 7,869,474 bytes;
  SHA-256 `f7dc4d0ad77173603959f9dd612a1bf877449edbb7ba9c50df200b3057414da7`.
- [Source CI run 45500604820](https://github.com/Prophet60191/m5stack-cardputer-zero-mesh-app/actions/runs/45500604820)
  tracks native runtime/source rebuild, regressions and minimal fresh Debian
  container checks. This is separate from fresh-image physical hardware.
- Matching Debian 13 arm64 isolated package/upgrade/firewall checks pass, including
  all four device-target CTest suites (UI setup, checkup, maintenance and real
  evdev reader) and all 175 backend tests. Tests do not open hardware or send RF.
- All 259 installed regular files match package bytes, mode, owner and group.
  Consistent private backups and the qualified previous installer remain retained.
  All 27 original messages, three threads/preferences and saved app/radio/profile
  files are unchanged after installation and after the real UI check.
- The focused headless checkup also passes on the device as its normal user, using
  the installed Noto CJK fonts; no physical display/input/radio is opened by that helper.
- A normal-user installed UI launch stays healthy for 15 seconds. Actual full
  framebuffer capture shows the home carousel and intact header text/percentage,
  without a battery icon. The live supervisor confirms Radio Off; the native
  daemon remains absent throughout. The launcher is restored and the test-created
  supervisor is stopped, matching the starting lifecycle state. ADB remains On.
- The Cap is disconnected. No radio power-on, message send, phone/GPS/RF or
  physical keyboard/camera acceptance is claimed. Both-theme headless navigation
  and rendered QR software decoding are separate from physical key/scan checks.
- The first temporary live-status fixture used an incorrect socket filename;
  its cleanup restored the launcher. The corrected fixture passes. The first
  installation encountered ADB's nonexistent inherited TMPDIR in apt-listchanges;
  the final reinstall uses TMPDIR=/tmp and completes without that warning.
  Neither diagnostic/setup issue is presented as an application defect.
- An intermediate same-version package preceded the final one-line-caption
  correction. Version alone cannot identify the qualified installer: use the
  exact source and SHA above. Its earlier hash is not the final package.

The 42-page manual and one-page reference pass their build/link/version checks;
all pages were rendered and contact-reviewed, with the changed pages inspected
at full size. The existing Drive manual is updated in place. No public release,
Store/upstream submission or renewed GPS/SIM/fresh-card work is authorized here.

## Lessons for future UI checks

Check total title/body/footer geometry at physical resolution, including long
names and maximum counters. DOT mode needs an explicit caption height for a
one-line result. Preserve rejected form values, restore the underlying list selection when a picker closes,
measure actual platform fonts, distinguish a visible composer
from merely allocated widgets, keep external text literal, and verify wrap-around
navigation independently from chronological message navigation. Keep actual-device
observations separate from headless screenshots and software QR decoding.
