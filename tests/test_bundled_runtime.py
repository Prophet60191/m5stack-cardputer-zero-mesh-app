# SPDX-License-Identifier: MIT
"""Package integrity and fixed native selection; no radio, network or user data."""
import hashlib
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'supervisor'))
import bundled_runtime as bundle

class BundledRuntimeTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.root=Path(self.temp.name)/'runtime';self.root.mkdir()
        (self.root/'radio').mkdir();(self.root/'wheels').mkdir()
        self.binary=self.root/'radio/meshtasticd';self.binary.write_bytes(b'synthetic native fixture');self.binary.chmod(0o755)
        (self.root/'wheels/synthetic-1-py3-none-any.whl').write_bytes(b'synthetic wheel fixture')
        (self.root/'requirements.lock').write_text('synthetic==1\n');(self.root/'python-lock.json').write_text('{}\n')
        self.m={'schema':1,'python':'3.13','architecture':'aarch64','python_packages':{'meshtastic':'2.7.11'},
            'wheels':[{'filename':'synthetic-1-py3-none-any.whl','sha256':bundle.file_hash(self.root/'wheels/synthetic-1-py3-none-any.whl'),'size':23}],
            'requirements_sha256':bundle.file_hash(self.root/'requirements.lock'),'python_lock_sha256':bundle.file_hash(self.root/'python-lock.json'),
            'native':{'version':bundle.VERSION,'sha256':bundle.file_hash(self.binary),'source_commit':bundle.NATIVE_COMMIT,'patch_sha256':bundle.PATCH_SHA256}}
        self.save()
    def tearDown(self):self.temp.cleanup()
    def save(self):
        identity={k:self.m[k] for k in ('python','architecture','python_packages','wheels','requirements_sha256')}
        self.m['python_runtime_id']=hashlib.sha256(bundle.canonical(identity)).hexdigest()
        self.m.pop('bundle_id',None);self.m['bundle_id']=hashlib.sha256(bundle.canonical(self.m)).hexdigest()
        (self.root/'manifest.json').write_text(json.dumps(self.m))
    def test_private_binary_is_selected_even_with_hostile_path(self):
        with patch.dict(os.environ,{'PATH':'/untrusted'}):
            self.assertEqual(bundle.daemon(self.root,owner=os.getuid()),(str(self.binary),bundle.VERSION))
    def test_changed_binary_is_rejected(self):
        self.binary.write_bytes(b'replaced executable')
        with self.assertRaisesRegex(ValueError,'checksum mismatch'):bundle.daemon(self.root,owner=os.getuid())
    def test_unqualified_source_is_rejected_even_with_consistent_manifest(self):
        self.m['native']['source_commit']='0'*40;self.save()
        with self.assertRaisesRegex(ValueError,'not qualified'):bundle.daemon(self.root,owner=os.getuid())
    def test_manifest_corruption_and_non_objects_have_clear_failures(self):
        for value in ('{','[]','true'):
            (self.root/'manifest.json').write_text(value)
            with self.assertRaises(ValueError):bundle.daemon(self.root,owner=os.getuid())
    def test_symlink_and_writable_native_are_rejected(self):
        original=self.root/'original';self.binary.rename(original);self.binary.symlink_to(original)
        with self.assertRaises(ValueError):bundle.daemon(self.root,owner=os.getuid())
        self.binary.unlink();original.rename(self.binary);self.binary.chmod(0o777)
        with self.assertRaises(ValueError):bundle.daemon(self.root,owner=os.getuid())
    def test_wrong_owner_is_rejected(self):
        with self.assertRaises(ValueError):bundle.daemon(self.root,owner=os.getuid()+1)
    def test_missing_native_and_corrupt_wheel_lock_fail_closed(self):
        self.binary.unlink()
        with self.assertRaisesRegex(ValueError,'missing'):bundle.daemon(self.root,owner=os.getuid())
        (self.root/'requirements.lock').write_text('changed')
        with self.assertRaisesRegex(ValueError,'lock checksum'):bundle.wheels(self.root,owner=os.getuid())
    def test_wheel_path_traversal_and_tampering_are_rejected(self):
        self.m['wheels'][0]['filename']='../escape.whl';self.save()
        with self.assertRaises(ValueError):bundle.wheels(self.root,owner=os.getuid())
        self.m['wheels'][0]['filename']='synthetic-1-py3-none-any.whl';self.save()
        (self.root/'wheels/synthetic-1-py3-none-any.whl').write_bytes(b'changed')
        with self.assertRaisesRegex(ValueError,'checksum mismatch'):bundle.wheels(self.root,owner=os.getuid())

if __name__=='__main__':unittest.main()
