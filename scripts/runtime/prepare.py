#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Build a private, offline runtime from reviewed artifacts; never accesses hardware."""
import argparse
import hashlib
import json
import os
import platform
import shutil
import tarfile
import tempfile
import urllib.parse
import urllib.request
import zipfile
from pathlib import Path, PurePosixPath
from packaging.tags import sys_tags
from packaging.utils import parse_wheel_filename

ROOT = Path(__file__).resolve().parents[2]
NATIVE_COMMIT = '54e0d8d0ab2ff56b3a9ce967e53f79e49af560fb'
PATCH_SHA256 = '33620bd494e4de69eff9944a9ae3d98418dada08f008355ee78947c215e64036'

def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for b in iter(lambda: f.read(1024*1024), b''): h.update(b)
    return h.hexdigest()

def canonical(value):
    return json.dumps(value, sort_keys=True, separators=(',', ':')).encode()

def fetch(item, cache, offline):
    name = item['filename']
    if Path(name).name != name or name in ('.', '..'): raise ValueError('unsafe artifact filename')
    path = cache/name
    if path.is_file() and not path.is_symlink() and digest(path) == item['sha256']: return path
    if offline: raise RuntimeError('Offline artifact missing or corrupt: '+name)
    url = urllib.parse.urlparse(item['url'])
    if url.scheme != 'https' or url.hostname != 'files.pythonhosted.org': raise ValueError('untrusted download host')
    if not 0 < item['size'] <= 64*1024*1024: raise ValueError('artifact size outside build bound')
    fd, tmp = tempfile.mkstemp(prefix='.download-', dir=cache)
    try:
        size = 0
        with os.fdopen(fd, 'wb') as out, urllib.request.urlopen(item['url'], timeout=30) as response:
            while block := response.read(1024*1024):
                size += len(block)
                if size > item['size']: raise ValueError('artifact exceeds pinned size')
                out.write(block)
        if size != item['size'] or digest(Path(tmp)) != item['sha256']: raise ValueError('artifact checksum mismatch')
        os.replace(tmp, path)
    finally:
        if os.path.exists(tmp): os.unlink(tmp)
    return path

