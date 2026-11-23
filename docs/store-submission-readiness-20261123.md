# Store submission readiness — 2026-11-23

**Current disposition: prepare for submission; no Store upload is authorized.**

## Distribution follow-up — qualified 0.1.19

The owner authorized the preparation work and will record the actual-device
video. [Exact qualification](distribution-validation-20261123.md) covers source
`8ada52a`, both CI jobs, the paired installer/source assets, all 255 installed
files, private data preservation and bounded real radio/GPS/RF/SDK checks.

- CZMS replaces the taken MESH code; app/package names stay unchanged. Current
  pinned preflight passes, with an optional localization recommendation. The
  later catalog dad9338 has unchanged policy bytes and no CZMS mention.
- One package includes the GPS-corrected private native daemon and 15 locked
  wheels. Minimal Debian arm64 offline first-run/cache/QR/remove checks pass.
  Standard OS libraries use APT; optional camera capture/Tailscale remain image
  and account facilities. No separate radio install or manual pip is needed.
- Complete corresponding source, licenses, build provenance, checksums and
  install/video instructions accompany the candidate. Reviewable draft/local
  assets are preparation; durable public availability begins only at publication.
- The existing device upgrade passes with original data and settings preserved.
  The owner has **no spare microSD** and explicitly defers fresh-image hardware
  qualification. Do not erase the working card or claim container proof is the
  same as fresh hardware.
- Owner video, final video/privacy review and explicit publication/submission
  remain. No Store PR or upstream submission has been made.

See [distribution](distribution.md) and [the video checklist](store-demo-checklist.md).
The historical list below records the original blockers; this follow-up
supersedes their now-resolved code/dependency/CI status.

## Original review (historical 0.1.18 baseline)

The following describes the original review before the above changes.

**Original disposition: prepare for submission, but do not publish this candidate yet.**
One automated blocker is confirmed: the Store share code MESH is now owned by
MeshTandem. Separate installation/dependency and first-submission evidence work
also remains. This review changes no app behavior, metadata identity, installed
package, radio settings or device. No authentication, fork, upload, PR, release or
submission was performed.

## What was checked

