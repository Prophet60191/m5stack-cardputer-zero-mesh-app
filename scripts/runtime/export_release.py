#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Prepare reviewable installer/source assets; never creates a tag or publishes."""
import argparse
import hashlib
import io
import json
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile

ROOT=Path(__file__).resolve().parents[2]

def sha(p):
    h=hashlib.sha256()
    with p.open('rb') as f:
        for block in iter(lambda:f.read(1024*1024),b''):h.update(block)
    return h.hexdigest()

def git(*args):
    return subprocess.check_output(['git','-c','safe.directory='+str(ROOT),'-C',str(ROOT),*args])


def write_install_instructions(path, source, version):
    """Keep the offline release instructions sourced from the maintained runbook."""
    header = (f"Cardputer Zero Mesh {version}\nDocumentation snapshot: {source}\n"
              f"Formatted runbook: https://github.com/Prophet60191/m5stack-cardputer-zero-mesh-app/blob/{source}/docs/install.md\n"
              "Qualification references identify the tested candidate; verify a newly built file against its own checksum set.\n\n")
    path.write_text(header + (ROOT/'docs/install.md').read_text())

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--deb',type=Path,required=True);p.add_argument('--native',type=Path,required=True)
    p.add_argument('--runtime',type=Path,required=True);p.add_argument('--python-source',type=Path,required=True)
    p.add_argument('--lvgl',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args()
    if git('status','--porcelain','--untracked-files=all').strip():raise ValueError('Export only clean committed application source')
    source=git('rev-parse','HEAD').decode().strip();meta=json.loads((ROOT/'app-builder.json').read_text());version=meta['version']
    control=subprocess.check_output(['dpkg-deb','-f',str(a.deb),'Package','Version','Architecture'],text=True)
    if f'Package: cardputerzero-mesh\nVersion: {version}-rev1\nArchitecture: arm64' not in control:raise ValueError('Wrong candidate package identity')
    # Verify the same native artifact/source pair used by runtime assembly.
    import sys;sys.path.insert(0,str(Path(__file__).parent))
    from prepare import check_native
    check_native(a.native)
    runtime=json.loads((a.runtime/'manifest.json').read_text())
    if runtime['native']['sha256']!=sha(a.native/'meshtasticd'):raise ValueError('Runtime/native artifact mismatch')
    a.output.mkdir(parents=True,exist_ok=True)
    package=a.output/a.deb.name;shutil.copy2(a.deb,package)
    archive=a.output/f'cardputerzero-mesh_{version}-source-and-dependencies.tar.gz'
    app_archive=git('archive','--format=tar','HEAD')
    lvgl_head=subprocess.check_output(['git','-c','safe.directory='+str(a.lvgl),'-C',str(a.lvgl),'rev-parse','HEAD'],text=True).strip()
    if lvgl_head!='85aa60d18b3d5e5588d7b247abf90198f07c8a63':raise ValueError('Wrong LVGL source')
    if subprocess.check_output(['git','-c','safe.directory='+str(a.lvgl),'-C',str(a.lvgl),'status','--porcelain','--untracked-files=no']):raise ValueError('LVGL source is modified')
    lvgl_archive=subprocess.check_output(['git','-c','safe.directory='+str(a.lvgl),'-C',str(a.lvgl),'archive','--format=tar','HEAD'])
    def normalized(info):
        info.uid=info.gid=0;info.uname=info.gname='root'
        return info
    def add_bytes(t,name,data):
        i=tarfile.TarInfo(name);i.size=len(data);i.mode=0o644;t.addfile(i,io.BytesIO(data))
    with tarfile.open(archive,'w:gz') as t:
        with tarfile.open(fileobj=io.BytesIO(app_archive)) as app:
            for m in app:
                if m.isfile():
                    m.name='app/'+m.name;t.addfile(normalized(m),app.extractfile(m))
                elif m.isdir():m.name='app/'+m.name;t.addfile(normalized(m))
                else:raise ValueError('Review non-regular application source member before release')
        add_bytes(t,'app/SOURCE_COMMIT',source.encode()+b'\n')
        with tarfile.open(fileobj=io.BytesIO(lvgl_archive)) as lib:
            for m in lib:
                original=m.name;m.name='dependencies/lvgl/'+m.name
                if m.isfile():t.addfile(normalized(m),lib.extractfile(m))
                elif m.isdir():t.addfile(normalized(m))
                elif m.issym():
                    # Preserve upstream source links only when their relative target stays inside the library.
                    from posixpath import normpath
                    target=normpath(str(Path(original).parent/m.linkname))
                    if m.linkname.startswith('/') or target.startswith('../'):raise ValueError('Unsafe LVGL source link')
                    t.addfile(normalized(m))
                else:raise ValueError('Unsupported LVGL source member')
        t.add(a.native/'corresponding-source.tar.gz',arcname='dependencies/native-corresponding-source.tar.gz',filter=normalized)
        t.add(a.native/'BUILD_INFO.json',arcname='dependencies/native-BUILD_INFO.json',filter=normalized)
        t.add(a.python_source,arcname='dependencies/python-corresponding-source.tar.gz',filter=normalized)
        t.add(a.runtime,arcname='dependencies/runtime',filter=normalized)
    shutil.copy2(a.runtime/'manifest.json',a.output/'RUNTIME_BUILD_INFO.json')
    (a.output/'SOURCE_COMMIT').write_text(source+'\n')
    write_install_instructions(a.output/'INSTALL.txt', source, version)
    shutil.copy2(ROOT/'docs/store-demo-checklist.md',a.output/'VIDEO-CHECKLIST.md')
    names=[package.name,archive.name,'RUNTIME_BUILD_INFO.json','SOURCE_COMMIT','INSTALL.txt','VIDEO-CHECKLIST.md']
    (a.output/'SHA256SUMS').write_text(''.join(sha(a.output/n)+'  '+n+'\n' for n in names))
    print(json.dumps({'source':source,'package_sha256':sha(package),'source_archive_sha256':sha(archive),'assets':names}))

if __name__=='__main__':main()
