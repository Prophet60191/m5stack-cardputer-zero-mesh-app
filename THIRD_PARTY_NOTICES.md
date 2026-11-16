# Third-party notices

The app derives from the M5Stack CardputerZero Template (MIT), inherited
CMake baseline 0.2.1: https://github.com/CardputerZero/Template . The original
M5Stack copyright/permission text is retained in LICENSE. Its exact upstream
commit was not recorded in the initial app import.

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
MIT. External meshtasticd and meshtastic==2.7.11 are installed separately,
not redistributed in the package. See NOTICE for project attribution and
trademarks. Not bundling these dependencies is a project distribution choice,
not a claim that GPL prohibits redistribution under its terms.
