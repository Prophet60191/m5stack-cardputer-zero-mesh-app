#!/bin/sh
# SPDX-License-Identifier: MIT
# Run ONLY in the isolated tests/Dockerfile container, with --network none.
set -eu
if [ ! -f /.dockerenv ] || [ "${CZ_MESH_ISOLATED_TEST:-}" != 1 ]; then
    echo 'Refusing: use the documented isolated test container.' >&2
    exit 1
fi
mesh_build="${CZ_MESH_BUILD_DIR:-/work/linux}"
test -f /runtime/manifest.json
cmake -S /src -B "$mesh_build" -G Ninja -DUSE_DESKTOP=OFF -DCM0_SDK_ROOT=/ \
    -DCM0_ALLOW_FETCH_DEPS=OFF -DFETCHCONTENT_SOURCE_DIR_LVGL=/lvgl \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DMESH_BUILD_BENCHMARKS=ON -DMESH_RUNTIME_BUNDLE=/runtime \
    -DCPACK_OUTPUT_FILE_PREFIX="$mesh_build/dist"
cmake --build "$mesh_build" -j 8
cd "$mesh_build"
ASAN_OPTIONS=detect_leaks=0 ctest --output-on-failure
cpack -G DEB
mesh_version=$(python3 -c 'import json; print(json.load(open("/src/app-builder.json"))["version"])')
package="dist/cardputerzero-mesh_${mesh_version}-rev1_arm64.deb"
dpkg-deb --info "$package"
test "$(dpkg-deb -f "$package" Pre-Depends)" = "python3 (>= 3.13), python3 (<< 3.14)"
dpkg-deb --contents "$package" > package-contents.txt
# Some minimized images exclude docs by default; include them for validation.
dpkg --path-include='/usr/share/doc/*' -i "$package"
test -x /usr/bin/cardputerzero-mesh
test -x /usr/bin/cardputerzero-mesh-launch
test -f /usr/lib/cardputerzero-mesh/user_service.py
test -f /usr/lib/cardputerzero-mesh/phone_gateway.py
test -f /usr/lib/cardputerzero-mesh/phone_services.py
test -f /usr/lib/cardputerzero-mesh/local_node_refresh.py
test -x /usr/lib/cardputerzero-mesh/runtime/radio/meshtasticd
test -f /usr/lib/cardputerzero-mesh/runtime/requirements.lock
test -f /usr/lib/cardputerzero-mesh/runtime_prepare.py
test ! -e /usr/bin/mesh-launch
test ! -e /usr/lib/systemd/user/cardputerzero-mesh-supervisor.service
test ! -e /usr/bin/mesh_benchmarks
test ! -e /usr/bin/mesh_channel_setup_regressions
test ! -e /usr/bin/mesh_maintenance_regressions
test ! -e /usr/bin/mesh_ui_checkup
test ! -e /usr/bin/mesh_keyboard_regressions
test ! -e /usr/bin/mesh_keyboard_evdev_regressions
./mesh_benchmarks > synthetic-benchmarks.txt
test -f /usr/share/doc/cardputerzero-mesh/docs/security.md
test -f /usr/share/doc/cardputerzero-mesh/docs/user-guide/README.md
test -f /usr/share/doc/cardputerzero-mesh/docs/user-guide/images/messages-list.png
test -z "$(find /usr/share/doc/cardputerzero-mesh/docs -type f -iname '*.pdf' -print -quit)"
test -f /etc/cardputerzero-mesh.conf
test ! -e /usr/bin/template_app
test ! -e /etc/sudoers.d/cardputerzero-mesh
systemd-analyze verify /usr/lib/systemd/system/cardputerzero-mesh-firewall.service
# Exercise the real upgrade archive, including removal of the old wrapper/unit.
# The trusted baseline is downloaded/checksummed before this offline container.
test -n "${CZ_MESH_UPGRADE_BASELINE:-}"
test "$(sha256sum "$CZ_MESH_UPGRADE_BASELINE" | cut -d ' ' -f 1)" = 8ff56073d45b43ed1cd1212f46bbdfcc9e345219c0897fbdf97d5fac2ad7b3b4
mesh_saved_config=$(sha256sum /etc/cardputerzero-mesh.conf | cut -d ' ' -f 1)
dpkg --path-include='/usr/share/doc/*' -i "$CZ_MESH_UPGRADE_BASELINE"
test -e /usr/bin/mesh-launch
test -e /usr/lib/systemd/user/cardputerzero-mesh-supervisor.service
dpkg --path-include='/usr/share/doc/*' -i "$package"
test ! -e /usr/bin/mesh-launch
test ! -e /usr/lib/systemd/user/cardputerzero-mesh-supervisor.service
test -x /usr/bin/cardputerzero-mesh-launch
test "$mesh_saved_config" = "$(sha256sum /etc/cardputerzero-mesh.conf | cut -d ' ' -f 1)"
test -z "$(dpkg --audit)"
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
install -d -o cz-mesh-firewall -g cz-mesh-firewall -m 0755 /run/cardputerzero-mesh-firewall
setpriv --reuid=cz-mesh-firewall --regid=cz-mesh-firewall --clear-groups \
    --inh-caps=+net_admin --ambient-caps=+net_admin \
    /usr/lib/cardputerzero-mesh/firewall-setup.sh monitor &
mesh_monitor=$!
trap 'kill "$mesh_monitor" 2>/dev/null || true; wait "$mesh_monitor" || true' EXIT
python3 - <<'PY'
import sys,time
sys.path.insert(0, '/usr/lib/cardputerzero-mesh')
import firewall
for _ in range(30):
    try:
        assert firewall.health_status()['ok']
        break
    except RuntimeError: time.sleep(.1)
else: raise SystemExit('monitor did not publish a trusted healthy verdict')
PY
nft insert rule inet cz_mesh input accept
sleep 3
python3 - <<'PY'
import sys
sys.path.insert(0, '/usr/lib/cardputerzero-mesh')
import firewall
try: firewall.health_status()
except RuntimeError: pass
else: raise SystemExit('monitor accepted a permissive firewall')
PY
kill "$mesh_monitor"
wait "$mesh_monitor"
trap - EXIT
python3 /usr/lib/cardputerzero-mesh/firewall.py ensure
python3 /src/tests/runtime-package-check.py
python -m unittest discover -s /src/tests -p 'test_*.py' -v
