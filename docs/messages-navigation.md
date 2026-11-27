# Messages navigation and loading — 0.1.22

Requested 2026-11-26: separate channel and direct-message lists, newest active
conversations first, one combined search, and investigation of slow loading.
The existing uncommitted shared home carousel is preserved in this update.

## Behavior

- Messages opens on **Channels**. **Left/Z** selects Channels and **Right/C** selects Direct Messages.
  **Tab** or **Shift+Tab** also switches between
  Channels and Direct Messages. Each list retains `+ New message` and wraps.
- Within each list, conversation activity is newest first. The star still marks
  a pinned chat; pins break equal-time ties rather than placing older messages
  above newer activity. Draft previews are preserved; unsent edits do not invent
  a sent-message timestamp.
- `/` searches saved message text, conversation names and sender names across
  both lists. English-letter matching ignores case. It returns individual
  messages, newest first, with `Channel:` or `DM:` destination labels.
  Unsent drafts are not sent messages and are not search results.
- Enter opens the matching bubble with the normal red selection border.
  Reading a DM search result does not ask for a sending channel. Starting a
  reply still requires a valid explicit channel. Esc returns to results;
  another Esc clears search and returns to the tab used to start it.
- Lists/results and chat bubbles build at most 50 rows at a time. The complete
  model/history is retained. Up/Down transparently loads another window at a
  boundary; older/newer text indicates more thread history, and long lists show
  a position count. Search can jump directly to an old message without building
  all intermediate bubbles.
- Selection uses copied thread/message identities, including sender and packet
  ID, rather than pointers into a vector. Refreshes preserve the visible target
  if it still exists; removed results leave a correctly highlighted remaining
  result. Channel history continues to use opaque channel identity, not slot.

## Loading investigation

The owner's report concerns both launch and opening Messages/conversations.
On the pre-change installed 0.1.21 build, one ordinary-user warm launch first
changed the framebuffer after **0.816 seconds**; the existing-supervisor check
alone took **0.520 seconds**. These observations do not reproduce every reported
slow start or qualify cold-start performance.

The previous screen constructs every bubble in a thread and every row in a
result/conversation list. Work therefore grows with the entire retained history,
including off-screen objects. The new windowing bounds that rendering work
without deleting or reducing searchable history. The Release configuration also previously enabled `LV_USE_ASSERT_OBJ`: LVGL
9.5.0 marks this object-type/existence diagnostic as slow and disables it in its
configuration template. It walks the object tree on checked API calls. This
update keeps it enabled in Debug builds (including host sanitizers) and disables
it in the device Release build. Fast NULL/allocation assertions and explicit
application checks stay enabled. No radio lifecycle, runtime verification,
hardware permissions, CPU governor or network safeguards were changed.

## Verification

Synthetic regression coverage exercises both themes, tab filtering, strict
recency including pins, combined search ordering, an older match, DM viewing
without a channel picker, empty results, Esc, page boundaries, live deletion,
2,000 retained messages and a bounded live object count. Existing UI, keyboard,
channel, persistence and backend tests remain required. A synthetic benchmark
uses real device fonts and compares the old/new 2,000-message screen on Lockon;
it does not contact the supervisor, use real chats or transmit RF.

Final package/device measurements and preservation results are recorded here
at qualification. Synthetic checks do not replace the owner's physical keyboard
and everyday-use acceptance. The unexplained historical phone disconnect and
GPS/USB/SIM/fresh-card/publication deferrals remain unchanged.

Intermediate real-font measurement on Lockon: the old synthetic 2,000-message
open timed out after 60 seconds; windowing alone opened it in 1,100.859 ms with
322 LVGL objects and all 2,000 messages retained. The pre-change 27-message
comparison opened in 276.763 ms with 179 objects. These are synthetic UI timings,
not claims about the owner's precise intermittent delay. Both-theme new navigation
regressions also passed on Lockon before the Release diagnostic adjustment.

Final Release real-font qualification on Lockon passes both themes, including
old-result jumps, page boundaries, live result deletion and retained history.
The 2,000-message open is approximately 91 ms in the integrated regression
(91.361 / 90.983 ms); the standalone comparison and package identity are recorded
in the installed closeout below. All six host Debug ASan/UBSan suites pass with
object diagnostics enabled; five arm64 Release suites and all 175 backend tests,
package/upgrade/firewall gates pass with the new configuration.

## Installed closeout

- Installed **0.1.22-rev1** from runtime/package source
  `15092a82990358eb3d4186739a045dd5a632ba1e`. Later closeout commits only
  update records. Package: 7,902,006 bytes; SHA-256
  `02f7771f91b78a6fd31f33927c88a5c941d7fe4026382c19c2332e470102cfa3`.
- All 265 installed regular files match package hashes, modes and root ownership;
  dpkg audit passes. All 27 messages, three saved thread records/drafts, three
  preferences and original saved app/radio/profile files are byte/logically
  preserved. Radio remains Off; launcher and ADB/hotplug are active.
- Standalone synthetic device timings: 27 messages **276.763 → 60.639 ms**;
  2,000 messages **did not finish within 60 seconds → 112.256 ms**, with 322
  live screen objects and all 2,000 messages still present. These single-run
  observations are not an exact universal speed or battery-life guarantee.
- Installed normal-user first changed framebuffer: **2.701 seconds** after
  the upgrade, including starting its supervisor; subsequent warm repeat
  **1.020 seconds**. Earlier warm baseline was 0.816 seconds. This does not
  establish a launch-time improvement or explain every intermittent slow start.
  Keep the owner's launch report open for concrete timing/reproduction if it
  recurs; the demonstrated fix is bounded UI/rendering cost.
- Both-theme actual-device-font navigation/search/page tests pass. All six host
  sanitizer suites, five arm64 Release suites, all 175 backend tests and the
  isolated package/upgrade/firewall gate pass. Source User guide CI succeeds;
  source host CI succeeds, with its arm64 job still running at closeout readback.
- User-guide Markdown/screenshots and generated 0.1.22 manual/reference are
  refreshed. Older published/Drive editions were not silently replaced. No new
  public release, Store submission, RF/phone/GPS acceptance or physical-key
  walkthrough is claimed. Private backups and reduced evidence are retained
  under the hardware workspace's `private-review-backups/mesh-messages-20261126/`.
