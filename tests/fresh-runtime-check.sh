#!/bin/sh
# SPDX-License-Identifier: MIT
set -eu
[ -f /.dockerenv ] && [ "${CZ_MESH_ISOLATED_TEST:-}" = 1 ]
python3 /src/tests/runtime-package-check.py
# Removal must not erase operator-owned data/cache or a legacy environment.
test ! -e /run/cardputerzero-mesh-package-update
dpkg --remove cardputerzero-mesh
test ! -e /usr/bin/cardputerzero-mesh
test ! -e /usr/lib/cardputerzero-mesh/runtime/manifest.json
test -f /home/mesh-runtime-test/.local/share/cardputerzero-mesh/venv/operator-note
test -d /home/mesh-runtime-test/.local/share/cardputerzero-mesh/managed-runtimes
test ! -e /run/cardputerzero-mesh-package-update
test -z "$(dpkg --audit)"
