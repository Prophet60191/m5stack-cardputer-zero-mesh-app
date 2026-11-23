# Deferred keyboard-alignment review

Logged 2026-11-18. **Status: pending; record only.** Resume when the owner asks.
This note does not establish that this app is aligned, justify an existing
difference, or authorize implementation changes or device tests now.

## Follow-up

Compare this app's actual keyboard implementation with current official M5Stack
guidance and the appropriate official input path. Check official sources first;
record the revisions reviewed and whether each behavior is aligned, adapted,
intentionally different or still unverified.

- Sym assignments and Fn actions; held, one-shot and locked modifier behavior.
- Bare F/Z/X/C navigation outside text entry and ordinary letters during typing.
- Text versus action routing, repeat/release ownership, lost/disconnected input,
  editor focus, and Help/Screenshot/overlay handling.
- Enter/Esc and long-Esc behavior; contextual/action footer hints. Distinguish
  official recommendations from requirements and our deliberate preferences.
- Whether this app uses raw Template input or the shared runtime's decoded event
  contract; avoid double translation or assuming the two APIs are interchangeable.

For every difference, document the official behavior, actual app behavior, source
locations, reason, benefits/costs, compatibility implications and evidence. A
Template integration need or personal preference can justify a difference; an
unsupported assumption cannot. Mark missing evidence explicitly. Any proposed
change remains a later discussion, preserving existing input regressions and
app-specific behavior.

## Starting references

- [Official key specifications](https://github.com/CardputerZero/cardputerzero.github.io/blob/eeb3c9f61afc33cc1ceb33f3ed0b81fd95491c0b/docs/en/CP0_keys.md).
- [Official Template input](https://github.com/CardputerZero/Template/blob/477fa6d217b683e3084c361a757f03dc8ca9f753/src/platform/linux_input.cpp).
- [Official shared input contract](https://github.com/CardputerZero/launcher/blob/13d2f26b103e5c9292be7287bc640df9adf6faeb/docs/current-implementation-and-components.md#input-delivery-contract).
- Companion app guide: `docs/official-input-evaluation.md` and
  `docs/library-principles.md` in `m5stack-cardputer-zero-app-guide`.
- App entry points: `src/platform/keyboard_decoder.cpp`,
  `src/platform/linux_input.cpp` and `tests/keyboard.cpp`.

These official revisions are starting references from the 2026-11-18 evaluation;
refresh them when resuming. A matching symbol table or historical test pass alone
does not establish complete app alignment. No audit or migration is completed by
this log.

