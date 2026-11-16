# Review report verification — 0.1.6

The supplied report described 0.1.4 source. Each claim was checked against the
actual current code, independently of the report's conclusions. The table keeps
the report's numbering. All fixtures are synthetic; offline checks use fake
radio/sysfs controls and real pinned protobufs, LVGL and SQLite.

| Item | Verification and correction | Evidence |
|---|---|---|
| 1. Duplicate scan never reaches the zero-result branch | Confirmed. The importer raised for an all-existing URL. It now returns zero without writes/readback; the scan/paste preview reports no change. | Real encoded ChannelSet, real importer and duplicate preview; no mocked zero import result. |
| 2. Leaving a scan can import silently or queue another scan | Confirmed behavior. Capture now only creates a preview. Escape, disconnect and screen destruction cancel capture/discard preview. One camera job is allowed; another is refused rather than queued. Confirmed imports announce completion to connected clients even after leaving their view. | Cancellation, stale request, screen destruction, queued-preview cancellation and process-group tests. |
| 3. Camera/import stalls the radio worker and drafts fill its queue | Confirmed serialization; the report overstates draft frequency: 650 ms is a pause debounce, not a periodic write while typing. Camera capture is now a separate bounded cancellable job. Local draft/history writes use the state lock rather than the radio queue. Confirmed admin writes stay serialized for radio consistency. Errors already produced toasts; waiting views now also have deadlines. | Status request completes during capture; 30 draft writes commit during a blocked admin operation without filling the radio queue. |
| 4. Scan omits its Online requirement | Confirmed. Ready instructions state it; Return checks it before capture. The supervisor independently rejects an offline scan. | Native ready/offline/start tests. |
| 5. Four independent version literals | Confirmed. `app-builder.json` supplies CMake's project/UI/package version and the supervisor's installed/source metadata reader. The C++ fallback literal is removed. | Metadata regression and actual Debian package build. |
| 6. Delete target overloads request `id` | Confirmed protocol ambiguity. The target is now `message_id`; `id` is reserved for request correlation. Upgrade UI and supervisor together. | Deleting one message with a different request ID preserves the other message and echoes the request ID. |
| 7. Duplicate greeting history | Confirmed; the UI also requested another copy. Greeting now queues one history snapshot and the UI does not request a duplicate. An unchanged retention preference no longer republishes history; changed retention can legitimately publish a trimmed snapshot. | Actual greeting/snapshot count regression. |
| 8. Slow greeting dependency probes under lifecycle lock | Confirmed for `dpkg-query`; the firewall check is already a fresh local trusted-file read, not a privileged subprocess. Daemon versions are cached for 60 seconds; greetings warm the cache outside the lifecycle lock. Every radio start forces a fresh compatibility query. A cold-cache greeting may still wait for that bounded query. | Cache count, lock-ownership and fresh-query tests. Fresh firewall validation is preserved. |
| 9. Status can throw while dereferencing the interface | Confirmed defensive weakness: mode was read outside the exception guard. Most production callers already hold the lifecycle lock, so the particular unlocked race is not established. Status now captures the interface/state and guards all library reads; an incomplete node yields Unknown. | Detaching the live interface during payload construction and an incomplete-library-node test. |
| 10. History Off retains identities/pins | Confirmed. Opt-out now deletes message and thread rows, checkpoints WAL and suppresses future thread metadata writes. Startup also removes metadata left by the old Off policy. Current RAM state remains available; re-enabling saves it. This is logical deletion, not forensic flash/backup erasure. | Raw thread-row count, restart migration, opt-out writes and RAM re-enable regressions. |
| 11. Unused app sudo grant | Runtime-unused, but previously documented for manual diagnosis. Removed the grant and sudo package dependency; the capability-only monitor remains. An unprivileged `firewall.py health` command replaces the manual sudo check. Legacy platform permissions/groups are preserved. | Package asserts no app sudoers file; health command cannot spawn a privileged process. Installed-upgrade verification is separate. |
| 12. Generic Working view has no deadline | Confirmed. Generic services expire after 60 seconds (90 for confirmed imports), report uncertainty on timeout/disconnect and ignore late replies. Validation errors echo a valid ID; an ID-less error naming the pending intent also finishes that view. Return cannot accidentally dismiss a busy view. | Native timeout, validation-error, disconnect, repeated Return and late-reply regressions. |
| 13. Import has no preview | Accurate behavior and a useful consent improvement, rather than evidence of command injection. Both pasted and scanned URLs now require a channel-name/classification preview and Cancel-first confirmation. Tokens are connection-bound, expire after 120 seconds and reject changed channel/tuning state. New channels retain location/MQTT Off and existing tuning. Secrets stay in supervisor RAM. | Real preview/import, consent, expiry, connection binding, state-change, no-write-before-confirm and secret-exclusion tests. |

Implementation is in [the supervisor](../supervisor/supervisor.py),
[channel import](../supervisor/channel_import.py), [state storage](../supervisor/state_store.py),
[application services](../supervisor/application_services.py),
[the view model](../src/viewmodel/base_viewmodel.cpp) and
[Settings](../src/view/screens/settings_screen.cpp). The updated contract is in
[the protocol](supervisor-protocol.md); [security.md](security.md) describes the
remaining trust boundaries and deletion limits.

Regression evidence: [backend follow-up cases](../tests/test_review_followup.py),
[persistence/application cases](../tests/test_application_services.py),
[existing backend cases](../tests/test_supervisor.py),
[native UI checks/renders](../tests/regressions.cpp) and
[Debian package validation](../tests/linux-package-check.sh).
Camera cancellation uses harmless real child processes in tests; it is not a
claim of a new physical camera/RF/GPS/cold-power qualification.
