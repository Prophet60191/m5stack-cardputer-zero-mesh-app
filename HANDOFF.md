# Mesh session handoff — 2026-11-27

The requested radio/reliability review and fixes are complete. No unfinished code,
installation, hardware prompt, device test helper or background task from this
review needs resuming. Begin further work from the next user request.

## Current app

- Canonical project: `/Users/josed/Code/m5stack-cardputer-zero-mesh-app`.
- GitHub: https://github.com/Prophet60191/m5stack-cardputer-zero-mesh-app.
- Installed **0.1.26-rev1**, exact functional/package source **91308d6**.
- Installer SHA256: `ba53e9279f1c5e47d0c3f5a8c883db07597d039c5e7c90992ca33d3f1703691a`.
- Later documentation commits do not imply another installation is required.

## Completed and verified

Radio recovery/API readiness, truthful lifecycle-safe traces, bounded numeric
signal handling, history failure visibility and atomic changes, duplicate packet
handling, and SDK callback lifetime cleanup are implemented. Read
[the history review](docs/history-reliability-20261127.md),
[the previous reliability pass](docs/reliability-followup-20261127.md) and
[radio startup](docs/radio-startup-20261127.md) for exact evidence and limitations.

215 backend tests, seven host sanitizer suites and six arm64/native package suites
pass. All 268 installed regular files/modes/owners and original saved files/SQLite
rows verify. Three actual configuration snapshots, two-minute healthy watch and
actual launcher/Diagnostics/return navigation pass for 0.1.26. Storage and
abandoned-callback faults were simulated in disposable fixtures, not introduced
into the owner's live database. Earlier 0.1.24/0.1.25 hardware results stay separate.

- Runtime-source CI: run **46021801767**, both jobs successful.
- Qualification-documentation CI: run **46023158946**, both jobs successful.
- Runtime-source user-guide CI: run **46021801987**, successful.
- Old pending-CI notes are historical and superseded by these results.

## Preserve on resumption

Last verified device state: Mesh open, radio Online/send-receive, ADB enabled.
Preserve saved channels, profiles, chats/drafts, TX/privacy/GPS/phone choices and
other apps/services. Never infer current Cap/antenna attachment from this old
state; confirm before a new powered test if hardware may have changed.

Private originals, installers and raw evidence remain outside Git under
`/Users/josed/Code/cardputer_zero/private-review-backups/`, in the dated radio,
reliability-followup and history-reliability directories. Exact task device
stages were removed after byte-verifying host backups. Ignored local builds and
rollback evidence are intentional; do not delete them to make git status clean.

The guarded source/backend mirrors in the hardware workspace are synchronized
through this closeout, retaining its independent notes. That workspace is not a
second Git checkout. Other apps and the app-builder guide's separate WIP were
not modified; do not absorb their changes into Mesh commits.

USB GPS, SIM, fresh-image hardware, video/submission/public release and prior
explicit phone-drop/physical qualification limits remain deferred. No new peer
message, phone or outdoor GPS acceptance is claimed by this pass. Reusable
storage/SDK lessons are retained in the history review for a coordinated guide
update later. No automatic RF replay or shared-hardware takeover was introduced.
