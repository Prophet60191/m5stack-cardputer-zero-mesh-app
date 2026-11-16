# Cardputer Zero Mesh

Native C++/LVGL UI (`src/`) talks to a Python radio supervisor (`supervisor/`)
over newline JSON. The UI owns no radio hardware. Read `CHANGELOG.md`,
`docs/security.md`, and `docs/review-remediation.md` before changing lifecycle,
privacy settings, protocol, or package integration.

- UI/build code is MIT; the supervisor and guardian/firewall helpers are GPL.
  Preserve upstream M5Stack attribution and both license texts.
- Test commands and build requirements are in `docs/build.md`. Regression tests
  use synthetic data, real LVGL/protobufs, and fake radio/sysfs controls.
- Device builds/package validation need a matching Debian arm64 environment.
  Inspect the actual package, sudo policy, and service files.
- Never put channel keys, share URLs, runtime state, logs, personal device IDs,
  addresses, or real chats into fixtures/screenshots. Known public default-key
  constants and synthetic keys in tests are intentional validation inputs.
- Keep hardware qualification distinct from simulator/container results. Do not
  power a Cap without its antenna; do not deploy, publish, or transmit RF as a
  side effect of running the offline regression suite.
