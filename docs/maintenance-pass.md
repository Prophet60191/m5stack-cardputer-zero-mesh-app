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
