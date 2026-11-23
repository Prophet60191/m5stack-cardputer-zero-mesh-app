# SPDX-License-Identifier: MIT
"""Read-only GPS cache refresh, real SDK decoding, lifecycle and failure checks."""
import copy
from pathlib import Path
import sys
import unittest
from types import SimpleNamespace
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'supervisor'))
from meshtastic.protobuf import mesh_pb2, channel_pb2
from meshtastic.mesh_interface import MeshInterface
from local_node_refresh import LocalNodeRefresh, NODE_ONLY_NONCE


class LocalNodeRefreshTests(unittest.TestCase):
    def setUp(self):
        self.now = 100
        self.refresh = LocalNodeRefresh(clock=lambda: self.now)
        self.sent = []
        self.iface = SimpleNamespace(_sendToRadio=lambda m:self.sent.append(copy.deepcopy(m)))

    def complete(self, iface=None, nonce=NODE_ONLY_NONCE):
        return self.refresh.observe(iface or self.iface, mesh_pb2.FromRadio(config_complete_id=nonce).SerializeToString())

    def test_single_bounded_request_then_interval(self):
        self.refresh.tick(self.iface)
        for _ in range(20):self.refresh.tick(self.iface)
        self.assertEqual(len(self.sent),1)
        self.assertEqual(self.sent[0].WhichOneof('payload_variant'),'want_config_id')
        self.assertEqual(self.sent[0].want_config_id,NODE_ONLY_NONCE)
        self.assertTrue(self.complete())
        self.now+=29;self.refresh.tick(self.iface);self.assertEqual(len(self.sent),1)
        self.now+=1;self.refresh.tick(self.iface);self.assertEqual(len(self.sent),2)

    def test_timeout_backs_off_without_fabricating_completion(self):
        self.refresh.tick(self.iface);self.now+=10;self.refresh.tick(self.iface)
        self.assertEqual(self.refresh.status,'timeout');self.assertFalse(self.complete())
        self.now+=29;self.refresh.tick(self.iface);self.assertEqual(len(self.sent),1)
        self.now+=1;self.refresh.tick(self.iface);self.assertEqual(len(self.sent),2)

    def test_stop_replacement_and_wrong_completion_cannot_complete_current_refresh(self):
        self.refresh.tick(self.iface);self.assertFalse(self.complete(nonce=123))
        old=self.iface;self.refresh.tick(None);self.assertEqual(self.refresh.status,'off')
        self.assertFalse(self.complete(old))
        self.iface=SimpleNamespace(_sendToRadio=lambda m:self.sent.append(copy.deepcopy(m)))
        self.refresh.tick(self.iface);self.assertFalse(self.complete(old));self.assertTrue(self.complete())

    def test_send_failure_is_isolated_and_rate_limited(self):
        def fail(_):raise OSError('synthetic transport error')
        self.iface._sendToRadio=fail
        self.refresh.tick(self.iface);self.assertEqual(self.refresh.status,'unavailable')
        self.iface._sendToRadio=lambda m:self.sent.append(m)
        self.refresh.tick(self.iface);self.assertFalse(self.sent)
        self.now+=30;self.refresh.tick(self.iface);self.assertEqual(len(self.sent),1)

    def test_completion_can_arrive_synchronously_without_lock_deadlock(self):
        self.iface._sendToRadio=lambda _:self.complete()
        self.refresh.tick(self.iface);self.assertEqual(self.refresh.status,'current')

    def test_real_sdk_updates_private_gps_without_resetting_channels_or_sending_rf(self):
        class SDK(MeshInterface):
            def _sendToRadio(inner, message):self.sent.append(copy.deepcopy(message))
        iface=SDK();iface.nodes={};iface.nodesByNum={};iface.configId=123
        iface.myInfo=mesh_pb2.MyNodeInfo(my_node_num=0x12345678);iface.localNode.nodeNum=0x12345678
        iface.isConnected.set()
        channel=channel_pb2.Channel(index=0,role=channel_pb2.Channel.PRIMARY)
        channel.settings.name='Synthetic';channel.settings.psk=b'R'*32
        iface._localChannels=[channel];iface.localNode.setChannels(iface._localChannels)
        iface.localNode.localConfig.lora.tx_enabled=False
        iface.localNode.localConfig.position.gps_mode=1
        iface.nodesByNum[0x12345678]={'num':0x12345678,'position':{'time':1}}
        iface.nodesByNum[0x87654321]={'num':0x87654321,'user':{'id':'!87654321'}}
        original_channels=[c.SerializeToString() for c in iface.localNode.channels]
        original_local_count=len(iface._localChannels)
        try:
            self.refresh.tick(iface)
            record=mesh_pb2.FromRadio();record.node_info.num=0x12345678
            record.node_info.user.id='!12345678'
            record.node_info.position.latitude_i=12500000;record.node_info.position.longitude_i=25000000
            record.node_info.position.time=1000
            iface._handleFromRadio(record.SerializeToString());self.refresh.observe(iface,record.SerializeToString())
            done=mesh_pb2.FromRadio(config_complete_id=NODE_ONLY_NONCE).SerializeToString()
            iface._handleFromRadio(done);self.assertTrue(self.refresh.observe(iface,done))
            self.assertEqual(iface.getMyNodeInfo()['position']['latitude'],1.25)
            self.assertEqual(iface.getMyNodeInfo()['position']['longitude'],2.5)
            self.assertEqual([c.SerializeToString() for c in iface.localNode.channels],original_channels)
            self.assertEqual(len(iface._localChannels),original_local_count);self.assertEqual(iface.configId,123)
            self.assertIn(0x87654321,iface.nodesByNum)
            self.assertFalse(iface.localNode.localConfig.lora.tx_enabled)
            self.assertEqual(channel.settings.module_settings.position_precision,0)
            self.assertTrue(all(m.WhichOneof('payload_variant')=='want_config_id' for m in self.sent))
            # A later time-only/no-fix record must remove cached coordinates,
            # rather than make the old location fresh using the new time.
            self.now+=30;self.refresh.tick(iface)
            record.node_info.position.Clear();record.node_info.position.time=2000
            iface._handleFromRadio(record.SerializeToString());self.refresh.observe(iface,record.SerializeToString())
            self.assertIsNone(iface.getMyNodeInfo()['position'].get('latitude'))
            self.assertIsNone(iface.getMyNodeInfo()['position'].get('longitude'))
        finally:iface.close()



