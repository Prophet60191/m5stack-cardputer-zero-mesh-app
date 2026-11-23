# Building the offline guides

The maintained source is [docs/user-guide/](../../docs/user-guide/README.md).
Review `guide.json` and every chapter's version/date stamp whenever controls
change. Screenshot examples are synthetic and must never contain real channel
keys, share URLs, device identities, chats, locations or personal paths.

## Local build

Use Python 3.12 or later. On Debian/Ubuntu, install `python3-venv` and
`fonts-dejavu-core`; on other systems, provide the directory containing the four
DejaVu Sans regular/bold/oblique/bold-oblique `.ttf` files with `--font-dir`.
Do not copy font files into this repository. Their redistribution notices are
included in [DejaVu-LICENSE.txt](DejaVu-LICENSE.txt) and attached to each PDF.

```sh
python3 -m venv build/docs-venv
build/docs-venv/bin/pip install -r scripts/user-guide/requirements.txt
build/docs-venv/bin/python scripts/user-guide/build.py --check-only
build/docs-venv/bin/python scripts/user-guide/build.py
```

Outputs are in ignored `build/user-guide/`: the combined manual, one-page
landscape US Letter quick reference, `SHA256SUMS` and `guide-manifest.json`.
The builder validates local links/anchors, version/date stamps, screenshot
dimensions/metadata, PDF text and font notices. It refuses to produce a
multi-page quick-reference card instead of shrinking the text automatically.
PDF section links and bookmarks support offline navigation. Relative links to
developer documentation become full repository links; those require Internet.

Before publishing, render **every page** with Poppler (`pdftoppm -png -r 110`) and
inspect the images for clipping, missing glyphs, table splits and readable
screenshots. Print/check the quick reference at actual size. Automated link and
page checks do not substitute for this visual review. PDF metadata must not
include workstation paths or private author details.

## CI and releases

The [User guide workflow](../../.github/workflows/user-guide.yml) builds and checks
the PDFs on relevant pushes, pull requests and manual runs. Download them from
its `mesh-user-guide` artifact while preparing a release. On publication of a
release or prerelease, the same workflow checks out that tag, verifies that its
version matches the reviewed guide, then attaches PDFs/checksums/manifest to
that release. Only that final job has repository-write permission; pull-request
builds cannot publish assets. It neither creates tags nor publishes releases.

Generated PDFs belong in release assets, not Git or `docs/user-guide/`. Packaging
includes the Markdown and screenshots but explicitly excludes PDFs. Device
runtime code and configuration are unaffected by a documentation-only update.

Release attachment names are `Cardputer-Zero-Mesh-User-Guide.pdf`,
`Quick-Reference-Card.pdf`, `guide-manifest.json` and **`GUIDE_SHA256SUMS`**. The
local builder still writes `SHA256SUMS`; the release job renames that manual-only
checksum file before upload. Never clobber a release's installer/source
`SHA256SUMS` with PDF hashes. Verify both sets independently. A draft release is
not published and does not trigger the publication attachment job.

When installation behavior changes, update the introductory/troubleshooting
chapters and [complete operations runbook](../../docs/install.md) as well as the
front README. Rebuild/review the manual/reference and refresh maintained current
copies; keep old versioned editions historical. Documentation updates do not
replace an already qualified installer or silently requalify new binaries.
