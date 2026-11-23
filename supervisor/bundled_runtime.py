# SPDX-License-Identifier: GPL-3.0-or-later
"""Validate the fixed, package-owned runtime. No PATH lookup or executable intents."""
import hashlib
import json
import re
import stat
from pathlib import Path

ROOT = Path(__file__).resolve().parent / 'runtime'
NATIVE_COMMIT = '54e0d8d0ab2ff56b3a9ce967e53f79e49af560fb'
PATCH_SHA256 = '33620bd494e4de69eff9944a9ae3d98418dada08f008355ee78947c215e64036'
VERSION = '2.7.26.54e0d8d+czgps1'


def canonical(value):
    return json.dumps(value, sort_keys=True, separators=(',', ':')).encode()


def file_hash(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024*1024), b''): h.update(block)
    return h.hexdigest()


def trusted(path, owner=0, *, directory=False, maximum=None, executable=False):
    st = path.lstat()
    kind = stat.S_ISDIR if directory else stat.S_ISREG
    if not kind(st.st_mode) or st.st_uid != owner or st.st_mode & 0o022:
        raise ValueError('Bundled runtime ownership or permissions are unsafe; reinstall the verified app package')
    if not directory and (st.st_nlink != 1 or maximum is not None and st.st_size > maximum):
        raise ValueError('Bundled runtime file is invalid; reinstall the verified app package')
    if executable and not st.st_mode & 0o100:
        raise ValueError('Bundled radio is not executable; reinstall the verified app package')


def load(root=None, *, owner=0):
    root = ROOT if root is None else Path(root)
    try:
        trusted(root.parent, owner, directory=True); trusted(root, owner, directory=True)
        trusted(root/'manifest.json', owner, maximum=128*1024)
        m = json.loads((root/'manifest.json').read_text())
        if not isinstance(m,dict):raise ValueError('Bundled runtime manifest must be an object')
        if type(m.get('schema')) is not int or m.get('schema') != 1 or m.get('python') != '3.13' or m.get('architecture') != 'aarch64':
            raise ValueError('Bundled runtime platform is not qualified')
        bundle_id = m.get('bundle_id');body={k:v for k,v in m.items() if k!='bundle_id'}
        if bundle_id != hashlib.sha256(canonical(body)).hexdigest(): raise ValueError('Bundled runtime manifest checksum mismatch')
        if not isinstance(m.get('python_packages'),dict):raise ValueError('Bundled Python package inventory is invalid')
        if m['python_packages'].get('meshtastic') != '2.7.11': raise ValueError('Bundled client is not qualified')
        identity={k:m[k] for k in ('python','architecture','python_packages','wheels','requirements_sha256')}
        if m.get('python_runtime_id') != hashlib.sha256(canonical(identity)).hexdigest():raise ValueError('Bundled Python identity mismatch')
        if not isinstance(m['wheels'],list) or not 1<=len(m['wheels'])<=32:raise ValueError('Bundled wheel inventory is invalid')
        names=set()
        for wheel in m['wheels']:
            name=wheel['filename']
            if not isinstance(name,str) or not re.fullmatch(r'[A-Za-z0-9_.+-]+\.whl',name) or name in names:
                raise ValueError('Bundled wheel filename is invalid')
            names.add(name)
            if not re.fullmatch('[0-9a-f]{64}',wheel['sha256']) or not 0<wheel['size']<=64*1024*1024:
                raise ValueError('Bundled wheel checksum or size is invalid')
        n=m['native']
        if not isinstance(n,dict):raise ValueError('Bundled radio inventory is invalid')
        if n.get('version')!=VERSION or n.get('source_commit')!=NATIVE_COMMIT or n.get('patch_sha256')!=PATCH_SHA256:
            raise ValueError('Bundled radio source is not qualified')
        if not re.fullmatch('[0-9a-f]{64}',n['sha256']):raise ValueError('Bundled radio checksum is invalid')
        return m
    except (OSError, KeyError, TypeError, json.JSONDecodeError) as exc:
        raise ValueError('Bundled runtime is missing or invalid; reinstall the verified app package') from exc


def daemon(root=None, *, owner=0):
    root = ROOT if root is None else Path(root)
    m=load(root,owner=owner)
    try:
        trusted(root/'radio',owner,directory=True)
        p=root/'radio/meshtasticd';trusted(p,owner,maximum=32*1024*1024,executable=True)
        if file_hash(p)!=m['native']['sha256']:raise ValueError('Bundled radio checksum mismatch; reinstall the verified app package')
        return str(p),m['native']['version']
    except OSError as exc:
        raise ValueError('Bundled radio is missing; reinstall the verified app package') from exc


def wheels(root=None, *, owner=0):
    root=ROOT if root is None else Path(root);m=load(root,owner=owner)
    try:
        trusted(root/'wheels',owner,directory=True)
        for item in m['wheels']:
            p=root/'wheels'/item['filename'];trusted(p,owner,maximum=64*1024*1024)
            if p.stat().st_size!=item['size'] or file_hash(p)!=item['sha256']:raise ValueError('Bundled Python wheel checksum mismatch')
        for name,key in (('requirements.lock','requirements_sha256'),('python-lock.json','python_lock_sha256')):
            p=root/name;trusted(p,owner,maximum=1024*1024)
            if file_hash(p)!=m[key]:raise ValueError('Bundled Python lock checksum mismatch')
        return m
    except OSError as exc:
        raise ValueError('Bundled Python wheels are missing; reinstall the verified app package') from exc
