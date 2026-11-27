# Mesh documentation

Current implementation: **0.1.19 bundled runtime**. Start with the current
instructions below; dated qualification and development records describe the
version they actually tested, not commands to rerun on a newer installation.

| Task | Read |
|---|---|
| Install/update, verify a download, back up, recover, roll back or uninstall | [Installation runbook](install.md) |
| Learn the keyboard, messages, channels, profiles, map and settings | [User guide and offline manuals](user-guide/README.md) |
| Build a complete installer, use caches/offline mode, understand dependencies/source assets | [Distribution](distribution.md) |
| Develop the desktop UI or run regressions/CI checks | [Build and test](build.md) |
| Understand UI/supervisor design and the fixed runtime | [Architecture](architecture.md), [backend README](../supervisor/README.md) |
| Integrate an interface safely | [Supervisor protocol](supervisor-protocol.md), [security boundaries](security.md) |
| Connect the official phone app | [Phone setup and limitations](phone-access.md) |
| Check what is installed/qualified or deferred | [Current status](current-status.md), [0.1.19 qualification](distribution-validation-20261123.md) |
| Prepare Store submission/video | [Readiness report](store-submission-readiness-20261123.md), [video checklist](store-demo-checklist.md) |

## Evidence and history

[Original review/remediation](review-remediation.md),
[maintenance](maintenance-pass.md), [channel setup](channel-setup-flow.md),
[channel usability](channel-setup-usability.md), [everyday refinements](usability-refinement.md),
[phone acceptance](phone-access-validation.md), [fitted GPS qualification](internal-gps-validation.md)
and [header checks](header-presentation.md) retain exact versions, hashes and
limits. Old standalone daemon/manual-venv commands apply only to their named
historical version. Current deployment uses the single bundled installer.

[The original implementation plan](implementation-plan.md),
[Store packaging corrections](store-readiness.md), [review follow-up](report-followup.md)
and [distribution plan](distribution-plan.md) are retained to explain decisions;
current instructions and qualification above supersede pending-state language.
The [keyboard contract](keyboard.md) remains current; the separate
[alignment follow-up](keyboard-alignment-follow-up.md) is still deferred.

No spare microSD: fresh-image hardware acceptance is deferred, distinct from
passed minimal Debian container checks. USB GPS/SIM work and documented
public-peer/IPv6/full-keyboard/long-term phone limits are not silently certified.
No public release or Store submission follows automatically from a source push.

- [Radio startup recovery and final qualification](radio-startup-20261127.md) — current 0.1.24 hardware/API recovery and exact installer.
