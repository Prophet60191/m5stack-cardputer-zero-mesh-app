# 0.1.2 implementation and qualification

> Historical/versioned record. For 0.1.19 bundled-runtime installation use the
> [current runbook](install.md); for current source/package evidence see
> [qualification](distribution-validation-20261123.md). Earlier standalone
> daemon/manual-venv and pending-state instructions below describe their named version.


Authorized scope: the twelve-item follow-up requested on 2026-11-15. This file is
the durable checklist; an item is complete only with implementation and evidence.
Real hardware checks are separate from synthetic regressions. Keep device IDs,
private profiles, keys, coordinates and raw logs out of the publication tree.

| Work | Acceptance | Status |
|---|---|---|
| Radio falls into Error | Identify actual failure, preserve reason, repair lifecycle/health checks; sustained on-device run | Three actual causes repaired; actual retunes/crash recovery pass; installed radio ran 30m12s with 119 healthy samples and zero failures/restarts |
| Peer/GPS/camera/cold boot | Actual bidirectional peer delivery, GPS fix/freshness, camera QR, physical power cycle and firewall-before-radio | Private ACK + matching reply, actual CSI QR and physical cold power/firewall order passed; outdoor GPS deferred by operator |
| arm64 package CI | Debian 13 arm64 build/install/policy validation and checksummed artifact on GitHub | Both final functional-source CI jobs passed; downloaded arm64 checksums/source verified |
| First-run setup | Fresh empty state can safely initialize receive-only, choose region/channel and explicitly enable TX; dependencies diagnosed | Implemented; actual empty-state RX-only startup/readback passed |
| Channel identity | Opaque stable identity carried through history, drafts, selections and intents; slot replacement archives old conversations | Implemented; slot/key/name/explicit-ID and stale-overlay/retry regressions pass |
| Live radio/node status | TX/RX/Off, API connection, node/telemetry changes and bounded coalesced updates | Implemented; real online/TX/RX readback, remote receive accounting and installed 30-minute radio run passed |
| Delivery results | Mesh vs recipient acknowledgment, preserved failure cause, no false read receipts or replay | Implemented; actual recipient ACK plus matching private peer reply passed |
| GPS freshness | Fix quality/age validated; stale or invalid coordinates cannot be silently sent | Implemented; invalid/stale/future guards and no-fix diagnostics pass; outdoor fix deferred |
| Conversations/drafts | Private bounded durable storage, retention/deletion, draft restoration and crash/restart evidence | Implemented; actual SIGKILL restored committed draft/pin without replay; draft-only reachability and persistence re-enable restart regressions pass |
| Channel manager | Create/import/rename/disable/remove/rotate with readback and identity-safe history | Implemented; actual create/rename/rotate/disable/enable/remove readback passed |
| Named mesh profiles | Private local storage, preview and confirmed offline application; no automatic RF/position/MQTT enabling | Implemented; actual save/preview/offline apply and RX/privacy readback passed |
| Diagnostics | Current health and dependency versions; export excludes secrets, chats, node identities and coordinates | Implemented; safe whitelist regression and actual healthy monitor/API/version reads passed |
| Search/pins/quiet hours | Keyboard-usable search, identity-bound pinned chats and persisted time-window notification policy | Implemented; native keyboard/search/pin and local-time policy checks passed; operator confirmed final settings screens/Esc |

Existing operator profile stays intact unless an explicit configuration action
changes it. Never represent an automated reboot as a physical cold-power test.
Do not send public RF traffic without the intended public test setup.

Current offline evidence: 67 backend tests (one Linux-only skip on macOS; all pass
in Debian arm64), native LVGL keyboard/lifetime/parser/transport and ASAN/UBSAN,
package install/policy and real capability-service/network-namespace validation.
Missing-client setup was separately exercised in an empty venv. The CI workflow
builds a matching package and publishes a checksummed candidate with source ID;
no stable binary release or tag is created automatically.

Device evidence is recorded in [review-remediation.md](review-remediation.md).
Original private keys and regional tuning are preserved. The test GPS receiver
change was restored to its original setting; automatic position and MQTT stay
Off. The final package is installed, its source hashes match, and the temporary
user-unit source override is removed. The packaged radio completed 30m12s with
119 healthy samples, zero failures and zero process restarts. The maximum trusted
firewall-verdict age was 2.01s (limit 7s). The subsequent storage-only history
re-enable correction passed both CI jobs with 67 backend cases. That exact
checksummed CI artifact was then installed: all ten runtime file hashes matched,
saved history/thread state survived, original private keys/tuning/TX/GPS were
preserved, and actual online/config/firewall readback passed. ADB and its hotplug
service are disabled/inactive again, verified over SSH. The normal packaged
service has no temporary source override. No repeat 30-minute radio soak is
claimed for the storage-only follow-up.

Functional-source CI: [67-test run and arm64 artifact](https://github.com/Prophet60191/m5stack-cardputer-zero-mesh-app/actions/runs/43809488405).

Remaining operator-dependent checks: outdoor GPS fix/freshness, physical IPv6 with
a reachable control, public-mesh interoperability with an intended public peer.
The operator verified the added settings screens and Esc on the final UI. Only
private RF tests were authorized and performed in this qualification session.
