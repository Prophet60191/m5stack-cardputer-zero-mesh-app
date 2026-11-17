# Maintenance pass — 2026-11-16

Scope: preserve features, controls, radio/privacy policy and the red selection
border. Review first, then make small changes with synthetic measurements.

| Priority | Observation from the code | Intended improvement |
|---|---|---|
| 1 | Conversation grouping repeatedly searches parallel arrays, selection-sorts all chats, then scans the history again for every search result. | Prepare typed conversation rows in one pass and sort once. Preserve drafts, archived identities, pins, search results and selection. Measure at the 2,000-message cap. |
| 1 | Every `threads` snapshot increments the message revision, even when it matches the current draft/pin/history state. That rebuilds the active LVGL screen. | Compare the effective merged state before notifying; preserve genuine changes, pending local edits and request completion. Measure revision/rebuild counts. |
| 2 | Repeated identical thread saves execute SQLite replacements and broadcast unchanged state. Identity validation is duplicated in save/delete paths. | Validate consistently and skip unchanged writes/broadcasts. Check actual SQLite changes, strict field validation, opt-out and re-enable behavior. |
| 2 | The unused template navbar/icon-button demo still compiles and retains Hello World/counter/bold/info state in the view model. | Remove only code with no live callers. Preserve the current menu, shortcuts, quit action, attribution and deliberate legacy-settings migration. |

The existing LVGL scalar/string subjects already suppress unchanged values;
adding another global comparison layer would duplicate that behavior. Node age
updates still need periodic refresh. Idle-loop timing, object safety assertions,
full message-list virtualization and a broad supervisor rewrite are outside this
pass. They need separate evidence before changing established behavior.

Measurements use wholly synthetic data. `mesh_benchmarks`, built only when
`MESH_BUILD_BENCHMARKS=ON`, is headless and opens no socket, camera, framebuffer or
radio. Timing results describe the measured operations, not battery life or
end-to-end RF delivery. Results and validation are recorded after completion.


## Measured results

Before/after runs used the same Cardputer and wholly synthetic 2,000-message
histories. Values below are median milliseconds from 21 samples after three
warmups. The baseline retained the existing grouping/search/sort rules after
moving them out of the screen for measurement; optimization followed afterward.
No framebuffer, camera, socket or RF operation was used by the benchmark.

| Conversations | List preparation before | After | History search before | After |
|---|---:|---:|---:|---:|
| 32 | 2.361 | 1.895 | 21.682 | 2.132 |
| 256 | 12.855 | 3.368 | 159.961 | 3.095 |
| 2,000 | 204.575 | 15.042 | 1,326.729 | 8.869 |

An unchanged thread-state payload incremented the message revision 104 times in
104 repetitions before this pass, and zero times afterward. With a draft-only
conversation screen present, the median callback cost fell from 2.660 ms to
0.003 ms; this measures that fixture, not every screen or a busy RF mesh.

On the same device, 100 identical draft saves changed 100 SQLite rows before and
zero afterward. Median save time was 5.675 ms before and 0.148 ms after. Durability,
opt-out deletion, row limits and strict field validation remain enabled. The app
also avoids rebroadcasting an already-current thread state.

## Behavior and validation

The UI still renders conversation/channel names, drafts, previews and timestamps;
pins remain first and recent messages follow. Search still finds older messages,
not only the latest preview. Existing archive identity and channel-qualified DM
rules are preserved. Equal-time chats now have stable first-seen order. A selected
chat stays selected when an incoming message reorders the list; missing selections
are still clamped. Request completion is processed even when state did not change.

Regression coverage checks these behaviors, dirty-draft merging, real changes and
deletions, strict Python boolean validation before equality checks, no extra disk
record after quota rejection, and existing radio/privacy/keyboard/lifetime cases.
The unused template widgets and demo-only view-model fields had no live callers;
current menu/quit/theme controls and upstream notices remain. The legacy
`template-app` settings migration is intentional compatibility code and remains.

The package gate builds/runs the optional benchmark and verifies it is not
installed. Native sanitizer checks and backend cases are separate from the
measurements. Hardware input, location and RF qualifications retain their earlier
limits. No claim is made about hours of battery life or reduced idle power.

