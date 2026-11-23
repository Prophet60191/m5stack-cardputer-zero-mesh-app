#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
expected=54e0d8d0ab2ff56b3a9ce967e53f79e49af560fb
[ "$(git rev-parse HEAD)" = "$expected" ]
git diff --exit-code
[ -z "$(git status --porcelain --untracked-files=no)" ]
git apply --check /support/atgm-probe-timeout.patch
git apply /support/atgm-probe-timeout.patch
python3 /support/test_probe.py /work
pio run -e native-tft -j 4
mkdir -p /out
cp .pio/build/native-tft/meshtasticd /out/meshtasticd
strip --strip-unneeded /out/meshtasticd
/out/meshtasticd --version > /out/NATIVE_VERSION
python3 /support/export_source.py /work /out /root/.platformio
