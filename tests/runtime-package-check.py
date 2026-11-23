#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Exercise the installed offline bundle as a new user in a hardware-free container."""
import hashlib,json,os,pwd,subprocess,sys,tempfile
from pathlib import Path
assert Path('/.dockerenv').is_file() and os.environ.get('CZ_MESH_ISOLATED_TEST')=='1'
assert not any(Path('/dev').glob('gpiochip*')) and not any(Path('/dev').glob('spidev*'))
USER='mesh-runtime-test'
try:user=pwd.getpwnam(USER)
except KeyError:
    subprocess.run(['useradd','-m','-U','-s','/bin/sh',USER],check=True);user=pwd.getpwnam(USER)

def run_user(args,check=True):
    cmd=['runuser','-u',USER,'--','env','PIP_INDEX_URL=https://invalid.example','PIP_FIND_LINKS=https://invalid.example',
         'PIP_REQUIREMENTS=/does/not/exist','PYTHONPATH=/does/not/exist',*map(str,args)]
    return subprocess.run(cmd,capture_output=True,text=True,timeout=180,check=check)

root=Path('/usr/lib/cardputerzero-mesh');script=root/'runtime_prepare.py'
legacy=Path(user.pw_dir)/'.local/share/cardputerzero-mesh/venv/operator-note'
run_user(['/usr/bin/python3','-I','-c',f'from pathlib import Path;p=Path({str(legacy)!r});p.parent.mkdir(parents=True);p.write_text("retain operator environment")'])
first=run_user(['/usr/bin/python3','-I',script]);python=Path(first.stdout.strip())
assert python.is_file() and 'managed-runtimes' in python.parts and 'Preparing the bundled' in first.stderr
second=run_user(['/usr/bin/python3','-I',script]);assert second.stdout==first.stdout and 'Preparing the bundled' not in second.stderr
m=json.loads((root/'runtime/manifest.json').read_text())
assert all(any((root/'runtime/licenses/python'/name).rglob('*')) for name in m['python_packages']), 'missing Python license texts'
code='''import importlib.metadata as md,json,sys
from pathlib import Path
expected=json.loads(sys.argv[1]);prefix=Path(sys.prefix)
for n,v in expected.items():
 assert md.version(n)==v
 assert Path(md.distribution(n)._path).is_relative_to(prefix),n
sys.path.insert(0,'/usr/lib/cardputerzero-mesh')
import bundled_runtime
assert bundled_runtime.daemon()[0]=='/usr/lib/cardputerzero-mesh/runtime/radio/meshtasticd'
from PIL import Image
from pyzbar.pyzbar import decode
assert decode(Image.new('L',(64,64),255))==[]
'''
run_user([python,'-I','-c',code,json.dumps(m['python_packages'])])
ready=python.parent.parent/'ready.json';old_ready=ready.read_bytes()
run_user(['/usr/bin/python3','-I','-c',f'from pathlib import Path;Path({str(ready)!r}).write_text("{{}}")'])
repair=run_user(['/usr/bin/python3','-I',script]);assert repair.stdout==first.stdout and 'Preparing the bundled' in repair.stderr
assert ready.read_bytes()==old_ready and legacy.read_text()=='retain operator environment'
# Wrong/corrupt artifacts are rejected before the healthy managed cache changes.
import shutil
with tempfile.TemporaryDirectory() as temp:
    stage=Path(temp);stage.chmod(0o755);bad=stage/'runtime';shutil.copytree(root/'runtime',bad)
    wheel=bad/'wheels'/m['wheels'][0]['filename'];wheel.write_bytes(b'corrupt wheel')
    code=f"import sys;sys.path.insert(0,'/usr/lib/cardputerzero-mesh');from runtime_prepare import prepare;prepare({str(bad)!r})"
    failure=run_user(['/usr/bin/python3','-I','-c',code],check=False)
    assert failure.returncode and 'checksum' in failure.stderr and ready.read_bytes()==old_ready
# The real bundled daemon/official SDK protocol is tested with simulated radio only.
probe=Path('/src/scripts/meshtasticd-gps/test_daemon.py')
if probe.is_file():run_user([python,probe,root/'runtime/radio/meshtasticd'])
assert not Path('/usr/bin/meshtasticd').exists(),'fresh checks must not rely on a global radio install'
print(json.dumps({'offline_runtime':'pass','packages':len(m['python_packages']),'cache_reuse':'pass','corrupt_cache_repair':'pass',
                  'bad_wheel_rejected':'pass','legacy_venv_preserved':'pass','QR_dependencies':'pass','private_radio':'pass'}))
