#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""One-command Debian arm64 build, with verified dependencies and an offline mode."""
import argparse
import fcntl
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
COMMIT='54e0d8d0ab2ff56b3a9ce967e53f79e49af560fb'
LVGL='85aa60d18b3d5e5588d7b247abf90198f07c8a63'
NATIVE_IMAGE='cz-mesh-native-build:debian13'
APP_IMAGE='cz-mesh-package-build:debian13'


def run(args,**kwargs):
    print('+ '+str(args[0])+' '+str(args[1]) if len(args)>1 else '+ '+str(args[0]),flush=True)
    subprocess.run(list(map(str,args)),check=True,**kwargs)


def docker(image,args,mounts,offline=False,extras=()):
    cmd=['docker','run','--rm','--platform','linux/arm64']
    if offline:cmd+=['--network','none']
    cmd+=list(extras)
    for src,dst,read_only in mounts:cmd+=['-v',str(Path(src).resolve())+':'+dst+(':ro' if read_only else '')]
    run(cmd+[image,*args])


def fetch(repo,sha,path,offline):
    if path.exists():
        got=subprocess.check_output(['git','-C',str(path),'rev-parse','HEAD'],text=True).strip()
        if got!=sha:raise ValueError('Build cache has the wrong revision: '+str(path))
        # Never erase/edit an operator's cache to make a dirty checkout pass.
        if subprocess.check_output(['git','-C',str(path),'status','--porcelain','--untracked-files=no'],text=True).strip():
            raise ValueError('Source cache was modified: '+str(path))
        return
    if offline:raise ValueError('Offline source cache missing: '+str(path))
    path.parent.mkdir(parents=True,exist_ok=True)
    run(['git','init','--quiet',path]);run(['git','-C',path,'fetch','--depth','1',repo,sha]);run(['git','-C',path,'checkout','--detach','FETCH_HEAD'])


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output',type=Path,default=ROOT/'build/distribution')
    p.add_argument('--native-artifact',type=Path,help='Optional verified native artifact directory; otherwise build pinned source')
    p.add_argument('--offline',action='store_true',help='Use preloaded images and verified cached artifacts only')
    p.add_argument('--prepare-only',action='store_true')
    a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);cache=out/'cache';cache.mkdir(exist_ok=True)
    build_lock=(out/'build.lock').open('a')
    try:fcntl.flock(build_lock.fileno(),fcntl.LOCK_EX|fcntl.LOCK_NB)
    except BlockingIOError:raise SystemExit('Another build is using this output directory; choose a different --output')
    if a.offline:
        for image in (NATIVE_IMAGE,APP_IMAGE):run(['docker','image','inspect',image],stdout=subprocess.DEVNULL)
    else:
        run(['docker','build','--platform','linux/arm64','-t',NATIVE_IMAGE,ROOT/'scripts/meshtasticd-gps'])
        run(['docker','build','--platform','linux/arm64','-t',APP_IMAGE,'-f',ROOT/'tests/Dockerfile',ROOT])
    native=a.native_artifact.resolve() if a.native_artifact else out/'native'
    if not (native/'BUILD_INFO.json').is_file():
        if a.offline:raise ValueError('Offline native artifacts are missing; prepare once with networking or unpack the source/dependency bundle')
        native.mkdir(parents=True,exist_ok=True)
        if list(native.iterdir()):raise ValueError('Refusing to overwrite an incomplete native artifact directory')
        with tempfile.TemporaryDirectory(prefix='native-work-',dir=cache) as temp:
            source=Path(temp)/'firmware';fetch('https://github.com/meshtastic/firmware.git',COMMIT,source,False)
            run(['git','-C',source,'submodule','update','--init','--depth','1','protobufs'])
            pio=cache/'pio';pio.mkdir(exist_ok=True)
            docker(NATIVE_IMAGE,['sh','-c','git config --global --add safe.directory /work; sh /support/build.sh'],
                   [(source,'/work',False),(pio,'/root/.platformio',False),(native,'/out',False),(ROOT/'scripts/meshtasticd-gps','/support',True)])
    lvgl=cache/'lvgl'
    fetch('https://github.com/lvgl/lvgl.git',LVGL,lvgl,a.offline)
    baseline=cache/'baseline';baseline.mkdir(exist_ok=True);old=baseline/'mesh-0.1.8.deb'
    if not old.exists():
        if a.offline:raise ValueError('Offline upgrade baseline is missing')
        import urllib.request,hashlib
        url='https://github.com/Prophet60191/m5stack-cardputer-zero-mesh-app/releases/download/v0.1.8-rc.1/cardputerzero-mesh_0.1.8_rev1_arm64.deb'
        with urllib.request.urlopen(url,timeout=30) as stream:old.write_bytes(stream.read(32*1024*1024))
    import hashlib
    if hashlib.sha256(old.read_bytes()).hexdigest()!='8ff56073d45b43ed1cd1212f46bbdfcc9e345219c0897fbdf97d5fac2ad7b3b4':raise ValueError('Upgrade baseline checksum mismatch')
    (cache/'python').mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='runtime-stage-',dir=out) as temp:
        stage=Path(temp)
        docker(APP_IMAGE,['python3','/src/scripts/runtime/prepare.py','--native','/native','--cache','/cache','--output','/out/runtime',*(['--offline'] if a.offline else [])],
               [(ROOT,'/src',True),(native,'/native',True),(cache/'python','/cache',False),(stage,'/out',False)],a.offline)
        # Assembly verifies provenance and hashes before executing any cached binary.
        docker(NATIVE_IMAGE,['python3','/support/test_daemon.py','/out/meshtasticd'],
               [(native,'/out',True),(ROOT/'scripts/meshtasticd-gps','/support',True)],True,('-e','CZ_MESH_ISOLATED_TEST=1'))
        if a.prepare_only:
            target=out/'prepared'
            if target.exists():
                if (target/'runtime/manifest.json').read_bytes()!=(stage/'runtime/manifest.json').read_bytes():raise ValueError('Prepared output changed; choose a new output directory')
            else:shutil.copytree(stage,target)
            print('Verified dependency cache prepared.');return
        work=out/'package-check';work.mkdir(exist_ok=True)
        docker(APP_IMAGE,['sh','/src/tests/linux-package-check.sh'],
               [(ROOT,'/src',True),(lvgl,'/lvgl',True),(baseline,'/baseline',True),(stage/'runtime','/runtime',True),(work,'/work',False)],True,
               ('--cap-add','NET_ADMIN','-e','CZ_MESH_ISOLATED_TEST=1','-e','CZ_MESH_UPGRADE_BASELINE=/baseline/mesh-0.1.8.deb'))
        artifacts=out/'artifacts';artifacts.mkdir(exist_ok=True)
        version=json.loads((ROOT/'app-builder.json').read_text())['version']
        package=work/'linux/dist'/f'cardputerzero-mesh_{version}-rev1_arm64.deb';shutil.copy2(package,artifacts/package.name)
        shutil.copy2(stage/'python-corresponding-source.tar.gz',artifacts/'python-corresponding-source.tar.gz')
        shutil.copy2(stage/'runtime/manifest.json',artifacts/'RUNTIME_BUILD_INFO.json')
        print('Verified installer:',artifacts/package.name)
        with tempfile.TemporaryDirectory(prefix='fresh-context-',dir=out) as temp:
            context=Path(temp)
            shutil.copy2(package,context/'cardputerzero-mesh.deb')
            shutil.copy2(ROOT/'tests/Dockerfile.fresh-runtime',context/'Dockerfile')
            run(['docker','build','--platform','linux/arm64','-t','cz-mesh-fresh-check',context])
        docker('cz-mesh-fresh-check',['sh','/src/tests/fresh-runtime-check.sh'],[(ROOT,'/src',True)],True,
               ('-e','CZ_MESH_ISOLATED_TEST=1'))
        clean=not subprocess.check_output(['git','-C',str(ROOT),'status','--porcelain','--untracked-files=all']).strip()
        if clean:
            docker(APP_IMAGE,['python3','/src/scripts/runtime/export_release.py','--deb','/work/linux/dist/'+package.name,
                   '--native','/native','--runtime','/stage/runtime','--python-source','/stage/python-corresponding-source.tar.gz',
                   '--lvgl','/lvgl','--output','/out'],[(ROOT,'/src',True),(work,'/work',True),(native,'/native',True),
                   (stage,'/stage',True),(lvgl,'/lvgl',True),(artifacts,'/out',False)],True)
        else:
            print('Development build only: commit clean source before exporting redistribution assets.')

if __name__=='__main__':main()
