# Keyboard contract and regression evidence

Mesh 0.1.8 uses the complete official CardputerZero symbol map. The previous
handwritten 14-entry table had three incorrect assignments and omitted 18 symbols:
code 92 must be `/`, code 80 `:`, and code 71 `-`. This explains Sym+N producing
a colon. Directly setting textarea text in old tests did not exercise this decoder.

## Authoritative inputs

- [M5Stack keyboard specifications](https://github.com/CardputerZero/cardputerzero.github.io/blob/main/docs/en/CP0_keys.md)
- [Pinned official launcher map](https://github.com/CardputerZero/launcher/blob/4c05b444cb977cb90e2f33eb4c8261226e370713/ext_components/cp0_lvgl/src/cp0/cp0_keyboard_keymap.c)
- The fixed root-owned system file `/usr/share/keymaps/tca8418_keypad_m5stack_keymap.map`
- [Independent reference fixture](../tests/fixtures/README.md), including provenance/hash

The [decoder](../src/platform/keyboard_decoder.h) is a reusable C++ component
using LVGL key constants, without UI objects or hardware calls. New apps should
vendor it with [its tests](../tests/keyboard.cpp) and the upstream fixture, pin
the source revision and preserve MIT/M5Stack attribution. The [app-builder guide](https://github.com/Prophet60191/m5stack-cardputer-zero-app-guide/blob/main/docs/05-keyboard.md)
explains the contract and integration requirements.

## Mapping and ownership

The built-in driver already chooses the Sym-layer event code. Map those codes
directly, including after a one-shot tap or lock; do not demand another physical
Sym-down event before recognizing them. Load the system map only for the identified
`tca8418c`/`tca8418` keyboard. Other keyboards keep US-layout translation, so their
ordinary `[` is not turned into `!`. This is not a general international-layout
or IME implementation.

The complete upstream map is compiled in as the fallback. A runtime override is
accepted only from the fixed, root-owned, regular non-writable system file and
only after bounded validation of all 32 unique assignments. Invalid/partial maps
leave the previous valid map intact. No input intent can supply a map path.

Keep modifier/active-key state per device. A key release uses its press-time
translation, independent of later Shift releases, focus changes or map reloads.
Query typing context per event, since Enter and the next letter can share a read
batch. On event loss, discard until SYN_REPORT and restore available kernel
modifier state. There is no direct I2C access or keyboard firmware modification.

## Fn and application shortcuts

- Fn+Z/C moves the cursor left/right; Fn+F/X is Up/Down (a one-line editor has no
  second line). Fn+K/N is Home/End and Fn+Backspace deletes to the right.
- Fn+H toggles Help and Fn+J takes a screenshot while editing too. A visible Help
  overlay consumes Enter and typing, preserving the underlying draft and cursor.
- PageUp/PageDown, Insert, F1-F12 and media/display keys are non-text actions.
  Mesh does not bind additional editor behavior to those keys. They cannot
  insert control values or unexpectedly move focus. Platform media/display
  handling is separate; Mesh does not add shell commands for those functions.
- Shortcuts are consumed in the indev reader **before** LVGL can send them to the
  focused textarea. The simulator uses that same filter. Hold-to-exit applies
  only in Home. Unbound Fn values never become invalid Unicode in a draft.
- LVGL owns repeat timing; Linux repeat records do not cause duplicate sends.
  Overlapping letter presses are serialized for LVGL's single-key input model.

## Verification and limits

`mesh_keyboard_regressions` compiles the production decoder on the development
host and compares every symbol to the independent original fixture. Linux also
builds `mesh_keyboard_evdev_regressions`: synthetic input_event records go through
the production reader, decoder, shortcut filter, LVGL indev and a focused textarea.
It checks exact text, editing controls, Help/Screenshot consumption, unbound Fn
actions, one-batch focus changes, rollover, loss and disconnect. It opens a pipe,
not `/dev/input`, and cannot send RF. Both targets are excluded from the installer.

Run CTest as documented in [build.md](build.md). The Debian arm64 package gate
runs the real reader regression as well as existing backend/policy checks.
Event replay tests the software contract; it is not a claim that an operator
physically pressed every key. Record an actual-device replay separately, and
recheck the driver contract when changing the system image or keyboard firmware.

The native arm64 CI job uploads `cardputerzero-mesh-keyboard-replay-arm64`
separately from the installer, with checksums, source commit and license notices.
After verifying them, run `./mesh_keyboard_evdev_regressions ./cardputerzero-keymap.map`
as the ordinary device user. This uses a headless LVGL display and a pipe and
reads only the fixed system keymap; it does not open the real keyboard, radio,
camera or framebuffer. No launcher/service shutdown or physical key sequence
is needed for this software replay check.