## Second-pass implementation — 2026-11-17

The owner resumed all seven items after Surveyor finished. The original assessment
below is retained as history; its pending language describes the preparation
stage, not the implementation status here. Source anchors in that assessment
refer to baseline `f1c2c77b9ccb1a26ff366539af7d51f371a3e8af`.

| Item | Implemented result |
|---|---|
| Draft/pin recovery | Retain local edits until a matching `thread.saved` acceptance; protect newer edits/pins from old snapshots/results; bounded retry after transport failure/reconnect; permanent errors stay visible; no RF replay. No-op acceptance performs no extra SQLite write or snapshot. History opt-out accepts RAM-only state explicitly. Graceful exit shares a two-second save/sync deadline. |
| Stable selections | Nodes, open details, recipient picker and message bubbles resolve stable identities across reorder/trim. Missing targets are cleared or unavailable, never silently used for a send/favourite/delete action. |
| Live facts and freshness | Observe RX/TX mode separately from Online; show `Online ?` for unconfirmed mode. A UI-thread presentation clock updates age labels, detail readings, map colors and Active membership without requiring another radio packet. Preserve open-detail scroll and object-bound observer cleanup. |
| Historical attribution | Message details use stored channel name/identity/privacy, with an explicit legacy privacy-unknown fallback. A reused numeric slot cannot relabel the old message. Routing/share validation is unchanged. |
| Refresh work | Suppress equal normalized node snapshots; update Settings values, navigation styles and bubble borders in place; coalesce initial Messages subscriptions. Refresh peer names only when identity/name presentation changes. |
| Retention | Count retained messages in one reverse pass and move kept records once, preserving exact IDs/order/caps. Publish one resulting revision rather than a revision per thread. |
| Device status | Collect only displayed battery facts; remove unused Ethernet/Wi-Fi polling. Exercise the production filesystem reader against temporary synthetic roots. |

The simplification review accepted a lightweight conversation identity record
instead of three parallel arrays, one small list-selection appearance helper,
and pure shared distance/freshness helpers. It deliberately rejected a general
popup framework, another state-owner class/process, broad supervisor rewrites,
full list virtualization and idle-loop changes: these added indirection or lacked
measurement. Preview text is released after rendering, and unselected rows do
not allocate unused selection styles. Features, licenses, legacy preference
migration, keyboard decoding and radio/privacy policy remain intact.

### Regression evidence

The original source passed its 98 backend cases and native sanitizer baseline.
Four new native cases then failed on that original implementation: rejected-save
draft loss, selected-node retargeting after reorder, stale RX/TX with Online
unchanged, and selected-bubble retargeting after trim. They pass after correction.

The final local source passes all 102 backend tests, macOS ASAN/UBSAN CTests,
Debian 13 arm64 ASAN/UBSAN maintenance and production-reader keyboard CTests,
and the isolated arm64 Release/package/fresh-install/actual 0.1.8 upgrade and
firewall/policy gate. Leak detection remains disabled for LVGL global caches;
this is not a general leak audit. The maintenance helper also passes headlessly
on the actual device using installed fonts and synthetic fixtures. It opens no
radio socket, framebuffer or camera.

Additional cases cover stale/malformed/late acceptance, RAM-only persistence,
failed commits, deletion, last-accepted-write policy across two clients, missing
nodes, picker identity, actual public-channel controls/readback, historical
channels, no node-event freshness transitions, filter boundaries, preserved
scroll, destroyed screens, unchanged snapshots/row lifetimes and battery errors.
A reference retention comparison checks 2,000-message fixtures across 1/32/256/
2,000 identities and caps 0/1/3/100. Timing is not a correctness threshold.

The shared app-builder guide records the reusable acceptance/identity/clock/
refresh/style-allocation lessons. Its 22 tests pass against pinned official Store
policies. The reviewed 0.1.12 Markdown and regenerated offline PDFs retain the
explicit hardware qualification limits. Exact-source CI, final installed-runtime
checks and device measurements are recorded in the follow-up below when complete.

### Device measurements and installed candidate