import test_supervisor as fixtures

class SupervisorRefreshTests(unittest.TestCase):
    def setUp(self):
        fixtures.SupervisorTests.setUp(self)
        self.s._stop.set();self.s._worker.join(1)
        self.requests=[]
        self.s._iface._sendToRadio=lambda m:self.requests.append(copy.deepcopy(m))
    tearDown=fixtures.SupervisorTests.tearDown

    def test_refresh_requires_enabled_gps_confirmed_online_owner_but_not_rf_tx(self):
        position=self.s._iface.localNode.localConfig.position
        for ch in self.s._iface.localNode.channels:ch.settings.module_settings.position_precision=0
        position.gps_mode=position.GpsMode.DISABLED;self.s._config_confirmed=True
        self.s._refresh_local_position();self.assertFalse(self.requests)
        position.gps_mode=position.GpsMode.ENABLED;self.s._config_confirmed=False
        self.s._refresh_local_position();self.assertFalse(self.requests)
        self.s._config_confirmed=True;self.s._radio='off'
        self.s._refresh_local_position();self.assertFalse(self.requests)
        self.s._radio='online';self.s._iface.localNode.localConfig.lora.tx_enabled=False
        self.s._refresh_local_position();self.assertEqual(len(self.requests),1)
        self.assertEqual(self.requests[0].want_config_id,NODE_ONLY_NONCE)
        self.assertFalse(self.s._iface.sent)
        self.assertFalse(self.s._position_sharing_on())

    def test_completed_snapshot_notifies_ui_after_existing_decoder_callback(self):
        self.s._iface.localNode.localConfig.position.gps_mode=1;self.s._config_confirmed=True
        self.s._refresh_local_position();self.s._nodes_dirty.clear()
        self.s._radio_frame(self.s._iface,mesh_pb2.FromRadio(config_complete_id=123).SerializeToString())
        self.assertFalse(self.s._nodes_dirty.is_set())
        self.s._radio_frame(self.s._iface,mesh_pb2.FromRadio(config_complete_id=NODE_ONLY_NONCE).SerializeToString())
        self.assertTrue(self.s._nodes_dirty.is_set())
        self.assertEqual(self.s._local_nodes.status,'current')

    def test_teardown_invalidates_late_completion(self):
        self.s._iface.localNode.localConfig.position.gps_mode=1;self.s._config_confirmed=True
        self.s._refresh_local_position();old=self.s._iface
        self.s._teardown();self.s._nodes_dirty.clear()
        self.s._radio_frame(old,mesh_pb2.FromRadio(config_complete_id=NODE_ONLY_NONCE).SerializeToString())
        self.assertEqual(self.s._local_nodes.status,'off')
        self.assertFalse(self.s._nodes_dirty.is_set())

if __name__=='__main__':unittest.main()