Review the official [development rules](https://github.com/CardputerZero/cardputerzero.github.io/blob/5d5123ca05bfc23e5a920f4e9fdbfaf12685e3ab/docs/en/CP0_dev.md)
and [submission instructions](https://github.com/CardputerZero/cardputerzero.github.io/blob/5d5123ca05bfc23e5a920f4e9fdbfaf12685e3ab/docs/en/CP0_submit.md),
[AppBuilder schema/CLI](https://github.com/CardputerZero/AppBuilder/tree/042356c579d728d684eccb68ed9f8669a32822bc)
and [packages catalog/policies](https://github.com/CardputerZero/packages/tree/b6c559a0448aae7715c4b45678a0b89c9eceb12b).
The general guide records the reviewed source hashes. Both executable policy
script hashes are unchanged from the previous review; the catalog and publishing
implementation have advanced.

Run the non-publishing guide preflight in a network-disabled Debian container
against the exact installed candidate, current catalog and intended uploader.
No maintainer script was executed and no device was accessed for this review.

- App: Cardputer Zero Mesh, package `cardputerzero-mesh`, version `0.1.18-rev1`.
- Functional/package source: `2913c01b754e895b306abf514a98b3daefcc16f0`.
- Candidate SHA-256:
  `b17b59743d3b48e997b60dbabb4fd0d47bfac980740fc453c9e6d6010dac55d4`.
- Current catalog: `b6c559a0448aae7715c4b45678a0b89c9eceb12b`.
- Result: **blocked**, one error: MESH already belongs to `meshtandem`.
- Advisory: localized title/summary would be useful; missing locales did not block.

The preflight's generic `binary-update` mode label does not establish that this
app has already been published. It is a local package-check mode, not Store
approval or proof of account ownership. Its existing CI pass used catalog
`94eb8d7` from the older review and therefore did not catch this newer collision.

## What is already in good shape

The current check reports no other listing/package-policy error. Package name,
title, version, categories, seven permission booleans, Store icon/screenshots,
Maintainer and installed paths pass these implemented checks. The title/package
were not found occupied; code availability is not reserved by a check.

The UI and supervisor are unprivileged. The fixed firewall monitor runs under
its own account with bounded network-admin capability, not as a root app. Its
actual unit/path passes this policy snapshot. That is evidence for the existing
modular design, not a general exemption from maintainer/security review. The
supervisor is useful for one radio owner, shared UI/phone access, privacy checks
and bounded asynchronous work; a different app need not copy it.

The [current app record](current-status.md), [header closeout](header-presentation.md),
[GPS record](internal-gps-validation.md) and [phone qualification](phone-access-validation.md)
cover 167 backend tests, native checks, arm64 install/upgrade/policy gates,
exact installed files and preserved data. Private RF, actual handset and fitted
GPS evidence exist at their documented versions and limits. They do not prove a
clean, dependency-free Store install or erase an unexplained earlier phone drop.

## Work to complete before submission

| Priority | Concrete action | Why / acceptance |
|---|---|---|
| Required automated fix | Choose another four-character Store code and update/rebuild metadata consistently. CZMS and CMES are unclaimed in this snapshot; neither is reserved. | Recheck the current catalog and exact rebuilt package. This is a Store lookup code, not a LoRa channel or profile; keep the app name/package stable unless separately agreed. |
| Release-readiness recommendation | Complete a supported fresh-image dependency/setup path. | Install on a clean supported Debian arm64 image with standard permissions; launch from Store/launcher, diagnose missing prerequisites, set up safely and demonstrate messaging. Do not infer this from the configured development device. |
| Dependency distribution | Provide durable, versioned access to the qualified radio service and GPS correction, or a tested supported replacement. | The app package does not install meshtasticd, the private Python environment, optional camera tools or Tailscale. The old baseline could not be downloaded from the configured repository during GPS work; a private reconstructed rollback is not a public installer plan. |
| Source/licensing release check | Retain the full corresponding source, notices and build provenance with any redistributed patched native binary. | CI already exports binary/source together, but expiring Actions artifacts alone are not a durable user download plan. Review the final release contents and update responsibilities. |
| Official first-submission requirement | Record a short, sanitized video on the actual CardputerZero. | Show launch, main features and exit; add it to the submission PR comments. The official guide says first submissions without the device demo are not approved. |
| Current gate/evidence | Refresh the app CI's pinned guide/catalog review before choosing a final artifact. | Its current checks use the older guide/policy/catalog pin. Run exact final bytes against current rules and keep fresh-install, security and hardware evidence distinct. |
| Final candidate review | Review final source/history, package, screenshots and video for secrets; freeze source/tag, version and checksum. | No private channel QR/keys, messages, coordinates, account links or identifying system details. Verify public downloads after upload. |

The permission declarations already disclose camera, network, additional hardware
and background service. Keep the description equally clear: supported Cap,
external prerequisites, optional phone access, privacy implications and known
limits. A public GPS demo should use synthetic/safe content and must not enable
location broadcasts merely to show a moving dot.

Do not solve external dependencies by blindly adding an automatic dependency
install: the native radio package can start its standalone daemon. Preserve the
supported mask/firewall/one-owner order before any radio power or transmission,
and test it on the clean image. This needs a deliberate packaging plan, not a
new blanket sudo grant or a competing service.

Manual dependency installation is not automatically a policy rejection in the
reviewed rules. The clean-image/install plan above is our release-quality
recommendation, and reviewer questions remain possible. The official preinstalled
GPS app's root/sudo power-control packaging does not override the no-root rule
for third-party submissions; do not copy it as a shortcut.

## The actual submission process

1. Finish the above, select the exact artifact/listing and prepare its demo.
2. Choose the official Developer Center at `https://dev.cardputer.cc/`, or the
   reviewed `czdev` CLI. Both paths create a PR in `CardputerZero/packages` after
   authentication/upload. This review does neither.
3. For CLI publishing, run `czdev publish --deb <exact-file>` from the app root
   containing `app-builder.json`. It uploads the binary to the contributor's
   packages fork and proposes metadata/assets/manifest; the `.deb` is not put
   in Git. There is no publish dry-run in this reviewed CLI.
4. Add the physical demo to the first-submission PR. Validation is followed by
   maintainer review; first release is not auto-approved merely because CI passes.
5. After approval, check promotion/index completion, Store listing, actual download
   hash and installation. Do not equate a merged PR with successful deployment.
6. Later updates need a higher embedded Debian version and owning account.
   Current Store lifecycle supersedes older versions, so preserve independent
   verified rollback assets. Keep acceptance limits and dependency upkeep visible.

## Recommendation

Keep using the qualified app while preparing its distribution. The next focused
work should be a new share code, dependency/install packaging decision and a
clean-image rehearsal, followed by the demo and final privacy/release check.
USB GPS and SIM-dependent features can stay deferred; they are not prerequisites
for publishing the app's already-supported scope. Do not advertise them as supported.