A sequential before/after run used baseline `f1c2c77` and functional source
`2e0035c`, the identical synthetic fixture, Debian arm64 Release flags, installed
device fonts and the same stationary Cardputer. Each median uses seven samples
after three warmups; both programs ran as the ordinary user at Nice 10. The
fixture prepares 2,000 messages and then applies cap 1 across the listed distinct
conversation identities. Retained IDs/order are checked separately in regressions.

| Distinct identities | Retention before (ms) | After (ms) |
|---|---:|---:|
| 1 | 6.493 | 3.456 |
| 32 | 82.327 | 3.681 |
| 256 | 741.860 | 5.325 |
| 2,000 | 2,985.986 | 13.248 |

A down/up navigation round trip on a 32-conversation list fell from **185.536 ms**
to **2.027 ms**. This is headless screen navigation/layout work using actual fonts,
not physical-key-to-display latency. The paired run deliberately stops each
synthetic program after that subset; no paired 256-conversation navigation or
whole-benchmark result is claimed. Earlier broad/overlapping attempts were
excluded, including a baseline that hit its three-minute limit. These figures
do not measure idle power, battery life or RF delivery.

Both [code CI jobs](https://github.com/Prophet60191/m5stack-cardputer-zero-mesh-app/actions/runs/44131291096)
and [guide rendering](https://github.com/Prophet60191/m5stack-cardputer-zero-mesh-app/actions/runs/44131291205)
passed on exact functional source
`2e0035c65cdd1f8b0f45d1c48b2aa45bf27fec37`. Non-publishing Store preflight has
zero errors. Installed archive `cardputerzero-mesh_0.1.12-rev1_arm64.deb` SHA-256:
`13a219c4662c56f09dee3060c3d4db8037dd49698426e7fb00b4bc3471608a3c`.
All **98** regular package files match hashes, root ownership and modes, and
`dpkg --verify` is clean. Later documentation-only closeout is separate from
this runtime/package source.

Private copied/checksummed backups and a consistent SQLite integrity check
preceded installation. Both saved profiles, key/channel configuration, identity
secret, GPS/TX/privacy/tuning settings and UI preferences match. All 19 saved
message rows, three draft/pin thread rows and preferences survived exactly.
`prefs/transmit_history.dat` changed during normal daemon stop/start and is
bookkeeping, not configuration; it was retained in the backup and excluded from
the immutable-setting comparison. No test message, position request or trace was
sent. The previous Online/receive-only state was restored without enabling
transmission, GPS, automatic location or MQTT.

The live new supervisor accepts an unchanged existing thread with its matching
`thread.saved` reply and `durable:true`, then passes `state.sync`; saved rows stay
unchanged. The exact CI keyboard helper passes on the ordinary-user device with
the official keymap fixture. Controlled installed-UI startup verifies the exact
live binary, `/dev/fb0`, built-in keyboard descriptor and NoNewPrivileges. The
launcher was restored without restarting the supervisor. This controlled
startup/stop is distinct from a physical graceful-exit test. The owner separately
confirmed F/X navigation in Settings/Messages, red bubble selection and Esc return.

The 39-page manual and one-page quick reference were regenerated by successful
CI, rendered and visually reviewed. Exact CI copies replace the Desktop offline
PDFs with the older copies archived. The general guide's
[22-test CI](https://github.com/Prophet60191/m5stack-cardputer-zero-app-guide/actions/runs/44131291049)
passes on `09da3ea`; its Surveyor lessons were preserved.

A ten-minute installed-build check collected **61 healthy samples, zero failures
and zero supervisor restarts** in Online/receive-only mode. The maximum trusted
firewall-verdict age was 2.03 seconds. This is bounded post-maintenance evidence;
it does not replace outdoor GPS, public-peer RF or physical IPv6 qualification.
ADB and hotplug remain active/enabled under the owner's instruction. Four
hash-matching inactive temporary test executables were removed; private evidence
and recovery backups remain outside the public repositories. No Surveyor source,
shared driver/network policy, hardware configuration or backup timer was changed.
All seven maintenance items are complete. GPS dongle support is the next,
separate task; receiver model/source selection and a real outdoor fix remain open.

## Deferred second pass — 2026-11-16

**Status: resumed by the owner on 2026-11-17 after Surveyor finished.** The
following is the recorded assessment and acceptance plan; implementation results
will be recorded separately. Preserve Surveyor, shared resource ownership and
saved mesh profiles and the active setup. GPS dongle support follows this maintenance task.

Baseline: Mesh 0.1.11, commit `f1c2c77b9ccb1a26ff366539af7d51f371a3e8af`.
The source tree was clean during the assessment. Findings below come from
reading the actual code and existing tests. They have not been reproduced in new
test runs, fixed or benchmarked. The measured first-pass results above remain a
separate record; they do not establish gains for these proposed changes.
Source line numbers below refer to this baseline and may move during fixes.

### Reliability first

1. **High priority: keep unconfirmed draft changes pending.**
   [`BaseViewModel::flush_drafts()`](https://github.com/Prophet60191/m5stack-cardputer-zero-mesh-app/blob/f1c2c77b9ccb1a26ff366539af7d51f371a3e8af/src/viewmodel/base_viewmodel.cpp#L915)
   clears `dirty_drafts_` before sending and ignores the result; `send_thread_state()`
   also returns no acceptance result. `apply_threads()` only protects drafts
   still marked dirty. Editing during a disconnect, leaving the typing box and
   reconnecting can therefore replace newer unsaved text with an older supervisor
   snapshot. Keep pending local edits protected until matching supervisor
   acceptance; handle transport rejection and lost connections explicitly.
   Recovery concerns local conversation state only, never replaying RF sends.
   Verify rejected/full transport queues, accepted-then-disconnected writes,
   stale echoes, newer edits during an in-flight save, multiple clients, clear/
   delete actions, history opt-out and graceful exit. Preserve debounce and
   idempotence; avoid an unbounded retry or notice loop.

2. **Medium priority: preserve selections by stable identity.**
   [`NodesScreen::rebuild_display()`](https://github.com/Prophet60191/m5stack-cardputer-zero-mesh-app/blob/f1c2c77b9ccb1a26ff366539af7d51f371a3e8af/src/view/screens/nodes_screen.cpp#L105)
   reorders nodes but retains a numeric selection. An incoming update can move
   the highlight to another person before the next Message/Favourite action.
   [`MessagesScreen::rebuild_thread()`](https://github.com/Prophet60191/m5stack-cardputer-zero-mesh-app/blob/f1c2c77b9ccb1a26ff366539af7d51f371a3e8af/src/view/screens/messages_screen.cpp#L302)
   similarly retains a numeric bubble selection when older messages are removed.
   Retention can move the highlight and subsequent actions to another message.
   Preserve the selected node ID and message identity across refreshes; define
   safe behavior when the selected item disappears. Verify reordering, filtering,
   retention, deletion, an open action menu and an open node detail. Keep the
   existing stable conversation-list selection and red bubble border.

3. **Medium priority: refresh live status and age displays explicitly.**
   [`BaseViewModel::apply_health()`](https://github.com/Prophet60191/m5stack-cardputer-zero-mesh-app/blob/f1c2c77b9ccb1a26ff366539af7d51f371a3e8af/src/viewmodel/base_viewmodel.cpp#L741)
   changes the radio mode without notifying observers. The
   [title bar](https://github.com/Prophet60191/m5stack-cardputer-zero-mesh-app/blob/f1c2c77b9ccb1a26ff366539af7d51f371a3e8af/src/view/widgets/titlebar.cpp#L123) observes radio-state/theme
   changes, so changing receive-only/transmit mode while remaining Online can
   leave the old RX or TX/RX label visible. The
   [node refresh callback](https://github.com/Prophet60191/m5stack-cardputer-zero-mesh-app/blob/f1c2c77b9ccb1a26ff366539af7d51f371a3e8af/src/view/screens/nodes_screen.cpp#L98) rebuilds the
   underlying list but does not refresh an open detail's readings or fix age.
   The [map](https://github.com/Prophet60191/m5stack-cardputer-zero-mesh-app/blob/f1c2c77b9ccb1a26ff366539af7d51f371a3e8af/src/view/screens/map_screen.cpp#L89) updates freshness on node
   events only; with the radio stopped or supervisor disconnected, a displayed
   fix can continue to look fresh after its age threshold. Notify actual mode
   changes and refresh time-dependent presentation without requiring new RF data
   or rebuilding unrelated objects. Verify RX-to-TX and TX-to-RX with unchanged
   Online state, updated telemetry in an open detail, and stale/future/no-fix
   boundaries with a controlled clock while offline. Preserve detail identity,
   scrolling and the supervisor's independent send-position freshness checks.

4. **Medium priority: retain historical channel attribution in message details.**
   [`MessagesScreen::details_text()`](https://github.com/Prophet60191/m5stack-cardputer-zero-mesh-app/blob/f1c2c77b9ccb1a26ff366539af7d51f371a3e8af/src/view/screens/messages_screen.cpp#L744)
   labels an old message using the channel currently occupying its numeric slot.
   Replacing that slot can attribute the old message to the replacement channel.
   Use the message's stored channel identity/name/classification, with an explicit
   fallback for legacy records. Verify slot replacement, slot movement, archived
   channels and legacy history; preserve existing routing and share-code checks.

### Measure before optimizing

5. **Reduce unnecessary object destruction and rebuilding.**
   [Message selection](https://github.com/Prophet60191/m5stack-cardputer-zero-mesh-app/blob/f1c2c77b9ccb1a26ff366539af7d51f371a3e8af/src/view/screens/messages_screen.cpp#L552) calls a
   [full conversation rebuild](https://github.com/Prophet60191/m5stack-cardputer-zero-mesh-app/blob/f1c2c77b9ccb1a26ff366539af7d51f371a3e8af/src/view/screens/messages_screen.cpp#L291).
   [Node snapshots](https://github.com/Prophet60191/m5stack-cardputer-zero-mesh-app/blob/f1c2c77b9ccb1a26ff366539af7d51f371a3e8af/src/viewmodel/base_viewmodel.cpp#L182) always publish a new
   revision, and [Settings observers](https://github.com/Prophet60191/m5stack-cardputer-zero-mesh-app/blob/f1c2c77b9ccb1a26ff366539af7d51f371a3e8af/src/view/screens/settings_screen.cpp#L94)
   each trigger full-list refreshes, including immediate subscription callbacks.
   Measure redraw/allocation counts and latency for navigation, unchanged
   snapshots and grouped state changes. Prefer updating affected objects and
   combining related refreshes. Keep age/filter refreshes even when payloads
   match; preserve scroll position, focus, overlay ownership and object lifetime.
   Full list virtualization needs separate evidence before adoption.

6. **Trim history in one pass.**
   [`trim_all_threads()`](https://github.com/Prophet60191/m5stack-cardputer-zero-mesh-app/blob/f1c2c77b9ccb1a26ff366539af7d51f371a3e8af/src/viewmodel/base_viewmodel.cpp#L607) enumerates
   threads and scans the history again for each through `trim_thread()`. At a
   nonzero cap, work grows with messages times conversations. Measure synthetic
   2,000-message cases, then consider one bounded pass retaining the newest
   messages per identity and one final change notification. Verify ordering,
   per-chat/global caps, cap zero, archived identities, unread counts and the
   selected message; compare retained IDs with the existing behavior.

7. **Avoid collecting unused network status.**
   The [title bar](https://github.com/Prophet60191/m5stack-cardputer-zero-mesh-app/blob/f1c2c77b9ccb1a26ff366539af7d51f371a3e8af/src/view/widgets/titlebar.cpp#L146) displays battery fields
   from `read_device_status()`, whose
   [device path](https://github.com/Prophet60191/m5stack-cardputer-zero-mesh-app/blob/f1c2c77b9ccb1a26ff366539af7d51f371a3e8af/src/platform/device_status.cpp#L148) also reads network sysfs
   and wireless status. No other current app caller consumes those network
   fields. Consider a battery-only read or removal of unused collection after
   confirming callers. Verify charging, missing battery/read errors and desktop
   behavior. Measure the small IO reduction; make no battery-life claim.

### Simplification without losing behavior

- Distinguish source complexity, installed size, runtime memory and CPU/IO work.
  Smaller source files alone do not establish a smaller or faster app. Record
  relevant baselines and compare the same synthetic scenarios after each change.
- Consolidate genuinely duplicated visual construction, formatting and pure
  calculations into small existing or purpose-specific helpers. Keep screen
  navigation, confirmations and resource ownership explicit. Add an abstraction
  only when it makes the actual callers easier to understand and test.
- Use typed state/records where they remove parallel arrays or ambiguous string
  dispatch. Separate a large responsibility when it can be tested independently;
  moving the same logic into more files is not itself an improvement.
- Delete code, assets or dependencies only after checking runtime/build/package
  callers and compatibility needs. Keep deliberate legacy-settings migration,
  user documentation, attribution and license notices.
- Preserve the native UI/Python supervisor boundary, keyboard contract, validated
  identities, persistence guarantees, bounded queues and lifecycle/firewall/
  privacy safeguards. Validation at different trust boundaries is intentional;
  do not remove it merely because similar checks appear elsewhere.
- Keep this a Mesh maintenance pass. A new framework, general component library,
  language rewrite or feature reduction is not part of this backlog.

### Resume and acceptance

Re-read `AGENTS.md`, the current changelog and this backlog; check for intervening
work and coordinate device ownership. Reproduce each reliability issue with a
focused regression before changing it. Address items 1–4 first, then measure and
select improvements from 5–7. Use small reviewable changes and record actual
results, including opportunities rejected because the benefit was negligible.

Run the relevant native sanitizer/input/UI and backend checks, then the isolated
matching Debian arm64 package/install/upgrade/firewall gate described in
[`build.md`](build.md). Use separate Mesh build folders; do not alter Surveyor's
environment or shared device settings. Hardware checks and installation wait for
device access and require preserving private profiles, saved chats/drafts and
the owner's standing ADB preference. Keep the earlier outdoor GPS/public-peer/
physical-IPv6 qualification limits distinct from this maintenance work. Passing
local checks is not a promise of zero regressions or proof of on-device gains.

### Preparation review and implementation order

The follow-up source/test review confirmed an important constraint for item 1:
the [local state dispatcher](https://github.com/Prophet60191/m5stack-cardputer-zero-mesh-app/blob/f1c2c77b9ccb1a26ff366539af7d51f371a3e8af/supervisor/supervisor.py#L868) deliberately sends
no echo for an unchanged `thread.state`. The
[protocol](supervisor-protocol.md) documents that behavior, and
[`MaintenanceTests`](../tests/test_maintenance.py) protects the absence of repeated
writes/broadcasts. Merely retaining a dirty flag until a broadcast arrives would
leave a successfully saved, unchanged draft pending indefinitely. A successful
transport enqueue also does not establish supervisor acceptance.

For the implementation, prefer a small correlated acknowledgment of validated
local state, including an unchanged save. Keep it separate from the generic
Settings service-request slot and from RF delivery acknowledgments. Tie it to
the submitted edit/request so a late result cannot clear a newer pending edit.
Keep full-state broadcasts and disk writes suppressed when nothing changed.
Specify whether acceptance is RAM-only or durable under the current history
preference; do not imply a disk save while history is disabled. Reconcile stale
connection results and multiple clients explicitly. Finalize and test the
backward-compatible contract when implementing; the published protocol is
unchanged by this preparation note. Preserve `state.sync` for graceful exit and
keep local saves independent of radio/admin waits.

Each batch below is **pending**. Existing tests are starting points, not proof
that the missing cases pass. Run the baseline, add meaningful failure cases,
make the bounded change, then check the affected suite before proceeding.

| Order | Change boundary | Existing starting point | Missing acceptance cases |
|---|---|---|---|
| 1 | Draft/pin synchronization in the view model, local dispatcher and event adapter; minimal protocol addition only if needed | `unchanged_thread_regression()` in [native regressions](../tests/regressions.cpp); [maintenance tests](../tests/test_maintenance.py); `test_drafts_do_not_fill_radio_queue_during_admin_wait()` in [follow-up tests](../tests/test_review_followup.py) | Rejected enqueue preserves text across reconnect; unchanged accepted save completes without a disk write/full broadcast; a late result cannot acknowledge a newer edit; connection loss after enqueue preserves uncertainty; retries stay bounded; deletion/history erasure cannot resurrect old state; no RF intent is replayed |
| 2 | Node and bubble selection, followed by historical message attribution | `maintenance_data_regression()`, `identity_confirmation_regression()` and `dual_channel_ui_regression()` in [native regressions](../tests/regressions.cpp) | Reordering retains the selected node/action target; trimming older messages retains the selected message; disappearance has an explicit safe outcome; open menus/details keep their target; slot replacement/movement leaves old message details attributed to the original identity |
| 3 | Radio-mode notification and time-dependent node/map presentation | Existing real-LVGL screens in [native regressions](../tests/regressions.cpp); `test_gps_freshness_and_invalid_coordinates()` in [backend tests](../tests/test_application_services.py) | RX/TX changes appear with Online unchanged; open details update readings without jumping scroll; map/detail age crosses the stale threshold without a node event; stale/future/no-fix behavior remains correct after disconnect/stop; destroying a screen cancels its refresh callback |
| 4 | Refresh suppression/coalescing and one-pass retention, considered separately | [Optional headless benchmark](../tests/maintenance_benchmarks.cpp), unchanged-state checks and existing history/retention tests | Count real rebuilds/objects and revisions; preserve all list order, selected identity, scrolling and footer/border output; test unchanged snapshots across age/filter boundaries; compare retained IDs, order and unread behavior against the current retention algorithm |
| 5 | Battery-only collection and small duplication cleanup | [Device status reader](../src/platform/device_status.cpp), existing UI/layout and Settings-file tests | Exercise the production reader against synthetic filesystem roots; verify battery/charging/missing/error cases without host sysfs; preserve desktop status; confirm unused network reads are gone; verify affected popup/key/focus behavior and all packaged resources |

For timing work, use the same synthetic data, build configuration and machine
before and after each change. Cover empty, typical and maximum-bound cases:
message histories up to 2,000, node lists up to 256, and all current Settings
rows. Record distributions/medians and rebuild/revision counts separately from
timing. Preserve the existing list/search and no-op-write measurements. Keep
timing thresholds out of correctness tests; use deterministic content/state/
operation-count assertions. Desktop measurements can select candidates, while
device performance and battery claims require later hardware evidence.

### Concrete simplification candidates

These are review candidates, not approved removals or promised size savings:

- Reuse the existing `ConversationRow` record in
  [`MessagesScreen`](../src/view/screens/messages_screen.h) instead of copying
  its key, peer and name into three parallel vectors. Keep selection bound to
  the record's identity and preserve every displayed field.
- Compare the repeated popup/list construction in
  [`messages_screen.cpp`](../src/view/screens/messages_screen.cpp) and
  [`settings_screen.cpp`](../src/view/screens/settings_screen.cpp). Share small
  visual helpers where callers stay simpler. Keep their distinct confirmation,
  editing, cancellation and focus behavior in the owning screen.
- Review the duplicated distance formatting/calculation in
  [`nodes_screen.cpp`](../src/view/screens/nodes_screen.cpp) and
  [`map_screen.cpp`](../src/view/screens/map_screen.cpp) for a small pure helper.
  Retain geographic validation and roundoff protection; verify zero-distance and
  near-antipodal coordinates before replacing either implementation.
- Consider separating pending conversation-state synchronization from the broad
  view model only if it reduces coupled state and permits independent tests.
  Do not add a new process, persistence owner, dependency or general framework.

Accept a cleanup when it removes duplicated behavior, unnecessary work or
ambiguous ownership and retains testable behavior. Reject an extraction that
only moves code or requires more indirection at every caller. Do not use an
arbitrary line-count reduction as the acceptance target.

Before release, run the full native sanitizer/input/UI and backend gates and
the isolated Debian arm64 package/fresh-install/upgrade/policy checks. Review
the exact package and source correspondence, then verify the installed build
and affected keyboard/navigation, draft persistence, status/freshness and radio
stability on the available device. Preserve private settings/history and use
synthetic data for screenshots and benchmark fixtures. Document each actual
result and any remaining physical qualification; leave release/deployment
unclaimed until completed. No new test, build or device result is recorded by
this documentation-only preparation.
