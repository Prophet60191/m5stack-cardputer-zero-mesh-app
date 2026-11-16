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
