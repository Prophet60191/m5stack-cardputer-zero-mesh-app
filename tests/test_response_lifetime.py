# SPDX-License-Identifier: MIT
"""Pinned SDK callback expiry and decoder serialization; no TCP or RF."""
import sys
from pathlib import Path
import threading
import unittest
from unittest.mock import Mock, patch
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'supervisor'))
from meshtastic.mesh_interface import MeshInterface
from meshtastic.protobuf import mesh_pb2, portnums_pb2
from phone_gateway import observing_interface, RESPONSE_LIFETIME


class ResponseLifetimeTests(unittest.TestCase):
    def setUp(self):
        self.now = 100
        self.clock = patch('phone_gateway.time.monotonic', side_effect=lambda: self.now)
        self.clock.start(); self.addCleanup(self.clock.stop)
        def initialize(iface, **kwargs):
            MeshInterface.__init__(iface)
            iface.socket = None
            iface._rxThread = threading.current_thread()
        with patch('meshtastic.tcp_interface.TCPInterface.__init__', initialize):
            self.iface = observing_interface(lambda *args: None)
        self.iface.nodes = {}; self.iface.nodesByNum = {}
        self.iface.myInfo = mesh_pb2.MyNodeInfo(my_node_num=0x87654321)
        self.addCleanup(self.iface.close)

    def test_abandoned_callbacks_expire_without_firing_or_resending(self):
        callback = Mock()
        self.iface._addResponseHandler(42, callback)
        self.now += RESPONSE_LIFETIME - 1
        self.iface.expire_responses(); self.assertIn(42, self.iface.responseHandlers)
        self.now += 1
        self.iface.expire_responses(); self.assertNotIn(42, self.iface.responseHandlers)
        self.assertEqual(self.iface._response_deadlines, {}); callback.assert_not_called()

    def test_repeated_timeouts_do_not_accumulate_handlers(self):
        for ident in range(1000):
            self.now += RESPONSE_LIFETIME
            self.iface._addResponseHandler(ident, lambda _: None)
            self.assertEqual(len(self.iface.responseHandlers), 1)
            self.assertEqual(len(self.iface._response_deadlines), 1)

    def test_replacing_request_id_has_its_own_lifetime(self):
        self.iface._addResponseHandler(42, Mock())
        self.now += RESPONSE_LIFETIME - 1
        callback = Mock(); self.iface._addResponseHandler(42, callback)
        self.now += 2; self.iface.expire_responses()
        self.assertIs(self.iface.responseHandlers[42].callback, callback)

    def test_real_sdk_reply_calls_once_and_completed_deadline_is_removed(self):
        callback = Mock(); self.iface._addResponseHandler(42, callback)
        record = mesh_pb2.FromRadio()
        setattr(record.packet, 'from', 0x12345678); record.packet.to = 0x87654321
        record.packet.decoded.portnum = portnums_pb2.ROUTING_APP
        record.packet.decoded.request_id = 42
        record.packet.decoded.payload = mesh_pb2.Routing(error_reason=mesh_pb2.Routing.NO_ROUTE).SerializeToString()
        self.iface._handleFromRadio(record.SerializeToString())
        callback.assert_called_once()
        self.assertEqual(callback.call_args.args[0]['decoded']['routing']['errorReason'], 'NO_ROUTE')
        self.iface.expire_responses()
        self.assertEqual(self.iface._response_deadlines, {})

    def test_expiry_cannot_remove_a_handler_during_sdk_decode(self):
        self.iface._addResponseHandler(42, Mock())
        self.now += RESPONSE_LIFETIME
        entered, release, expired = threading.Event(), threading.Event(), threading.Event()
        def decode(*args):
            entered.set(); self.assertTrue(release.wait(2))
            handler = self.iface.responseHandlers.pop(42)
            handler.callback({})
        with patch.object(MeshInterface, '_handleFromRadio', decode):
            reader = threading.Thread(target=lambda: self.iface._handleFromRadio(b''))
            pruner = threading.Thread(target=lambda: (self.iface.expire_responses(), expired.set()))
            reader.start()
            try:
                self.assertTrue(entered.wait(1)); pruner.start()
                self.assertFalse(expired.wait(.05))
            finally:
                release.set(); reader.join(2)
                if pruner.ident is not None: pruner.join(2)
            self.assertFalse(reader.is_alive()); self.assertFalse(pruner.is_alive())
            self.assertTrue(expired.is_set()); self.assertEqual(self.iface._response_deadlines, {})


if __name__ == '__main__':
    unittest.main()
