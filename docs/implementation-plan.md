# 0.1.2 implementation and qualification

Authorized scope: the twelve-item follow-up requested on 2026-11-15. This file is
the durable checklist; an item is complete only with implementation and evidence.
Real hardware checks are separate from synthetic regressions. Keep device IDs,
private profiles, keys, coordinates and raw logs out of the publication tree.

| Work | Acceptance | Status |
|---|---|---|
| Radio falls into Error | Identify actual failure, preserve reason, repair lifecycle/health checks; sustained on-device run | Confirmed sudo health-check timeout and unkillable elevated helper; replacement monitor passes package tests; device soak pending |
| Peer/GPS/camera/cold boot | Actual bidirectional peer delivery, GPS fix/freshness, camera QR, physical power cycle and firewall-before-radio | Pending; operator confirmed antenna and private peer ready |
| arm64 package CI | Debian 13 arm64 build/install/policy validation and checksummed artifact on GitHub | Implemented; local Debian arm64 gate passes; awaiting pushed workflow run |
| First-run setup | Fresh empty state can safely initialize receive-only, choose region/channel and explicitly enable TX; dependencies diagnosed | Implemented; offline checks pass; final hardware qualification pending |
| Channel identity | Opaque stable identity carried through history, drafts, selections and intents; slot replacement archives old conversations | Implemented; offline checks pass; final hardware qualification pending |
| Live radio/node status | TX/RX/Off, API connection, node/telemetry changes and bounded coalesced updates | Implemented; offline checks pass; final hardware qualification pending |
| Delivery results | Mesh vs recipient acknowledgment, preserved failure cause, no false read receipts or replay | Implemented; offline checks pass; final hardware qualification pending |
| GPS freshness | Fix quality/age validated; stale or invalid coordinates cannot be silently sent | Implemented; offline checks pass; final hardware qualification pending |
| Conversations/drafts | Private bounded durable storage, retention/deletion, draft restoration and crash/restart evidence | Implemented; offline checks pass; final hardware qualification pending |
| Channel manager | Create/import/rename/disable/remove/rotate with readback and identity-safe history | Implemented; offline checks pass; final hardware qualification pending |
| Named mesh profiles | Private local storage, preview and confirmed offline application; no automatic RF/position/MQTT enabling | Implemented; offline checks pass; final hardware qualification pending |
| Diagnostics | Current health and dependency versions; export excludes secrets, chats, node identities and coordinates | Implemented; offline checks pass; final hardware qualification pending |
| Search/pins/quiet hours | Keyboard-usable search, identity-bound pinned chats and persisted time-window notification policy | Implemented; offline checks pass; final hardware qualification pending |

Existing operator profile stays intact unless an explicit configuration action
changes it. Never represent an automated reboot as a physical cold-power test.
Do not send public RF traffic without the intended public test setup.

Current offline evidence: 62 backend tests (one Linux-only skip on macOS; all pass
in the Debian arm64 package container), native LVGL keyboard regression and
ASAN/UBSAN checks. Missing-client setup was separately exercised in an empty venv.
Built-in CSI camera capture was physically verified at 640x480 without saving an
image; QR decoder installed in the radio venv; the built-in CSI camera decoded the exact synthetic target after adjusting its capture deadline to measured device timing.

Device qualification: candidate installed, temporary user-service override removed,
packaged service and capability monitor verified. Real startup exposed vendor
255-valued GPIO brightness rejected by guardian recovery; normalized and covered
by regression. A second real conflict is the NFC SPI-CS2 runtime overlay claiming
GPIO22; preflight now detects it before Cap power rather than allowing a daemon
assertion. Physical cold-power verification, private-peer delivery, outdoor GPS
and the corrected installed-build radio soak remain in progress.
The original private state/configuration is backed up privately. TX is temporarily
Off for qualification; restore the operator's approved original setting afterward.
