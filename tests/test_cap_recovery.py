# SPDX-License-Identifier: MIT
"""No hardware: exercise the production power/recovery/restore sequence."""
import sys
import errno
from pathlib import Path
import unittest
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'supervisor'))
import cap_lora as cap

class CapRecoveryTests(unittest.TestCase):
 def exercise(self,verify,expected_error=None,power='1',sleep_error=None,drive_rf=True):
  writes=[]
  with patch.object(cap,'preflight'), patch.object(cap,'_read_text',side_effect=[power,'1']), patch.object(cap,'_write_text',side_effect=lambda p,v:writes.append((p,v))), patch.object(cap.os,'open',return_value=123), patch.object(cap.os,'close') as close, patch.object(cap.fcntl,'ioctl'), patch.object(cap,'verify_expander',side_effect=verify) as read, patch.object(cap,'enable_rf_path') as rf, patch.object(cap.time,'sleep',side_effect=sleep_error):
   if expected_error:
    with self.assertRaises(expected_error):
     with cap.powered_cap(drive_rf=drive_rf):self.fail('must not enter after failed initialization')
   else:
    with cap.powered_cap(drive_rf=drive_rf) as chip:
     self.assertEqual(chip,0xa2 if drive_rf else None)
   self.assertEqual(writes[-2:],[(cap.MUX_LED,'1'),(cap.POWER_LED,cap.switch_value(power))])
   return writes,read.call_count,rf.call_count,close.call_count
 def test_first_success_does_not_cycle(self):
  writes,reads,rf,closed=self.exercise([0xa2])
  self.assertEqual((reads,rf,closed),(1,1,1));self.assertNotIn((cap.POWER_LED,'0'),writes)
 def test_no_response_recovers_once_before_rf_configuration(self):
  writes,reads,rf,closed=self.exercise([cap.CapError('absent','missing'),0xa2],power='255')
  self.assertEqual((reads,rf,closed),(2,1,2));self.assertEqual(writes.count((cap.POWER_LED,'0')),1)
 def test_absent_cap_stops_after_one_recovery_and_restores_off(self):
  writes,reads,rf,closed=self.exercise([cap.CapError('absent','missing')]*2,cap.CapError,power='0')
  self.assertEqual((reads,rf,closed),(2,0,2));self.assertEqual(writes.count((cap.POWER_LED,'0')),2)
 def test_wrong_chip_never_cycles_or_writes_rf(self):
  for kind in ('wrong-chip','permission','busy'):
   with self.subTest(kind=kind):
    writes,reads,rf,closed=self.exercise([cap.CapError(kind,'unavailable')],cap.CapError)
    self.assertEqual((reads,rf,closed),(1,0,1));self.assertNotIn((cap.POWER_LED,'0'),writes)
 def test_i2c_permission_error_is_not_misclassified_as_absent(self):
  with patch.object(cap,'_i2c_read_register',side_effect=PermissionError(errno.EACCES,'denied')):
   with self.assertRaises(cap.CapError) as error:cap.verify_expander(123)
   self.assertEqual(error.exception.kind,'permission')
 def test_interrupted_recovery_restores_original_switches(self):
  self.exercise([cap.CapError('absent','missing')],SystemExit,sleep_error=[None,SystemExit()])
 def test_power_only_does_not_probe_or_cycle(self):
  writes,reads,rf,closed=self.exercise([],drive_rf=False)
  self.assertEqual((reads,rf,closed),(0,0,0));self.assertNotIn((cap.POWER_LED,'0'),writes)
