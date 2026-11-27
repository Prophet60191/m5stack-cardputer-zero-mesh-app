# History and background reliability review — 2026-11-27

This focused pass examines message/delivery state, persistence failures and
unanswered SDK requests. It follows the separate 0.1.25 traceroute/radio pass;
both jobs for that source (31d3b43, run 46018922395) now pass. It is not an
exhaustive security or long-term hardware qualification.

## Reproduced defects and fixes

1. **High: failed timeout persistence can kill radio monitoring.** `_delivery`
   popped the attempt and changed RAM state, then raised on a failed SQLite write.
   The timeout path in `_watch_daemon` was outside its health exception handler,
   so its thread could exit permanently. Live outcomes now survive write failures.
2. **Medium: received messages and ACK results can disappear from the live UI.**
   `_record`/`_delivery` tried persistence before publishing events. A failed
   write suppressed the event even though the message or ACK had actually arrived.
   Bounded RAM state and live events now continue, with a fixed memory-only warning
   rather than an exception or invented successful save. Later updates/reads retry
   persistence only. A successful message-history save reports recovery; it does
   not acknowledge separate failed draft edits or replay any radio operation.
3. **Medium: failed deletion/retention can still remove RAM messages.** The old
   paths modified RAM and pending attempts before their store update. A failed
   deletion could then be applied unintentionally by a later save despite the
   reported failure. These operations now commit first or restore the old limit,
   messages and attempts, and return a fixed failure reason.
4. **Medium: opting back into history can partially commit.** The preference,
   messages and each draft/pin were separate transactions. A failure could leave
   history enabled while RAM drafts were absent on disk; an unchanged draft save
   then falsely acknowledged durability. One atomic transaction now commits all
   of those records and updates the store cache only after success.
5. **Low: repeated RF packets reorder server history.** `_record` removed and
   appended duplicate sender/packet IDs, disagreeing with the UI's in-place update
   and unnecessarily rewriting SQLite. Duplicates now leave history and UI alone.
   Explicit outgoing retries still update their existing bubble in place.
6. **Low: unanswered SDK callbacks accumulate for the life of the radio.** Direct
   inspection of the pinned client shows no timeout removal from responseHandlers.
   The existing observing-interface adapter now tracks registrations and expires
   abandoned handlers after 90 seconds, during periodic health checks or later
   registrations. This exceeds the existing 60s send, 20s trace and 3s individual
   admin waits. It is cleanup, not a new RF retry or delivery timeout.

Expiry and SDK decoding share a reentrant lock covering the SDK's handler
lookup/pop/callback chain. It cannot remove a handler between lookup and pop,
remove a newer registration reusing the same packet ID, or fire a timeout callback.
Reader callbacks must not acquire lifecycle ownership; cleanup callers must not
hold the supervisor state lock while waiting for the decoder lock. The official
SDK continues to decode/handle the actual protocol; its pinned dependency is
unchanged. This is a small integration adapter, not a replacement implementation.

## Validation

Real SQLite TEMP triggers abort inserts/deletes to reproduce rollback failures;
no real device storage is filled or corrupted. The first eight storage tests fail
against 0.1.25 and pass after the fixes. Additional tests cover false durable
acceptance, duplicate UI events and sanitized Diagnostics state. Five SDK tests
exercise real protobuf decoding, 1,000 expired registrations, packet-ID reuse and
concurrent decode/expiry. The native LVGL Diagnostics path checks the failure and
success rows in both themes. Tests open no physical radio and perform no RF send.

Final full gates, package identity and device qualification are pending here.
