# Mesh documentation

Start with [current status](current-status.md) and the completed
[session handoff](../HANDOFF.md). Current instructions below apply to the
maintained app; dated reports describe only their named source and test scope.

| Task | Read |
|---|---|
| Install/update, verify downloads, back up, recover, roll back or uninstall | [Installation runbook](install.md) |
| Learn keyboard, messages, channels, profiles, map and settings | [User guide and offline manuals](user-guide/README.md) |
| Build a complete installer or understand offline caches/source assets | [Distribution](distribution.md) |
| Develop desktop UI or run regressions | [Build and test](build.md) |
| Understand UI/supervisor design and the private runtime | [Architecture](architecture.md), [backend README](../supervisor/README.md) |
| Integrate an interface safely | [Supervisor protocol](supervisor-protocol.md), [security](security.md) |
| Connect the official phone app | [Phone setup and limitations](phone-access.md) |
| Check installed/qualified/deferred work | [Current status](current-status.md), [session handoff](../HANDOFF.md) |
| Prepare Store submission/video | [Readiness report](store-submission-readiness-20261123.md), [video checklist](store-demo-checklist.md) |

## Qualification records

- [History/background reliability](history-reliability-20261127.md) — latest recorded installation and storage/SDK checks.
- [Previous reliability pass](reliability-followup-20261127.md) — traces, I2C and signal handling.
- [Radio startup](radio-startup-20261127.md) — GPIO conflict, bounded recovery, API readiness and Cap cycles.
- [Keyboard audit](keyboard-audit-20261127.md) — completed standalone system keys and input-replay scope; the earlier alignment deferral is superseded.
- [Messages/navigation](messages-navigation.md), [bundled distribution](distribution-validation-20261123.md), [phone acceptance](phone-access-validation.md), [fitted GPS](internal-gps-validation.md) and [header checks](header-presentation.md) — their named versions and limits.

## Design and history

[Original review/remediation](review-remediation.md), [maintenance](maintenance-pass.md),
[channel setup](channel-setup-flow.md), [channel usability](channel-setup-usability.md)
and [everyday refinements](usability-refinement.md) retain exact dated evidence.
The [original plan](implementation-plan.md), [Store corrections](store-readiness.md),
[review follow-up](report-followup.md) and [distribution plan](distribution-plan.md)
explain earlier decisions; their old pending states are not current task orders.
The [keyboard contract](keyboard.md) remains reference material.

[CHANGELOG](../CHANGELOG.md) is append-only session history. Superseded combined
status text lives in [history](history/status-before-organization-20261127.md).
Old standalone-daemon/manual-venv commands apply to their historical versions;
current installation uses the single bundled installer. Physical/fresh-image/
public-peer/long-term limits remain explicit. Source publication does not by
itself publish an installer or constitute Store acceptance.
