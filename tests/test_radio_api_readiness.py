# SPDX-License-Identifier: MIT
"""Cold-start TCP timing tests; no sockets, Cap power or RF."""
from pathlib import Path
import sys,threading,unittest
from unittest.mock import Mock,patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'supervisor'))
import supervisor as mod

class ApiReadinessTests(unittest.TestCase):
 def setUp(self):
  self.s=object.__new__(mod.Supervisor);self.s._stop=threading.Event();self.s._daemon=Mock();self.s._daemon.poll.return_value=None;self.s._port=47400;self.s._iface=None
 def test_refused_connections_then_ready_subscribes_once(self):
  interface=object()
  with patch.object(mod,'observing_interface',side_effect=[ConnectionRefusedError(),ConnectionRefusedError(),interface]) as factory, patch('pubsub.pub.subscribe') as subscribe, patch.object(self.s._stop,'wait') as wait:
   self.s._connect_mesh()
   self.assertIs(self.s._iface,interface);self.assertEqual(factory.call_count,3);self.assertEqual(wait.call_count,2);self.assertEqual(subscribe.call_count,6)
   self.assertEqual(factory.call_args.kwargs,{'hostname':'127.0.0.1','portNumber':47400,'timeout':8})
 def test_refusal_deadline_is_not_reset_per_attempt(self):
  with patch.object(mod,'observing_interface',side_effect=ConnectionRefusedError()) as factory, patch.object(mod.time,'monotonic',side_effect=[0,1,9,10]), patch.object(self.s._stop,'wait'):
   with self.assertRaisesRegex(TimeoutError,'did not become ready'):self.s._connect_mesh()
   self.assertEqual(factory.call_count,3);self.assertIsNone(self.s._iface)
 def test_daemon_exit_stops_retries(self):
  self.s._daemon.poll.side_effect=[None,1]
  with patch.object(mod,'observing_interface',side_effect=ConnectionRefusedError()) as factory, patch.object(self.s._stop,'wait'):
   with self.assertRaisesRegex(RuntimeError,'daemon stopped'):self.s._connect_mesh()
   self.assertEqual(factory.call_count,1)
 def test_service_stop_cancels_wait(self):
  with patch.object(mod,'observing_interface',side_effect=ConnectionRefusedError()) as factory, patch.object(self.s._stop,'wait',side_effect=lambda _:self.s._stop.set()):
   with self.assertRaisesRegex(RuntimeError,'cancelled'):self.s._connect_mesh()
   self.assertEqual(factory.call_count,1)
 def test_handshake_error_is_not_retried(self):
  with patch.object(mod,'observing_interface',side_effect=TimeoutError('handshake failed')) as factory:
   with self.assertRaisesRegex(TimeoutError,'handshake failed'):self.s._connect_mesh()
   self.assertEqual(factory.call_count,1)
 def test_already_stopped_never_opens_connection(self):
  self.s._stop.set()
  with patch.object(mod,'observing_interface') as factory:
   with self.assertRaisesRegex(RuntimeError,'cancelled'):self.s._connect_mesh()
   factory.assert_not_called()