def check_native(root):
    for name in ('meshtasticd', 'corresponding-source.tar.gz', 'NATIVE_VERSION', 'BUILD_INFO.json'):
        entries = dict((n, h) for h, n in (line.split() for line in (root/'SHA256SUMS').read_text().splitlines()))
        if name not in entries or digest(root/name) != entries[name]: raise ValueError('native artifact checksum mismatch: '+name)
    info = json.loads((root/'BUILD_INFO.json').read_text())
    if (info.get('upstream_commit') != NATIVE_COMMIT or info.get('patch_sha256') != PATCH_SHA256
            or info.get('architecture') not in ('aarch64','arm64') or info.get('native_sha256') != digest(root/'meshtasticd')):
        raise ValueError('native provenance is not qualified')
    b = (root/'meshtasticd').read_bytes()[:20]
    if b[:5] != b'\x7fELF\x02' or b[5] != 1 or int.from_bytes(b[18:20], 'little') != 183:
        raise ValueError('native artifact is not little-endian AArch64 ELF')
    if not (root/'NATIVE_VERSION').read_text().strip().startswith('2.7.26.54e0d8d'):
        raise ValueError('unqualified native version')
    return info

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--native', type=Path, required=True)
    ap.add_argument('--cache', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    ap.add_argument('--offline', action='store_true')
    a = ap.parse_args()
    if platform.machine() not in ('aarch64','arm64') or tuple(__import__('sys').version_info[:2]) != (3,13):
        ap.error('run inside the documented Debian 13 arm64 build container')
    info = check_native(a.native)
    lock_path = ROOT/'packaging/runtime/python-lock.json'
    lock = json.loads(lock_path.read_text())
    a.cache.mkdir(parents=True, exist_ok=True)
    if a.output.exists(): raise ValueError('output must be a new directory')
    a.output.mkdir(parents=True)
    wheels_dir = a.output/'wheels'; wheels_dir.mkdir()
    source_dir = a.output.parent/'python-sources'; source_dir.mkdir(exist_ok=False)
    rank = {tag:i for i,tag in enumerate(sys_tags())}
    selected = []; requirements = []; source_inventory = []
    for p in lock['packages']:
        candidates = []
        for item in p['artifacts']:
            if item['kind'] != 'bdist_wheel': continue
            tags = parse_wheel_filename(item['filename'])[3]
            score = min((rank[t] for t in tags if t in rank), default=None)
            if score is not None: candidates.append((score, item['filename'], item))
        if not candidates: raise ValueError('no qualified wheel for '+p['name'])
        item = min(candidates)[2]; path = fetch(item, a.cache, a.offline)
        shutil.copyfile(path, wheels_dir/path.name)
        selected.append({k:item[k] for k in ('filename','sha256','size')})
        requirements.append(f"{p['name']}=={p['version']} --hash=sha256:{item['sha256']}")
        sources = [i for i in p['artifacts'] if i['kind']=='sdist']
        # pyzbar's Linux wheel is pure Python; that published wheel is also its source.
        if not sources:
            if p['name'] != 'pyzbar' or not path.name.endswith('-none-any.whl'): raise ValueError('source artifact missing')
            sources = [item]
        for original in sources:
            src = fetch(original, a.cache, a.offline);shutil.copyfile(src, source_dir/src.name)
            source_inventory.append({'package':p['name'],'version':p['version'], **original})
            # Older wheels (notably pyserial 3.5) omit license files; retain
            # their original source-distribution texts as well.
            if original['kind']=='sdist':
                with tarfile.open(src) as archive:
                    for member in archive:
                        part=PurePosixPath(member.name);name=part.name.lower()
                        if not member.isfile() or not name.startswith(('license','copying','notice','copyright')):continue
                        if part.is_absolute() or '..' in part.parts or member.size>1024*1024:raise ValueError('unsafe source license member')
                        dest=a.output/'licenses/python'/p['name']/'source'/part
                        dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(archive.extractfile(member).read())
        with zipfile.ZipFile(path) as z:
            for name in z.namelist():
                part = PurePosixPath(name)
                if part.is_absolute() or '..' in part.parts: raise ValueError('unsafe wheel path')
                if '.dist-info/' in name and any(t in part.name.lower() for t in ('license','copying','notice','copyright')) and not name.endswith('/'):
                    dest = a.output/'licenses/python'/p['name']/part;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(z.read(name))
    for package in lock['packages']:
        if not any((a.output/'licenses/python'/package['name']).rglob('*')):raise ValueError('Missing original license text: '+package['name'])
    (a.output/'requirements.lock').write_text('\n'.join(requirements)+'\n')
    shutil.copyfile(lock_path,a.output/'python-lock.json')
    (source_dir/'SOURCE_INVENTORY.json').write_text(json.dumps(source_inventory,indent=2)+'\n')
    shutil.copyfile(lock_path,source_dir/'python-lock.json')
    with tarfile.open(a.output.parent/'python-corresponding-source.tar.gz','w:gz') as t:t.add(source_dir,arcname='python-sources')
    shutil.rmtree(source_dir)
    radio = a.output/'radio';radio.mkdir();shutil.copyfile(a.native/'meshtasticd',radio/'meshtasticd');os.chmod(radio/'meshtasticd',0o755)
    # Preserve license texts for the native executable's source and linked components.
    with tarfile.open(a.native/'corresponding-source.tar.gz') as t:
        for member in t:
            part=PurePosixPath(member.name);name=part.name.lower()
            if not member.isfile() or not (name.startswith(('license','copying','notice')) or name=='copyright'):continue
            if part.is_absolute() or '..' in part.parts or member.size>1024*1024:raise ValueError('unsafe license member')
            dest=a.output/'licenses/native'/part;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(t.extractfile(member).read())
    manifest={'schema':1,'python':'3.13','architecture':'aarch64',
        'python_packages':{p['name']:p['version'] for p in lock['packages']},'wheels':selected,
        'requirements_sha256':digest(a.output/'requirements.lock'),'python_lock_sha256':digest(lock_path),
        'native':{'version':'2.7.26.54e0d8d+czgps1','sha256':info['native_sha256'],'source_commit':NATIVE_COMMIT,
                  'patch_sha256':PATCH_SHA256,'source_archive_sha256':digest(a.native/'corresponding-source.tar.gz')}}
    python_identity={k:manifest[k] for k in ('python','architecture','python_packages','wheels','requirements_sha256')}
    manifest['python_runtime_id']=hashlib.sha256(canonical(python_identity)).hexdigest()
    manifest['bundle_id']=hashlib.sha256(canonical(manifest)).hexdigest()
    (a.output/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    (a.output/'DEPENDENCIES.md').write_text('# Bundled runtime\n\nThe original Python wheels include their licenses; copies of available license texts\nare under licenses/python. pyzbar is distributed as its original pure-Python wheel.\nNative component licenses are under licenses/native. Full corresponding native and\nPython source archives accompany the distribution; preserve them when redistributing.\nNo user data, credentials, radio configuration or system daemon is bundled.\n')
    for p in a.output.rglob('*'):
        if p.is_file() and p != radio/'meshtasticd':os.chmod(p,0o644)
    print(json.dumps({'runtime_id':manifest['python_runtime_id'],'wheels':len(selected),'native_sha256':info['native_sha256']}))

if __name__=='__main__':main()
