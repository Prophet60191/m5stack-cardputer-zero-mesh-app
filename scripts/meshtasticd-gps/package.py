# SPDX-License-Identifier: GPL-3.0-or-later
"""Replace only the native executable in a supplied pristine baseline package."""
import argparse,hashlib,re,shutil,struct,subprocess,tempfile
from pathlib import Path

BASE_VERSION='2.7.26.61~obs54e0d8d~beta'
UPSTREAM='54e0d8d0ab2ff56b3a9ce967e53f79e49af560fb'
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--base-deb',type=Path,required=True);p.add_argument('--binary',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args()
fields=subprocess.check_output(['dpkg-deb','-f',str(a.base_deb),'Package','Version','Architecture'],text=True)
assert 'Package: meshtasticd' in fields and 'Version: '+BASE_VERSION in fields and 'Architecture: arm64' in fields,'unexpected baseline'
elf=a.binary.read_bytes();assert elf[:4]==b'\x7fELF' and elf[5]==1 and struct.unpack_from('<H',elf,18)[0]==183,'expected little-endian AArch64 ELF'
with tempfile.TemporaryDirectory() as temp:
 root=Path(temp)/'pkg';subprocess.run(['dpkg-deb','-R',str(a.base_deb.resolve()),str(root)],check=True)
 target=root/'usr/bin/meshtasticd';assert target.is_file() and not target.is_symlink()
 shutil.copyfile(a.binary,target);target.chmod(0o755)
 doc=root/'usr/share/doc/meshtasticd';doc.mkdir(parents=True,exist_ok=True)
 patch=Path(__file__).with_name('atgm-probe-timeout.patch')
 shutil.copyfile(patch,doc/'cardputer-gps-probe.patch')
 (doc/'cardputer-gps-probe.txt').write_text('''Local downstream GPS identification timeout correction, czgps1.
Upstream source: https://github.com/meshtastic/firmware/tree/'''+UPSTREAM+'''
Only the ATGM33xx identity wait changes from 500ms to 1500ms, plus its comment.
This accommodates measured AT6668 TXT responses queued at approximately 1Hz.
Receiver firmware, baud settings, RF and privacy defaults are unchanged.
Build/reproduction: scripts/meshtasticd-gps in the Cardputer Zero Mesh repository.
Keep the paired corresponding-source archive when distributing the binary.
Compiled binary SHA256: '''+hashlib.sha256(elf).hexdigest()+'\n')
 control=root/'DEBIAN/control';text=control.read_text()
 # Derive requirements from the rebuilt ELF rather than assuming its toolchain
 # produced exactly the baseline library requirements.
 scan=Path(temp)/'debian';scan.mkdir()
 (scan/'control').write_text('Source: meshtasticd-gps-check\n\nPackage: meshtasticd-gps-check\nArchitecture: any\nDescription: dependency scan\n')
 deps=subprocess.check_output(['dpkg-shlibdeps','-O','-e'+str(target)],cwd=temp,text=True).strip().removeprefix('shlibs:Depends=')
 existing=re.search(r'^Depends:(.*(?:\n[ \t]+.*)*)',text,re.M).group(1).replace('\n',' ').strip()
 merged={d.strip().split()[0]:d.strip() for d in existing.split(',')}
 merged.update({d.strip().split()[0]:d.strip() for d in deps.split(',')})
 text=re.sub(r'^Depends:.*(?:\n[ \t]+.*)*', 'Depends: '+', '.join(merged.values()),text,flags=re.M)
 text=re.sub(r'^Version:.*$', 'Version: '+BASE_VERSION+'+czgps1',text,flags=re.M)
 size=sum(f.stat().st_size for f in root.rglob('*') if f.is_file() and not f.is_symlink() and root/'DEBIAN' not in f.parents)
 text=re.sub(r'^Installed-Size:.*$', 'Installed-Size: '+str((size+1023)//1024),text,flags=re.M);control.write_text(text)
 rows=[]
 for f in sorted(root.rglob('*')):
  if f.is_file() and not f.is_symlink() and root/'DEBIAN' not in f.parents:rows.append(hashlib.md5(f.read_bytes()).hexdigest()+'  '+str(f.relative_to(root)))
 (root/'DEBIAN/md5sums').write_text('\n'.join(rows)+'\n')
 a.output.parent.mkdir(parents=True,exist_ok=True)
 subprocess.run(['dpkg-deb','--build','--root-owner-group','-Zgzip',str(root),str(a.output.resolve())],check=True)
