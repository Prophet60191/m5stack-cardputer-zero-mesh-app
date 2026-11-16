#!/bin/sh
# SPDX-License-Identifier: MIT
# Run ONLY in the isolated tests/Dockerfile container, with --network none.
set -eu
if [ ! -f /.dockerenv ] || [ "${CZ_MESH_ISOLATED_TEST:-}" != 1 ]; then
    echo 'Refusing: use the documented isolated test container.' >&2
    exit 1
fi
mesh_build="${CZ_MESH_BUILD_DIR:-/work/linux}"
cmake -S /src -B "$mesh_build" -G Ninja -DUSE_DESKTOP=OFF -DCM0_SDK_ROOT=/ \
    -DCM0_ALLOW_FETCH_DEPS=OFF -DFETCHCONTENT_SOURCE_DIR_LVGL=/lvgl \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr \
    -DCPACK_OUTPUT_FILE_PREFIX="$mesh_build/dist"
cmake --build "$mesh_build" -j 8
cd "$mesh_build"
cpack -G DEB
package=dist/cardputerzero-mesh_0.1.1_rev1_arm64.deb
dpkg-deb --info "$package"
dpkg-deb --contents "$package" > package-contents.txt
# Some minimized images exclude docs by default; include them for validation.
dpkg --path-include='/usr/share/doc/*' -i "$package"
test -x /usr/bin/cardputerzero-mesh
test -f /usr/share/doc/cardputerzero-mesh/docs/security.md
test -f /etc/cardputerzero-mesh.conf
test ! -e /usr/bin/template_app
visudo -cf /etc/sudoers.d/cardputerzero-mesh
systemd-analyze verify /usr/lib/systemd/system/cardputerzero-mesh-firewall.service \
    /usr/lib/systemd/user/cardputerzero-mesh-supervisor.service
systemctl enable cardputerzero-mesh-firewall.service
systemctl is-enabled cardputerzero-mesh-firewall.service
setpriv --reuid=cz-mesh-firewall --regid=cz-mesh-firewall --clear-groups \
    --inh-caps=+net_admin --ambient-caps=+net_admin \
    /usr/lib/cardputerzero-mesh/firewall-setup.sh ensure
python3 /usr/lib/cardputerzero-mesh/firewall.py check
nft insert rule inet cz_mesh input accept
if python3 /usr/lib/cardputerzero-mesh/firewall.py check; then
    echo 'FAIL: permissive rule was accepted' >&2
    exit 1
fi
python3 /usr/lib/cardputerzero-mesh/firewall.py ensure
python -m unittest discover -s /src/tests -p 'test_*.py' -v
