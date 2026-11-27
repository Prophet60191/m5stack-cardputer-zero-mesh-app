# Keyboard integration audit — 2026-11-27

## Scope and decision

This work covers Mesh, Wireless Surveyor, NFC Tools and App Shell only.
Quartermaster and other third-party applications are excluded and unchanged.
These apps use standalone Template-derived LVGL input. Keep that architecture;
do not decode events a second time in an app using the official shared runtime.

The device driver already resolves held, one-shot and locked Fn/Sym layers.
Our decoder translates its events, keeps per-device modifier/release ownership,
and checks all 32 symbols against an independent official fixture. Recognizing
an action code is not sufficient: these standalone apps lacked the system handler
which the official launcher/shared runtime supplies. That caused volume keys to
be consumed without changing volume. We reuse the official mixer implementation
unchanged, with a small app-owned adapter and normal-user OS interfaces.

## Official sources reviewed

- [Keyboard specifications](https://github.com/CardputerZero/cardputerzero.github.io/blob/main/docs/en/CP0_keys.md).
- [Official launcher](https://github.com/CardputerZero/launcher/tree/d59f15918c473bc61de13b4960d69ce0e7514899), specifically GlobalHintPolicy, setup_value_policy.hpp and cp0_audio_mixer_control / cp0_pulse_client. The four copied source files and their exact hashes are in system_keys/upstream.json.
- [Template](https://github.com/CardputerZero/Template/tree/477fa6d217b683e3084c361a757f03dc8ca9f753). Standalone integration adapts its input delivery; no firmware or OS keymap changes.

## Keyboard contract

| Keys | Behavior |
| --- | --- |
| Ordinary letters, Shift, all 32 Sym symbols | Text in an editor; existing app shortcuts outside typing |
| Tab / Shift+Tab | Forward / backward focus or the screen's documented tab action |
| Fn+Z/C/F/X | Left/right/up/down; typing uses cursor navigation |
| Fn+K/N; Fn+Backspace | Home/End; forward Delete |
| Fn+L/M | Page Up/Down in applicable lists; never text in an editor |
| Fn+H/J | Help / screenshot; preserve editor draft and cursor |
| Fn+A/S/D | Mute / volume down / up, 10% steps, 0–100% |
| Fn+U/I | Brightness down / up, 10% steps, 10–100% |
| Fn+Q/W/E | Play/pause / previous / next for one MPRIS player |
| Fn+1…0/O/P | F1–F12 semantic actions; unbound where no app feature exists |
| Fn+B | Insert semantic action; NFC's variable picker where applicable; otherwise unbound |
| Enter / Esc | Contextual accept/back; typing and overlays own their keys |

Mesh adds explicit Left/Z → Channels and Right/C → Direct Messages on its
unfiltered conversation list. Tab/Shift+Tab remain available. Search spans both
lists; composing and overlays keep their own keys.

System feedback is a short nonfocusable overlay. Bounded worker operations keep
LVGL responsive. Mute/media auto-repeats are ignored; level repeats are bounded
and released repeats are dropped. No player, several players or an unavailable
service produces a clear limitation message. This does not promise remote control
of a music app which does not expose MPRIS, or a general international IME.
No root audio process, new global key reader, input grab or new permissions.

## Verification boundaries

Decoder fixtures and synthetic evdev-to-editor tests cover mapping, release,
repeat, loss/disconnect, typing and shortcut ownership. Desktop and Debian arm64
suites are distinct from real hardware checks. SDL now maps Shift+Tab, paging,
function and media keys consistently with the native action contract.

A real normal-user Lockon probe successfully read and changed volume, mute and
brightness, then restored exact per-channel volume, mute and raw brightness.
That proves the OS backend, not that someone physically pressed every Fn key.
Installed-app evdev replay and physical acceptance must be recorded separately.
No NFC card writes, RF transmissions, scans, camera or GPS work is part of this
keyboard audit. Existing saved app data and settings must be preserved.

## Offline qualification

Desktop suites: Mesh 7/7, Surveyor 6/6, NFC Tools 14/14, App Shell 7/7.
Debian arm64 suites: Mesh 6/6 plus 175 backend tests, Surveyor 8/8,
NFC Tools 16/16, App Shell 6/6. Installer dependencies now explicitly include
libpulse0 and libglib2.0-bin. Tests use fixtures/fakes and do not claim RF,
NFC card, physical modifier or field-survey qualification.

## Installed keyboard verification — 2026-11-27

The committed installer was installed on Lockon. Its regular packaged files,
modes and root ownership match the extracted package. Actual native UI input
replay on the internal evdev device changed volume/mute/brightness and showed
correct feedback, running as the ordinary user. Exact original per-channel
volume, mute and raw brightness were restored; launcher restored, ADB left On.
Original saved app files and all existing database rows remain, with SQLite
integrity OK; Mesh's system configuration is byte-for-byte unchanged and radio
stays Off. No Cap power, RF send, scan, tag write or camera/GPS test was made.
Media's real normal-user D-Bus backend delivered PlayPause/Previous/Next to a
synthetic player; absent/ambiguous players were safely reported. This is not
acceptance of an arbitrary music app lacking MPRIS or of every physical key.
