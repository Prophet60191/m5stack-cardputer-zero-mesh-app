# Why Mesh uses a modular UI and supervisor

Decision: retain a native C++/LVGL presentation process and a separate Python
resource owner, communicating through a private local protocol. This is a
deliberate Mesh design choice. The official CardputerZero Template and shared
SDK are useful foundations with different integration paths; a separate Python
supervisor is not a requirement for every CardputerZero application.

## Purpose and goals

Mesh needs a small keyboard-first interface, the existing Meshtastic Python
client, background reception after the screen closes, and explicit ownership of
a radio daemon and shared Cap resources. Separate responsibilities let each part
change and be tested without requiring the other to perform its real work.

The goals are responsive interaction, consistent authoritative state, controlled
resource lifetimes and maintainable interfaces. Reuse is a possible benefit of
those interfaces, not a claim that this repository is already a general SDK.

## Responsibilities and interfaces

| Module | Owns | Boundary |
|---|---|---|
| Native UI, viewmodels and widgets | Navigation, text editing, presentation and a read model of confirmed/pending/unknown radio state | Sends named intents; renders events on the LVGL thread |
| Input adapter and decoder | Per-device key translation, routing, repeat/release handling and the currently owned hold action | Raw events become characters or semantic controls; no radio API |
| Supervisor and application services | Radio/configuration authority, validated operations, delivery/history state and client synchronization | Bounded newline JSON over an owner-only Unix socket |
| Guardian and user-service lifecycle | Child daemon containment, ownership leases, safe stop/reap/restore and background lifetime | Fixed process/service operations; no client-supplied executable or command |
| Firewall monitor | Narrow networking enforcement and a fresh health verdict | Separate capability-limited service; UI and supervisor remain normal-user processes |
| External Meshtastic stack | Firmware-facing API and radio transport | Pinned, separately installed daemon/client dependencies with compatibility checks |

The UI has cached presentation state, not authority to operate the radio directly.
The [protocol](supervisor-protocol.md) defines requests, broadcasts, correlation,
validation and reconnect semantics. [Security](security.md) describes permissions,
the same-user trust boundary and enforcement limits.

## Why the additional process is useful here

**Independent UI work.** Screens and keyboard handling can be exercised with
synthetic state and a fake or absent radio owner. Native rendering keeps the
presentation path close to the official LVGL Template. Python uses the existing
Meshtastic client rather than introducing another implementation of that client.

**Background reception.** Closing the UI does not close the radio owner. The
ordinary-user service has its own lifecycle/control group. Reopening the UI
connects to the existing owner; explicit radio/service stop ends reception.

**Clear authority.** Channel identity, delivery results and confirmed privacy
settings have one owner. Request/target IDs, readback, timeouts and snapshots are
still necessary; process separation alone does not prevent stale or wrong results.

**Controlled permissions and recovery.** The UI does not inherit a general root
shell. A fixed capability service handles firewall work, and the guardian/service
contain descendants and recover saved hardware state on supported failure paths.
An ordinary process exit or Python `finally` is insufficient for abrupt death.

**Smaller changes and tests.** Keyboard fixes can be tested without RF. UI changes
need not rewrite Cap control. The actual input-reader tests use synthetic pipes;
backend tests use fake radio/sysfs controls and real pinned protobuf/event APIs.
Those seams make failures easier to reproduce and allow selected MIT components
to be considered for other apps with their fixtures and provenance intact.

## Relationship to official components

The [official CMake Template](https://github.com/CardputerZero/Template/tree/477fa6d217b683e3084c361a757f03dc8ca9f753)
provides the original UI structure. The [shared cp0 runtime](https://github.com/CardputerZero/launcher/blob/4c05b444cb977cb90e2f33eb4c8261226e370713/docs/current-implementation-and-components.md)
offers a broader SCons/Kconfig service/input path. Mesh uses the Template path
and its own resource-owner protocol; it does not require replacing the device's
launcher or loading a second official keyboard decoder.

Compatibility means honoring the actual launcher, input, permission and package
contracts. Identical internal structure is not necessary. We retain the correct
official symbol assignments and the reviewed Store/package rules. These checks
are separate from Store approval and from proving every device/image combination.

The distinction from native/in-process official services is therefore purposeful:
it meets Mesh's background lifetime, Python-client and hardware ownership needs.
A calculator or simple local utility can use a single process. The general
[app-builder guide](https://github.com/Prophet60191/m5stack-cardputer-zero-app-guide)
explains architecture choices and optional project sections.

## Costs and limits

The extra process uses memory and requires protocol/schema maintenance, dependency
management, request deadlines, reconnect handling and coordinated upgrades.
Separation does not make unbounded work safe: UI input is budgeted independently
of the supervisor's queue limits, and slow operations must not block local saves.

The Unix socket is a same-user control API, not protection against other programs
running as that user. Local daemon clients, stored keys/history and root/device
access remain sensitive. Software cannot verify an antenna or guarantee hardware
restoration through physical power loss or a kernel failure. See the explicit
limits and actual-device qualification in [the remediation record](review-remediation.md).

MIT UI/input files and GPL supervisor/helper files retain their own terms and full
notices. A socket or subprocess boundary is not an automatic licensing exemption;
the nature of an integration and the applicable component terms still matter.
No claim of automatic GPL isolation, universal reuse or measured battery savings
is made by this design decision.
