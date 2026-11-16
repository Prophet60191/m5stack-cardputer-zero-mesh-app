# Keyboard reference fixture

`cardputerzero-keymap.map` is the unmodified M5Stack system symbol map, also
represented by the MIT-licensed [official launcher table](https://github.com/CardputerZero/launcher/blob/4c05b444cb977cb90e2f33eb4c8261226e370713/ext_components/cp0_lvgl/src/cp0/cp0_keyboard_keymap.c).
Copyright 2026 M5Stack Technology CO LTD. It contains only public key assignments.
SHA256: `ac70eb97e9324a17c60d238f76ff90a3efb154f6bb026650ba78ced0c5b9896c`.

It is an independent expected-output fixture, not generated from our decoder.
Tests must compare all 32 assignments. Do not update both this fixture and the
implementation just to make a failing test green; verify an upstream/system
change first and update its attribution, hash and compatibility record.
