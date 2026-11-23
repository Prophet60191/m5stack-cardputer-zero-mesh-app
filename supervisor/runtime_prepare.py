#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Prepare the logged-in user's managed Python cache using package wheels only."""
import fcntl
import json
import os
from pathlib import Path
import shutil
import stat
import subprocess
import sys
import time

HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE))
import bundled_runtime


def private_directory(path, *, allow_read=False):
    path.mkdir(mode=0o700,parents=True,exist_ok=True)
    st=path.lstat()
    if not stat.S_ISDIR(st.st_mode) or st.st_uid!=os.getuid() or st.st_mode & (0o022 if allow_read else 0o077):
        raise ValueError('Managed runtime directory is not private and owned by this user')


def clean_environment():
    env=dict(os.environ)
    for key in list(env):
        if key.startswith('PIP_') or key in ('PYTHONPATH','PYTHONHOME'):env.pop(key,None)
    env.update(PYTHONNOUSERSITE='1', PIP_CONFIG_FILE='/dev/null', PIP_DISABLE_PIP_VERSION_CHECK='1', PIP_NO_INPUT='1')
    return env


def healthy(python, versions):
    code='import importlib.metadata as m,json,sys; p=json.loads(sys.argv[1]); assert all(m.version(n)==v for n,v in p.items()); import meshtastic.tcp_interface, yaml; from pubsub import pub'
    try:
        return subprocess.run([str(python),'-I','-c',code,json.dumps(versions)],env=clean_environment(),
                              stdout=subprocess.DEVNULL,stderr=subprocess.PIPE,timeout=20).returncode==0
    except (OSError,subprocess.TimeoutExpired):return False


def prepare(root=None, *, owner=0, data_home=None):
    if sys.version_info[:2]!=(3,13):raise ValueError('This package requires Debian 13 Python 3.13')
    root=bundled_runtime.ROOT if root is None else Path(root)
    m=bundled_runtime.wheels(root,owner=owner)
    base=Path(data_home) if data_home is not None else Path(os.environ.get('XDG_DATA_HOME',str(Path.home()/'.local/share')))
    app=base/'cardputerzero-mesh';private_directory(app,allow_read=True)
    runtimes=app/'managed-runtimes';private_directory(runtimes)
    fd=os.open(runtimes/'prepare.lock',os.O_CREAT|os.O_RDWR|os.O_NOFOLLOW|os.O_CLOEXEC,0o600)
    try:
        st=os.fstat(fd)
        if not stat.S_ISREG(st.st_mode) or st.st_uid!=os.getuid() or st.st_mode&0o077 or st.st_nlink!=1:
            raise ValueError('Unsafe managed runtime lock')
        deadline=time.monotonic()+60
        while True:
            try:fcntl.flock(fd,fcntl.LOCK_EX|fcntl.LOCK_NB);break
            except BlockingIOError:
                if time.monotonic()>=deadline:raise TimeoutError('Another offline runtime preparation is still running')
                time.sleep(.1)
        target=runtimes/m['python_runtime_id'];python=target/'bin/python';ready=target/'ready.json'
        if target.exists() or target.is_symlink():
            private_directory(target)
            if ready.is_file() and not ready.is_symlink():
                bundled_runtime.trusted(ready,os.getuid(),maximum=4096)
                try:valid=json.loads(ready.read_text())=={'python_runtime_id':m['python_runtime_id']}
                except (OSError,ValueError):valid=False
                if valid and healthy(python,m['python_packages']):return python
            # Only this fixed private cache prefix is disposable; user data/legacy venv are untouched.
            shutil.rmtree(target)
        print('Preparing the bundled radio runtime offline. First launch may take a moment.',file=sys.stderr,flush=True)
        previous=os.umask(0o077)
        try:
            target.mkdir(mode=0o700)
            subprocess.run(['/usr/bin/python3','-I','-m','venv','--system-site-packages',str(target)],
                           check=True,env=clean_environment(),stdout=sys.stderr,stderr=sys.stderr,timeout=90)
            subprocess.run([str(python),'-I','-m','pip','install','--no-index','--no-deps','--ignore-installed',
                            '--only-binary=:all:','--require-hashes','--no-compile',
                            '--find-links',str(root/'wheels'),'-r',str(root/'requirements.lock')],
                           check=True,env=clean_environment(),stdout=sys.stderr,stderr=sys.stderr,timeout=120)
            if not healthy(python,m['python_packages']):raise RuntimeError('Bundled Python runtime import/version check failed')
            ready.write_text(json.dumps({'python_runtime_id':m['python_runtime_id']})+'\n')
            os.chmod(ready,0o600)
        finally:os.umask(previous)
        return python
    finally:os.close(fd)


def main():
    if os.geteuid()==0:raise SystemExit('Run runtime preparation as the logged-in device user, without sudo')
    try:print(prepare())
    except (OSError,ValueError,RuntimeError,subprocess.SubprocessError) as exc:
        print('Mesh offline runtime setup failed: '+str(exc),file=sys.stderr);return 1
    return 0

if __name__=='__main__':raise SystemExit(main())
