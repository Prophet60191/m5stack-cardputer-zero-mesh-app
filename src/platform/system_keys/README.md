# System-key integration for standalone Template apps

This focused adapter reuses M5Stack's `Cp0AudioMixerControl` and in-process
PulseAudio client **unchanged**, pinned in upstream.json. The official shared
runtime already handles these controls; apps on that runtime should use its
normal key/global-hint path instead of layering this adapter on top.

Our standalone Template-derived apps own their evdev input while the launcher
is paused. Their decoder recognizes media/display codes, but recognition alone
does not adjust sound or brightness. This adapter closes that integration gap;
it does not replace the driver, keymap, official firmware or shared shell.

Fn+A/S/D controls mute/volume; Fn+U/I brightness. Steps are 10 percent, volume
0–100, brightness 10–100, matching the official launcher policy. Fn+Q/W/E sends
PlayPause/Previous/Next to one MPRIS player. With no player, several players or
an unavailable service, it reports that limitation instead of guessing a target.
It does not play a test tone, capture audio or launch a player.

The app registers the raw key hook before ordinary LVGL/text routing. Actions
run on a bounded worker and results reach a small transient label on the LVGL
thread. Releases stop queued repeats; toggle/media repeats are ignored. No
shell, root process, input grab, radio access, new daemon or extra permissions.
Missing libpulse/server/backlight access fails visibly. OS volume/mute state is
used rather than an app-local cached percentage; boot persistence remains the
platform's responsibility. The adapter does not edit launcher configuration.

The optional C ABI supports an isolated test probe. No firmware is bundled here. Fake-backend tests never change real system settings.
Hardware/input replay, mixer readback and physical-key acceptance are distinct.

Files under upstream/ remain Copyright M5Stack Technology CO LTD, MIT. Local
adapter and tests are MIT. See LICENSE and upstream.json for exact provenance.

## Add to a standalone app

Copy this directory to `src/platform/system_keys`, retaining upstream.json,
LICENSE and the unchanged upstream files. Add `add_subdirectory(src/platform/system_keys)`
and link the platform/application target to `cp0_system_keys`. Include
`system_keys/bridge.h` and keep one Bridge alive for the interactive UI lifetime,
after LVGL init and before input begins. Destroy it before deleting LVGL/display.

The raw EV_KEY reader must call `platform::route_system_key_event(code,value)`
before text decoding, for press, repeat and release. Use the corresponding
SystemKeyListener APIs in the example apps' linux_input files. Consume handled
events so the same action cannot reach an editor or run twice. SDL maps its
semantic media/display keys through the same hook. Do not start another reader.
Register after other platform setup but before application shortcut dispatch.

Runtime dependencies: `libpulse0` and `libglib2.0-bin`, plus the existing user
Pulse/PipeWire and session D-Bus services. Brightness uses the existing device
backlight permissions; this component adds none. MPRIS media actions only target
one registered player. Do not promise unsupported player integrations.

Run the included tests with `cmake -S . -B build`, `cmake --build build`, then
`ctest --test-dir build --output-on-failure`. The tests use a fake backend and
change no system settings. Test the integration in each app too, particularly
while typing, with Help open, on release/disconnect and under repeated input.
