# Self-contained distribution work — 2026-11-23

Owner authorized the remaining release-preparation items, a GitHub profile link
on the guide cover, a new available Store code, and reducing manual dependencies.
No Store submission, public release or upstream contribution is authorized here.
The owner will supply the video. Fresh-image hardware testing is explicitly
deferred because no spare microSD is available; never erase the working card.

## Intended result

- One Debian arm64 app package includes the qualified patched native daemon in
  its own private directory and hash-pinned Python wheels. No separate global
  meshtasticd install or manual pip setup. Do not ship a competing system daemon.
- First launch prepares a private managed Python environment from those wheels
  without networking. Keep the old user venv untouched for rollback.
- Debian supplies the system interpreter, libraries, fonts and camera decoder
  libraries through declared dependencies. Optional camera capture drivers and
  Tailscale remain OS/image facilities; no account credentials are bundled.
- Source checkout contains pinned recipes/checksums and one build command;
  build dependencies are fetched automatically and cached. Compiled dependencies
  do not bloat Git. Produce a source/dependency archive alongside the installer
  so downloads are durable release-ready assets, not only expiring CI artifacts.
- Use CZMS only after checking current catalog. Bump the candidate version and
  refresh the app CI to the current reviewed Store checker/catalog.
- Test no-network runtime preparation, missing/corrupt bundles, package fresh
  install/upgrade/remove in clean Debian containers and normal-user startup.
  Separate these from the deferred clean-image hardware check. Preserve actual
  device chats/profiles/TX/GPS/privacy/phone settings if upgrading after gates.
- Prepare a short privacy-safe video checklist; owner records the physical demo.

Release-ready assets and a final submission checklist must be reviewable before
any later request to publish. No temporary or fabricated URLs qualify as public
availability. Keep patched native binary/full corresponding source together.
