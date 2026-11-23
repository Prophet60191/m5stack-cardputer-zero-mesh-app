# Third-party notices

The app derives from the M5Stack CardputerZero Template (MIT), inherited
CMake baseline 0.2.1: https://github.com/CardputerZero/Template . The original
M5Stack copyright/permission text is retained in LICENSE. Its exact upstream
commit was not recorded in the initial app import.

The complete CardputerZero symbol assignments in keyboard_decoder.cpp and the
independent test fixture follow M5Stack's MIT-licensed launcher keymap at
4c05b444cb977cb90e2f33eb4c8261226e370713 (Copyright 2026 M5Stack Technology CO LTD):
https://github.com/CardputerZero/launcher/blob/4c05b444cb977cb90e2f33eb4c8261226e370713/ext_components/cp0_lvgl/src/cp0/cp0_keyboard_keymap.c .
The M5Stack MIT permission text is retained in LICENSE; provenance and the
unaltered system-map fixture hash are recorded in tests/fixtures/README.md.

LVGL is pinned to v9.5.0 (85aa60d18b3d5e5588d7b247abf90198f07c8a63),
https://github.com/lvgl/lvgl . Full upstream notices are in licenses/:

- LVGL: MIT, LVGL.txt.
- ThorVG bundled with that LVGL revision: MIT, ThorVG.txt.
- Its RapidJSON headers (1.1.0): MIT and additional notices, RapidJSON.txt.
- Project Nayuki QR generator: MIT, QR-Code-generator.txt.
- ChaN TJpgDec: permissive custom terms, TJpgDec.txt.
- TLSF allocator: BSD terms, TLSF.txt; printf implementation: sprintf.txt.
- Montserrat compiled font data: SIL OFL 1.1, Montserrat-OFL.txt.
- FreeType: FreeType.txt (a system library, not vendored by this app).

Font/icon files: assets/fonts/LICENSE.txt preserves the Phosphor Icons MIT
notice and Kenney Input Prompts CC0 dedication. Noto Sans CJK is loaded from
the system on the device and is not bundled. Artwork under assets/images and
the click sound were inherited from the MIT template; mesh.png is app artwork.

The Debian package dynamically uses the platform's FreeType, libpng, JPEG,
zlib, fmt and standard C/C++ libraries; it depends on their distro packages
rather than shipping copies. Desktop-only SDL2 uses the zlib license. Refer
to those packages' copyright/license files for their build-specific terms.

Radio supervisor and guardian/firewall helpers: GPL-3.0-or-later; the full
text is supervisor/LICENSE.GPL and the Python sources are shipped in the
package. cap_lora.py, qr_scan.py and the retired radio_up.py entry point are
MIT. Starting with 0.1.19, the package redistributes a private native
meshtasticd and hash-pinned Python wheels. Their original license files are
installed under `/usr/lib/cardputerzero-mesh/runtime/licenses/`; the runtime
manifest records exact sources, patches and hashes. Release assets must include
the complete corresponding-source/dependency archive, not only the installer.
See [distribution](docs/distribution.md). The MIT UI and GPL runtime remain
separate processes; redistribution preserves each component's own terms.

## Optional phone transport

Tailscale is installed and administered separately; no Tailscale binary or source
is bundled in this package. Phone access uses its documented private Serve/PROXY
protocol interface. The official Meshtastic phone application is also separate.
Consult their own licenses and terms; the existing UI/supervisor license split
is unchanged.

The native GPS compatibility build under `scripts/meshtasticd-gps/`
rebuilds the package-private Meshtastic firmware/meshtasticd from pinned official
GPL-3.0-or-later source with an identification-timeout patch. It is not linked
into the MIT UI. Native artifacts must retain attribution and their paired full
corresponding-source/dependency archive; the scripts document the version/hash
and the qualification boundary. Historical standalone package instructions remain for 0.1.18 rollback only. No upstream submission is
implied by retaining a local downstream patch.
