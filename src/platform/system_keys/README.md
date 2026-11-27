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

The C ABI optionally supports a Python compatibility adapter. No app source or
firmware is bundled here. Fake-backend tests never change real system settings.
Hardware/input replay, mixer readback and physical-key acceptance are distinct.

Files under upstream/ remain Copyright M5Stack Technology CO LTD, MIT. Local
adapter and tests are MIT. See LICENSE and upstream.json for exact provenance.
