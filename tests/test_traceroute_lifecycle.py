# SPDX-License-Identifier: MIT
"""Real protobuf replies and delayed radio work; no sockets, hardware or RF."""
import threading
import unittest
from unittest.mock import Mock, patch
from meshtastic.protobuf import mesh_pb2
import test_supervisor as fixtures


class TracerouteTests(unittest.TestCase):
    setUp = fixtures.SupervisorTests.setUp
    tearDown = fixtures.SupervisorTests.tearDown

    def prepare(self):
        self.s._config_confirmed = True
        self.iface = self.s._iface
        self.context = self.s._config_fingerprint()
        self.iface.sendData = Mock()

    def run_trace(self):
        self.s._run_traceroute(self.iface, '!12345678', 1, 'trace-test', self.context)
        return [e for e in self.c.events if e['event'] == 'traceroute'][-1]

    def reply(self, port='TRACEROUTE_APP', payload=b'', sender=0x12345678):
        self.iface.sendData.side_effect = lambda *a, **kw: kw['onResponse'](
            {'from': sender, 'decoded': {'portnum': port, 'payload': payload}})

    def test_direct_route_is_a_success_only_for_destination_reply(self):
        self.prepare(); self.reply()
        result = self.run_trace()
        self.assertTrue(result['ok'])
        self.assertEqual(result['path'], ['You', '!12345678'])

    def test_route_hops_are_retained(self):
        self.prepare(); self.reply(payload=mesh_pb2.RouteDiscovery(route=[0x11223344]).SerializeToString())
        self.assertEqual(self.run_trace()['path'], ['You', '!11223344', '!12345678'])

    def test_routing_failure_is_not_a_successful_direct_route(self):
        self.prepare(); self.reply(port='ROUTING_APP', payload=mesh_pb2.Routing(error_reason=mesh_pb2.Routing.NO_ROUTE).SerializeToString())
        result = self.run_trace()
        self.assertFalse(result['ok']); self.assertIn('NO_ROUTE', result['text'])
        self.assertNotIn('path', result)

    def test_mesh_ack_does_not_replace_the_destination_route(self):
        self.prepare()
        def replies(*args, **kw):
            callback = kw['onResponse']
            callback({'from': 0x11111111, 'decoded': {'portnum': 'ROUTING_APP',
                      'payload': mesh_pb2.Routing(error_reason=0).SerializeToString()}})
            callback({'from': 0x12345678, 'decoded': {'portnum': 'TRACEROUTE_APP',
                      'payload': mesh_pb2.RouteDiscovery(route=[0x11223344]).SerializeToString()}})
        self.iface.sendData.side_effect = replies
        self.assertEqual(self.run_trace()['path'], ['You', '!11223344', '!12345678'])

    def test_timeout_reports_failure_without_a_fabricated_route(self):
        self.prepare()
        with patch('supervisor.time.monotonic', side_effect=[0, 20]):
            result = self.run_trace()
        self.assertFalse(result['ok']); self.assertIn('timed out', result['text'])

    def test_malformed_wrong_port_and_wrong_sender_are_failures(self):
        for reply in ({'payload': b'\xff'}, {'port': 'TEXT_MESSAGE_APP'}, {'sender': 0x87654321}):
            with self.subTest(reply=reply):
                self.prepare(); self.reply(**reply)
                self.assertFalse(self.run_trace()['ok'])

    def test_delayed_thread_cannot_transmit_after_radio_or_setup_change(self):
        for change in ('replace', 'receive-only', 'key', 'stop'):
            with self.subTest(change=change):
                self.prepare()
                if change == 'replace': self.s._iface = fixtures.Radio()
                elif change == 'receive-only': self.iface.localNode.localConfig.lora.tx_enabled = False
                elif change == 'key': self.iface.localNode.channels[1].settings.psk = b'Q' * 32
                else: self.s._stop.set()
                self.assertFalse(self.run_trace()['ok']); self.iface.sendData.assert_not_called()
                self.s._stop.clear(); self.s._iface = fixtures.Radio()

    def test_reply_after_setup_change_cannot_report_success(self):
        self.prepare()
        def changed(*args, **kw):
            self.s._iface = fixtures.Radio()
            kw['onResponse']({'from': 0x12345678, 'decoded': {'portnum': 'TRACEROUTE_APP', 'payload': b''}})
        self.iface.sendData.side_effect = changed
        self.assertFalse(self.run_trace()['ok'])

    def test_pending_trace_cancels_promptly_without_holding_lifecycle_lock(self):
        self.prepare()
        sent = threading.Event()
        self.iface.sendData.side_effect = lambda *a, **kw: sent.set()
        worker = threading.Thread(target=self.run_trace)
        worker.start()
        try:
            self.assertTrue(sent.wait(1))
            with self.s._life_lock: self.s._radio = 'off'
            worker.join(1)
            self.assertFalse(worker.is_alive())
        finally:
            self.s._stop.set(); worker.join(1)

    def test_thread_start_failure_releases_capacity(self):
        self.prepare()
        req = {'intent': 'traceroute', 'node': '!12345678', 'channel': 1,
               'channel_id': self.s._channel_identity(1), 'id': 'trace-test'}
        with patch.object(threading.Thread, 'start', side_effect=RuntimeError('synthetic start failure')):
            self.s._dispatch(self.c, req)
        self.assertTrue(self.s._trace_slots.acquire(blocking=False))
        self.assertTrue(self.s._trace_slots.acquire(blocking=False))
        self.assertFalse(self.s._trace_slots.acquire(blocking=False))
        self.s._trace_slots.release(); self.s._trace_slots.release()


if __name__ == '__main__':
    unittest.main()
