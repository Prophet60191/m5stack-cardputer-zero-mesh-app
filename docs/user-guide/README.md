# User Guide & Quick Reference

For Mesh **0.1.24 (release candidate)** · Updated **2026-11-27**

Start with the introduction if you are new to the app, or keep the quick reference
handy when you are away from a computer.

| Guide | What you will find |
|---|---|
| [Quick Reference Card](00-Quick-Reference-Card.md) | Keys, status labels, hardware care and the group sync checklist |
| [Introduction & Getting Started](01-Introduction-and-Getting-Started.md) | First setup, keyboard basics and background listening |
| [Messages](02-Messages-Menu.md) | Chats, drafts, delivery results, location and channel QR sharing |
| [Nodes](03-Nodes-Menu.md) | Nearby radios, favorites, filters and node details |
| [Map](04-Map-Menu.md) | North-up radar, distances and position freshness |
| [Settings](05-Settings-Menu.md) | Radio, channels, profiles, privacy and diagnostics |
| [Troubleshooting & FAQ](06-Troubleshooting-and-FAQ.md) | Radio errors, failed sends, scanning and GPS questions |

Before starting, follow the [installation instructions](../../README.md#install--run).
Attach the antenna before powering the radio, and choose a permitted region.
Public and private channels are both supported. A private channel's QR or share
URL contains its secret key: share it only with the intended group.

## Installation and maintenance

0.1.19 bundles its private radio and Python wheels. Start with the
[complete installation runbook](../install.md) for verified downloads, first
launch, upgrade/backup, recovery, rollback and uninstall. The app's usage chapters
below explain day-to-day operation. Release `INSTALL.txt` is an offline copy of
the full runbook; the manuals use their own `GUIDE_SHA256SUMS` checksum set.

## Offline downloads

For current candidate PDFs, download the `mesh-user-guide` artifact from the
latest successful [User guide workflow](https://github.com/Prophet60191/m5stack-cardputer-zero-mesh-app/actions/workflows/user-guide.yml),
or follow the [local build instructions](../../scripts/user-guide/README.md).
These candidate artifacts are separate from published release downloads.

The last published PDFs cover 0.1.14; the Markdown here describes the 0.1.19 candidate. Download the reviewed 0.1.14 PDFs from the **[0.1.14 candidate release](https://github.com/Prophet60191/m5stack-cardputer-zero-mesh-app/releases/tag/v0.1.14-rc.1)**:

- **Cardputer-Zero-Mesh-User-Guide.pdf** — combined illustrated manual, with
  contents, internal links and chapter bookmarks.
- **Quick-Reference-Card.pdf** — one landscape US Letter page, suitable for
  printing or saving on your phone. Print at actual size, or fit to A4 landscape.

The app is still a release candidate. These guides describe its controls; they
do not certify the remaining public-peer RF, physical IPv6 or long-term reliability checks. The fitted GPS passed its documented outdoor check.
See the [verification record](../review-remediation.md).

## Screenshots and privacy

All 31 screenshots use synthetic names, node IDs, messages and positions. The QR
picture is demonstration data, not a channel to join. The screenshots were
rendered with 0.1.6, 0.1.13, 0.1.14, 0.1.15, 0.1.16 and 0.1.18; current controls are documented for 0.1.19; older illustrations are identified here. Older illustrations can show navigation reminders that 0.1.14 moves to HELP. Diagnostics on your
device shows your installed version. Colors can differ with your selected theme. Older illustrations may show a battery icon; 0.1.18 keeps only the numeric percentage.

Public messages, identity and telemetry can be visible to other people. Other
mesh nodes may bridge traffic to the Internet. Private/Public labels describe
channel keys, not a guarantee about the firmware's separate DM encryption. Saved
chats and mesh profiles are private local files, **not encrypted storage**.

## Keeping the guides current

Markdown and `images/` are the maintained source. PDFs are generated from these
same chapters and published as versioned release assets; generated PDFs are not
committed to Git or included in the device installer. The installer does include
the Markdown guides and screenshots for offline reference.

The [PDF build instructions](../../scripts/user-guide/README.md) describe local
generation, link/version/layout validation and the documentation workflow. The
workflow attaches the PDFs to published releases, including prereleases, only
when the release tag matches the reviewed app version. Generated PDFs include
their font license notices as attachments. Documentation and screenshots follow
the repository's [MIT license](../../LICENSE); external trademarks retain their
respective ownership. Meshtastic is a registered trademark of Meshtastic LLC;
this is an independent community project.
