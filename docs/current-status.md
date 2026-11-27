# Current status

Updated 2026-11-27 from the completed [session handoff](../HANDOFF.md) and dated
qualification records. This is maintained project status, not a new live-device
inspection. [Documentation index](README.md) provides the operating/build routes.

## Recorded installed baseline

**Mesh 0.1.26-rev1**, functional/package source **91308d6**, is the latest recorded
installed baseline. Later documentation commits do not require another install.
The requested radio/reliability review and installation are complete; begin
further work from the next user request.

[History/background reliability](history-reliability-20261127.md) owns exact
artifact hashes, storage/transaction/duplicate/SDK fixes and installed evidence.
The completed handoff records successful runtime-source and qualification-document
CI jobs, user-guide CI and preserved original app data. Its last verified device
state was Mesh open, radio Online/send-receive and ADB enabled. Confirm hardware
attachment anew before a future powered test.

## Feature and evidence routes

| Area | Maintained explanation or dated qualification |
|---|---|
| Radio recovery and local API readiness | [0.1.24 startup record](radio-startup-20261127.md) |
| Traceroute, I2C and signal handling | [0.1.25 reliability record](reliability-followup-20261127.md) |
| Live events, durable history and SDK callbacks | [0.1.26 history/background record](history-reliability-20261127.md) |
| Channels/DM tabs, combined search and bounded history rendering | [Messages navigation](messages-navigation.md) |
| Standalone system keys and input | [Keyboard audit](keyboard-audit-20261127.md) |
| Bundled offline runtime and preserved upgrades | [Distribution](distribution.md), [0.1.19 qualification](distribution-validation-20261123.md) |
| Fitted GPS and official phone access | [GPS evidence](internal-gps-validation.md), [phone evidence](phone-access-validation.md) |

Each report qualifies its named artifact and test scope. Earlier radio/GPS/phone
results are not repeated qualification of every later UI or reliability update.

## Remaining boundaries

Fresh-image hardware acceptance remains explicitly deferred without a spare card;
never erase the working setup. USB GPS and SIM work remain deferred. Public-peer
interoperability, physical IPv6, the full physical keyboard sweep, long-term phone
reliability and earlier nonfatal GPS warnings retain their documented limits.
Source tests and replayed input do not establish physical/field acceptance.

[Store readiness](store-submission-readiness-20261123.md) owns video, release and
submission preparation. CZMS replaces the taken MESH code in the prepared
metadata. Source pushes and a draft release do not publish a new installer or
constitute Store acceptance. No publication/submission is implied by this cleanup.

## History

[CHANGELOG](../CHANGELOG.md) and versioned reports retain session evidence.
The prior concatenated status page is preserved in
[history](history/status-before-organization-20261127.md). Its old Radio Off and
pending-CI wording applies to its dated records, not this maintained baseline.
