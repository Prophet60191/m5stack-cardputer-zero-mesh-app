# SPDX-License-Identifier: GPL-3.0-or-later
"""Pair native object code with the source, dependencies and build instructions."""
import hashlib,json,platform,subprocess,sys,tarfile
from pathlib import Path
source,out,core=map(Path,sys.argv[1:4]);support=Path(__file__).resolve().parent
assert (core/'packages').is_dir() and (core/'platforms').is_dir(),'retain the PlatformIO core volume'
def keep(info):
 parts=Path(info.name).parts
 if '.git' in parts or '.DS_Store' in parts or '__pycache__' in parts or any(p.startswith('._') for p in parts) or '.pio/build' in info.name:return None
 info.uid=info.gid=0;info.uname=info.gname='root'
 return info
bundle=out/'corresponding-source.tar.gz'
with tarfile.open(bundle,'w:gz') as t:
 t.add(source,arcname='firmware',filter=keep)
 for folder in ('packages','platforms'):t.add(core/folder,arcname='build-dependencies/'+folder,filter=keep)
 t.add(support,arcname='build-support',filter=keep)
 # Minimal PUBLIC upstream Git objects preserve the version banner for offline
 # rebuilds. Never include configs, remotes, hooks, credentials or reflogs.
 git=source/'.git'
 def public_git(info):
  info.uid=info.gid=0;info.uname=info.gname='root'
  return info if not info.issym() and not info.islnk() else None
 for name in ('HEAD','shallow','objects','refs'):
  p=git/name
  if p.exists():t.add(p,arcname='firmware/.git/'+name,filter=public_git)

info={'upstream_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=source,text=True).strip(),
 'patch_sha256':hashlib.sha256((support/'atgm-probe-timeout.patch').read_bytes()).hexdigest(),
 'platformio':subprocess.check_output(['pio','--version'],text=True).strip(),
 'compiler':subprocess.check_output(['g++','--version'],text=True).splitlines()[0],
 'architecture':platform.machine(),
 'native_sha256':hashlib.sha256((out/'meshtasticd').read_bytes()).hexdigest()}
(out/'BUILD_INFO.json').write_text(json.dumps(info,indent=2)+'\n')
files=('meshtasticd','corresponding-source.tar.gz','NATIVE_VERSION','BUILD_INFO.json')
(out/'SHA256SUMS').write_text(''.join(hashlib.sha256((out/n).read_bytes()).hexdigest()+'  '+n+'\n' for n in files))
