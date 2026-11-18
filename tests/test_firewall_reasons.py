# SPDX-License-Identifier: MIT
"""Verifier failures stay fail-closed and reveal no raw command output."""
import json,os,sys,tempfile,unittest,subprocess
from pathlib import Path
from unittest.mock import patch
from types import SimpleNamespace
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'supervisor'))
import firewall

class FirewallReasonTests(unittest.TestCase):
    def test_timeout_and_execution_remain_failed(self):
        for error,kind in [(subprocess.TimeoutExpired('nft',3),'timeout'),(OSError('private path'),'execution')]:
            with self.subTest(kind=kind),patch.object(firewall.subprocess,'run',side_effect=error):
                self.assertEqual(firewall.verify('/usr/sbin/nft'),(False,kind))
    def test_command_error_never_echoes_output(self):
        with patch.object(firewall.subprocess,'run',return_value=SimpleNamespace(returncode=1,stdout='private host',stderr='private key')):
            self.assertEqual(firewall.verify('/usr/sbin/nft'),(False,'command'))
    def test_bad_data_is_failed(self):
        for value in ['not JSON','[]','{"nftables":[null]}']:
            with self.subTest(value=value),patch.object(firewall.subprocess,'run',return_value=SimpleNamespace(returncode=0,stdout=value)):
                self.assertEqual(firewall.verify('/usr/sbin/nft'),(False,'json'))
    def test_success_and_rules_mismatch_are_distinct(self):
        with patch.object(firewall.subprocess,'run',return_value=SimpleNamespace(returncode=0,stdout='{}')):
            with patch.object(firewall,'protected',return_value=True):self.assertEqual(firewall.verify('/usr/sbin/nft'),(True,''))
            with patch.object(firewall,'protected',return_value=False):self.assertEqual(firewall.verify('/usr/sbin/nft'),(False,'rules'))
    def test_health_reasons_are_allowlisted_and_never_make_failure_healthy(self):
        with tempfile.TemporaryDirectory() as directory:
            p=Path(directory)/'status.json'
            for reason in ['timeout','command','execution','json','rules','stopping','private key',{'bad':'type'}]:
                p.write_text(json.dumps({'ok':False,'checked':100,'boot':'synthetic','failure':reason}));p.chmod(0o600)
                with self.subTest(reason=reason),self.assertRaises(RuntimeError) as caught:
                    firewall.health_status(p,uid=os.getuid(),now=101,boot='synthetic')
                self.assertNotIn('private key',str(caught.exception));self.assertIn('keep radio off',str(caught.exception))
if __name__=='__main__':unittest.main()
