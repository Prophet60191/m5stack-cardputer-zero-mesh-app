# SPDX-License-Identifier: MIT
"""Synthetic state, protocol, setup, profile and health regression coverage; no RF."""
import base64
import copy
import json
import os
from pathlib import Path
import sys
import tempfile
import time
import unittest
from unittest.mock import patch
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'supervisor'))
from radio_settings import RadioSettings, SCHEMA, atomic_write
from state_store import StateStore
import firewall
import supervisor
import test_supervisor as fixtures
from meshtastic.protobuf import channel_pb2, localonly_pb2


class PersistenceTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(); self.path = Path(self.tmp.name)
        self.store = StateStore(self.path)
    def tearDown(self): self.store.close(); self.tmp.cleanup()
    def test_order_updates_deletion_retention_and_reopen(self):
        messages = [{'from':'self','id':str(i),'status':'sending'} for i in range(2005)]
        self.store.replace_messages(messages)
        messages = messages[-2000:]; messages[0]['status'] = 'failed'
        self.store.replace_messages(messages)
        self.store.close(); self.store = StateStore(self.path)
        self.assertEqual(self.store.load_messages(), messages)
        messages = messages[-50:]; self.store.replace_messages(messages)
        self.store.close(); self.store = StateStore(self.path)
        self.assertEqual(self.store.load_messages(), messages)
        self.assertEqual((self.path/'conversations.sqlite3').stat().st_mode & 0o777, 0o600)
    def test_drafts_pins_and_no_history(self):
        self.store.set_thread('ch:synthetic', {'draft':'unsent','pinned':True})
        self.store.replace_messages([{'from':'self','id':'1','text':'synthetic'}])
        self.store.set_preference('history.enabled', False)
        self.store.replace_messages([{'from':'self','id':'2'}])
        self.store.set_thread('dm:!12345678@group', {'draft':'RAM-only','pinned':False})
        self.store.close(); self.store = StateStore(self.path)
        self.assertEqual(self.store.load_messages(), [])
        self.assertEqual(self.store.load_threads(), {})
        self.assertEqual(self.store._db.execute('SELECT COUNT(*) FROM threads').fetchone()[0], 0)
        for key, val in [('bad', {}), ('ch:x', {'draft':'x'*201}), ('dm:x', {'pinned':1})]:
            with self.assertRaises(ValueError): self.store.set_thread(key,val)
    def test_database_symlink_rejected(self):
        self.store.close(); (self.path/'conversations.sqlite3').unlink()
        (self.path/'other').touch(); (self.path/'conversations.sqlite3').symlink_to(self.path/'other')
        with self.assertRaises(PermissionError): StateStore(self.path)
        (self.path/'conversations.sqlite3').unlink(); self.store = StateStore(self.path)
    def test_restart_scrubs_metadata_left_by_the_old_opt_out_policy(self):
        self.store.set_preference('history.enabled', False)
        self.store._db.execute('INSERT INTO threads VALUES (?,?)', ('dm:!12345678', '{"draft":"","pinned":true}'))
        self.store._db.commit(); self.store.close(); self.store = StateStore(self.path)
        self.assertEqual(self.store.load_threads(), {})
        self.assertEqual(self.store._db.execute('SELECT COUNT(*) FROM threads').fetchone()[0], 0)


class RadioSettingsTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(); self.path = Path(self.tmp.name); self.settings = RadioSettings(self.path)
    def tearDown(self): self.tmp.cleanup()
    def test_first_run_rx_only_private_and_public(self):
        self.settings.initialize('US', True, 'Synthetic')
        cfg, channels = self.settings.load()
        self.assertEqual((cfg.version, channels.version), (SCHEMA,SCHEMA))
        self.assertEqual(len(channels.channels),8); self.assertEqual(len(channels.channels[0].settings.psk),32)
        self.assertFalse(cfg.lora.tx_enabled)
        self.assertTrue(cfg.security.serial_enabled)
        self.assertFalse(any(ch.settings.module_settings.position_precision or ch.settings.uplink_enabled or ch.settings.downlink_enabled for ch in channels.channels))
        with self.assertRaises(ValueError): self.settings.initialize('US')
        modules = localonly_pb2.LocalModuleConfig.FromString((self.settings.prefs/'module.proto').read_bytes())
        self.assertFalse(modules.mqtt.enabled)
        with tempfile.TemporaryDirectory() as other:
            public = RadioSettings(Path(other)); public.initialize('EU_868'); cfg,chs=public.load()
            self.assertFalse(cfg.lora.tx_enabled); self.assertEqual(chs.channels[0].settings.psk,b'\x01')
    def test_invalid_setup_never_writes_preferences(self):
        for region,name in [('UNSET','Name'),('US',''),('US','x'*12),('US','line\nbreak')]:
            with self.assertRaises(ValueError): self.settings.initialize(region,True,name)
        self.assertFalse(self.settings.prefs.exists())
    def test_identity_survives_slot_role_and_restart_not_key_name(self):
        self.settings.initialize('US',True,'Synthetic'); cfg,chs=self.settings.load(); ch=chs.channels[0]
        ident=self.settings.identity(ch,cfg.lora)
        moved=copy.deepcopy(ch); moved.index=5; moved.role=2
        self.assertEqual(self.settings.identity(moved,cfg.lora),ident)
        self.assertEqual(RadioSettings(self.path).identity(moved,cfg.lora),ident)
        moved.settings.id=42; self.assertNotEqual(self.settings.identity(moved,cfg.lora),ident)
        moved.settings.id=0
        moved.settings.name='Different'; self.assertNotEqual(self.settings.identity(moved,cfg.lora),ident)
        moved.CopyFrom(ch); moved.settings.psk=b'S'*32; self.assertNotEqual(self.settings.identity(moved,cfg.lora),ident)
        other=copy.deepcopy(ch); other.settings.psk=b'\x01'; ch.settings.psk=bytes.fromhex('d4f1bb3a20290759f0bcffabcf4e6901')
        self.assertEqual(self.settings.identity(other,cfg.lora),self.settings.identity(ch,cfg.lora))
    def test_transaction_recovery_after_partial_write(self):
        self.settings.initialize('US',True,'Synthetic'); cfg,chs=self.settings.load(); cfg.lora.hop_limit=5
        original=__import__('radio_settings').atomic_write
        def fail(path,payload):
            if path.name=='channels.proto': raise OSError('simulated power loss')
            original(path,payload)
        with patch('radio_settings.atomic_write', side_effect=fail), self.assertRaises(OSError): self.settings.apply(cfg,chs)
        self.assertTrue((self.path/'settings-change.pending').exists())
        recovered=RadioSettings(self.path); self.assertEqual(recovered.load()[0].lora.hop_limit,5)
        self.assertFalse((self.path/'settings-change.pending').exists())
    def test_profiles_no_path_traversal_and_private_files(self):
        self.settings.initialize('US',True,'Synthetic'); cfg,chs=self.settings.load()
        ident=self.settings.save_profile('Field profile',cfg,chs)
        self.assertEqual((self.settings.profiles/(ident+'.json')).stat().st_mode & 0o777,0o600)
        self.assertEqual(self.settings.list_profiles()[0]['name'],'Field profile')
        self.assertNotIn('config',self.settings.list_profiles()[0])
        for bad in ['../config','/tmp/synthetic','A'*32,None]:
            with self.assertRaises(ValueError): self.settings.profile(bad)
            with self.assertRaises(ValueError): self.settings.delete_profile(bad)
        self.settings.delete_profile(ident); self.assertFalse(self.settings.list_profiles())


class HealthTests(unittest.TestCase):
    def test_verdict_owner_freshness_boot_mode_and_type(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/'status.json'
            def write(**change):
                d={'ok':True,'checked':100,'boot':'synthetic'}; d.update(change); p.write_text(json.dumps(d)); p.chmod(0o644)
            def check(): return firewall.health_status(p,uid=os.getuid(),now=105,boot='synthetic')
            write(); self.assertTrue(check()['ok'])
            for changes in [{'checked':97},{'checked':106},{'checked':True},{'boot':'different'},{'ok':False},{'ok':1},{'checked':float('nan')}]:
                write(**changes)
                with self.assertRaises(RuntimeError): check()
            write(); p.chmod(0o666)
            with self.assertRaises(RuntimeError): check()
            p.chmod(0o644)
            with self.assertRaises(RuntimeError): firewall.health_status(p,uid=os.getuid()+1,now=105,boot='synthetic')
    def test_application_health_does_not_spawn_privileged_process(self):
        with patch.object(firewall,'health_status',return_value={'ok':True}), patch('subprocess.run') as run:
            self.assertTrue(supervisor.Supervisor._check_firewall(None)['ok']); run.assert_not_called()


class ApplicationTests(unittest.TestCase):
    # Share fixtures, not inherited tests, so the suite is not accidentally duplicated.
    setUp=fixtures.SupervisorTests.setUp; tearDown=fixtures.SupervisorTests.tearDown
    def test_stale_identity_rejected_before_rf_even_when_slot_reused(self):
        identity=self.s._channel_identity(1)
        self.s._iface.localNode.channels[1].settings.psk=b'S'*32
        self.s._dispatch(self.c,{'intent':'message.send','channel':1,'channel_id':identity,'text':'secret','cid':'synthetic'})
        self.assertFalse(self.s._iface.sent); self.assertFalse(self.s._messages)
        self.assertIn('channel changed',self.c.events[-1]['text'])
    def test_receive_only_blocks_all_rf_actions(self):
        self.s._iface.localNode.localConfig.lora.tx_enabled=False
        for intent, extra in [('message.send',{'text':'synthetic','cid':'x'}),('position.send',{}),('position.request',{'node':'!12345678'}),('traceroute',{'node':'!12345678'})]:
            self.s._dispatch(self.c,dict(intent=intent,channel=1,channel_id=self.s._channel_identity(1),**extra))
            self.assertEqual(self.c.events[-1]['event'],'error'); self.assertIn('receive-only',self.c.events[-1]['text'])
        self.assertFalse(self.s._iface.sent)
    def test_gps_freshness_and_invalid_coordinates(self):
        for pos in [{}, {'latitude':1,'longitude':2}, {'latitude':1,'longitude':2,'time':time.time()-121}, {'latitude':91,'longitude':2,'time':time.time()}, {'latitude':float('nan'),'longitude':2,'time':time.time()}, {'latitude':1,'longitude':2,'time':time.time()+60}]:
            self.s._iface.getMyNodeInfo=lambda:{'position':pos}
            try:
                result=self.s._my_position(); self.assertEqual(result,(None,None))
            except ValueError: pass
        self.s._iface.getMyNodeInfo=lambda:{'position':{'latitude':1.25,'longitude':2.5,'time':time.time()-10}}
        self.assertEqual(self.s._my_position(),(1.25,2.5))
    def test_restart_marks_pending_failed_and_does_not_replay(self):
        self.s._record({'from':'self','id':'pending','to':'broadcast','channel':1,'channel_id':'synthetic','status':'sending'})
        self.s._stop.set(); self.s._worker.join(1); self.s._store.close(); self.s._state_lease.close()
        with patch.object(Path,'home',return_value=Path(self.tmp.name)): other=supervisor.Supervisor()
        try:
            self.assertEqual(other._messages[0]['status'],'failed'); self.assertIn('not replayed',other._messages[0]['reason'])
            self.assertIsNone(other._iface); self.assertEqual(other._radio,'off')
        finally: other._stop.set(); other._worker.join(1); other._store.close(); other._state_lease.close()
    def test_reenabling_history_saves_ram_drafts_without_another_edit(self):
        state = {'draft': 'Synthetic unsent draft', 'pinned': True}
        self.s._threads['ch:synthetic'] = state
        self.s._dispatch(self.c, {'intent': 'history.set', 'enabled': False})
        self.s._store.set_thread('ch:synthetic', state)
        self.assertEqual(self.s._store.load_threads(), {})
        with patch.object(self.s, '_broadcast') as broadcast:
            self.s._dispatch(self.c, {'intent': 'history.set', 'enabled': True})
        self.assertEqual(broadcast.call_args[0][0]['event'], 'threads')
        reopened = StateStore(self.s._state_dir)
        try: self.assertEqual(reopened.load_threads()['ch:synthetic'], state)
        finally: reopened.close()
    def test_channel_manager_rotate_protect_primary_and_stale_request(self):
        ident=self.s._channel_identity(1)
        self.s._channel_manage(dict(action='rotate',channel=1,channel_id=ident,confirmed=True))
        self.assertNotEqual(ident,self.s._channel_identity(1))
        with self.assertRaises(ValueError): self.s._channel_manage(dict(action='disable',channel=1,channel_id=ident,confirmed=True))
        with self.assertRaises(ValueError): self.s._channel_manage(dict(action='remove',channel=0,channel_id=self.s._channel_identity(0),confirmed=True))
        self.assertFalse(self.s._iface.localNode.channels[1].settings.module_settings.position_precision)
    def test_profile_application_offline_rx_and_no_location_mqtt(self):
        self.s._iface=None; self.s._radio='off'; self.s._settings.initialize('US',True,'Synthetic')
        cfg,chs=self.s._settings.load(); cfg.lora.tx_enabled=True; chs.channels[0].settings.module_settings.position_precision=16
        ident=self.s._settings.save_profile('Synthetic',cfg,chs)
        self.s._profile_apply({'profile':ident,'confirmed':True}); cfg,chs=self.s._settings.load()
        self.assertFalse(cfg.lora.tx_enabled); self.assertFalse(chs.channels[0].settings.module_settings.position_precision)
        self.assertEqual(self.s._radio,'off')
    def test_diagnostics_whitelist_no_content_identity_key_or_coordinates(self):
        self.s._iface.getMyNodeInfo=lambda:{'user':{'id':'!12345678','longName':'Sensitive Owner'},'position':{'latitude':1.23456,'longitude':2.34567,'time':time.time()}}
        self.s._record({'id':'x','from':'!12345678','text':'Sensitive chat','channel':1})
        with patch.object(self.s,'_daemon_version',return_value='synthetic'), patch.object(self.s,'_check_firewall',return_value={'ok':True}): data=self.s._export_diagnostics()
        text=json.dumps(data)
        for secret in ['!12345678','Sensitive Owner','Sensitive chat','1.23456','2.34567','psk','Group 1',str(self.s._state_dir)]: self.assertNotIn(secret,text)
        self.assertEqual((self.s._state_dir/'diagnostics-export.json').stat().st_mode&0o777,0o600)
    def test_stale_connection_callback_cannot_stop_new_radio(self):
        with self.s._life_lock:
            old=self.s._iface; self.s._on_connection_lost(old)
            self.s._iface=fixtures.Radio()
        # Drain the closure explicitly under lifecycle serialization.
        # Worker timing is held to make interface replacement deterministic.
        with self.s._life_lock:
            pass
        time.sleep(.05)
        self.assertEqual(self.s._radio,'online')
    def test_unconfirmed_transmit_change_stops_radio(self):
        with patch.object(self.s,'_verify_config',side_effect=TimeoutError('synthetic timeout')):
            with self.assertRaises(TimeoutError): self.s._set_transmit({'enabled':False})
        self.assertIsNone(self.s._iface); self.assertEqual(self.s._radio,'error'); self.assertFalse(self.s._config_confirmed)
    def test_recipient_ack_requires_matching_source_and_failure_reason_is_preserved(self):
        request={'intent':'message.send','channel':1,'channel_id':self.s._channel_identity(1),'text':'Synthetic','dest':'!12345678','cid':'direct'}
        self.s._dispatch(self.c,request)
        self.s._iface.callbacks[-1]({'from':0x11111111,'decoded':{'routing':{'errorReason':'NONE'}}})
        self.assertEqual(self.s._messages[-1]['status'],'mesh'); self.assertIn('not confirmed',self.s._messages[-1]['reason'])
        self.s._dispatch(self.c,request)
        self.s._iface.callbacks[-1]({'from':0x12345678,'decoded':{'routing':{'errorReason':'NONE'}}})
        self.assertEqual(self.s._messages[-1]['status'],'delivered')
        self.s._dispatch(self.c,request)
        self.s._iface.callbacks[-1]({'decoded':{'routing':{'errorReason':9}}})
        self.assertEqual(self.s._messages[-1]['status'],'failed'); self.assertIn('duty-cycle',self.s._messages[-1]['reason'])
    def test_escaped_control_history_fits_snapshot_byte_bound(self):
        self.s._messages=[{'id':str(i),'from':'synthetic','to':'broadcast','channel':1,'text':'\x01'*512} for i in range(2000)]
        self.s._trim_history()
        self.assertLess(len(self.s._messages),2000)
        self.assertLess(len(json.dumps(self.s._messages_payload()).encode()),supervisor.MAX_EVENT_BYTES)

    def test_second_supervisor_cannot_mutate_live_state(self):
        self.s._record({'from':'self','id':'pending','status':'sending'})
        with patch.object(Path,'home',return_value=Path(self.tmp.name)), self.assertRaises(RuntimeError): supervisor.Supervisor()
        self.assertEqual(self.s._store.load_messages()[0]['status'],'sending')

    def test_firmware_empty_settings_are_free_but_disabled_keys_are_preserved(self):
        for ch in self.s._iface.localNode.channels[3:]:ch.settings.SetInParent()
        retained=self.s._iface.localNode.channels[3];retained.settings.name='Retained';retained.settings.psk=b'R'*32
        before=retained.SerializeToString()
        self.s._channel_manage({'action':'create','kind':'private','name':'NewGroup','confirmed':True})
        self.assertEqual(retained.SerializeToString(),before)
        self.assertEqual(self.s._iface.localNode.channels[4].role,2)
        self.assertFalse(self.s._channel_configured(self.s._iface.localNode.channels[5]))

    def test_remote_routing_ack_is_rx_but_local_admin_or_cached_node_update_is_not(self):
        self.s._iface.getMyNodeInfo=lambda:{'num':0x11111111}
        self.s._on_receive_packet({'from':0x11111111},self.s._iface);self.assertEqual(self.s._last_rx,0)
        self.s._on_node_update(self.s._iface,node={});self.assertEqual(self.s._last_rx,0)
        self.s._on_receive_packet({'from':0x12345678,'decoded':{'routing':{'errorReason':'NONE'}}},self.s._iface)
        self.assertGreater(self.s._last_rx,0);self.assertTrue(self.s._nodes_dirty.is_set())

    def test_tuning_readback_accepts_derived_fields_but_not_transmit_mode_changes(self):
        interface=self.s._iface
        def restart(*,bad_tx=False):
            cfg,_=self.s._settings.load();interface.localNode.localConfig.CopyFrom(cfg)
            interface.localNode.localConfig.lora.spread_factor=12
            if bad_tx:interface.localNode.localConfig.lora.tx_enabled=False
            self.s._iface=interface;self.s._radio='online'
        with patch.object(self.s,'radio_start',side_effect=restart):self.s._restart_with_config('lora.modem_preset','MEDIUM_FAST')
        self.assertEqual(self.s._iface.localNode.localConfig.lora.modem_preset,4);self.assertEqual(self.s._radio,'online')
        with patch.object(self.s,'radio_start',side_effect=lambda:restart(bad_tx=True)),self.assertRaises(ValueError):self.s._restart_with_config('lora.channel_num',20)
        self.assertEqual(self.s._radio,'error');self.assertIsNone(self.s._iface)

    def test_actual_pubsub_parent_and_telemetry_contracts_are_compatible(self):
        from pubsub.core import Publisher
        publisher=Publisher()
        publisher.subscribe(self.s._on_receive_text,'meshtastic.receive.text')
        publisher.subscribe(self.s._on_receive_packet,'meshtastic.receive')
        publisher.subscribe(self.s._on_receive_position,'meshtastic.receive.position')
        publisher.subscribe(self.s._on_node_update,'meshtastic.node.updated')
        publisher.subscribe(self.s._on_telemetry,'meshtastic.receive.telemetry')
        self.s._iface.getMyNodeInfo=lambda:{'num':0x11111111}
        publisher.sendMessage('meshtastic.receive.telemetry',packet={'from':0x12345678},interface=self.s._iface)
        self.assertTrue(self.s._nodes_dirty.is_set());self.assertGreater(self.s._last_rx,0)
        publisher.sendMessage('meshtastic.node.updated',node={},interface=self.s._iface)


class CameraTests(unittest.TestCase):
    def test_only_bounded_utf8_meshtastic_codes_are_accepted(self):
        import qr_scan
        from types import SimpleNamespace
        values=[b'bad', b'\xff', b'x'*5000, b'https://meshtastic.org/e/#synthetic']
        self.assertEqual(qr_scan.share_url([SimpleNamespace(data=v) for v in values]),values[-1].decode())
    def test_csi_capture_uses_fixed_program_and_dimensions_without_persisting_frames(self):
        import qr_scan
        from types import SimpleNamespace
        class Frame:
            size=(960,720)
            def __enter__(self):return self
            def __exit__(self,*args):pass
        decoder=SimpleNamespace(decode=lambda frame:[SimpleNamespace(data=b'https://meshtastic.org/e/#synthetic')])
        modules={'pyzbar':SimpleNamespace(), 'pyzbar.pyzbar':decoder, 'PIL':SimpleNamespace(Image=SimpleNamespace(open=lambda data:Frame()))}
        result=SimpleNamespace(returncode=0,stdout=b'synthetic frame')
        with patch.dict(sys.modules,modules), patch.object(qr_scan,'capture_program',return_value='/usr/bin/rpicam-still'), patch.object(qr_scan.subprocess,'run',return_value=result) as capture:
            self.assertEqual(qr_scan.scan(),'https://meshtastic.org/e/#synthetic')
            argv=capture.call_args.args[0]
            self.assertEqual(argv[0],'/usr/bin/rpicam-still'); self.assertEqual(argv[-1],'-')
            self.assertLessEqual(capture.call_args.kwargs['timeout'],8)
            self.assertEqual(argv[argv.index('--width')+1],'960')


class PowerRecoveryTests(unittest.TestCase):
    def test_vendor_255_is_enabled_and_recovery_validates_both_fields_before_writes(self):
        import cap_lora,radio_guardian
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/'power.json'
            path.write_text(json.dumps({'power':'255','mux':'0'}))
            with patch.object(cap_lora,'_write_text') as write:
                radio_guardian.restore(path)
                self.assertEqual(write.call_args_list[0].args,(cap_lora.MUX_LED,'0'))
                self.assertEqual(write.call_args_list[1].args,(cap_lora.POWER_LED,'1'))
            self.assertFalse(path.exists())
            for value in ['256','-1','1.0','yes','../path',True,None]:
                path.write_text(json.dumps({'power':'1','mux':value}))
                with patch.object(cap_lora,'_write_text') as write, self.assertRaises(ValueError): radio_guardian.restore(path)
                write.assert_not_called(); self.assertTrue(path.exists())
    def test_gpio_preflight_reads_claims_and_refuses_nfc_conflict_without_switch_writes(self):
        import cap_lora,struct
        def info(fd,request,data,mutate):
            self.assertEqual(request,0xC048B402)
            offset=struct.unpack_from('I',data)[0]
            if offset==22:
                struct.pack_into('I',data,4,3);data[40:47]=b'backing'
        with patch.object(cap_lora.os,'open',return_value=123), patch.object(cap_lora.os,'close') as close, patch.object(cap_lora.fcntl,'ioctl',side_effect=info):
            self.assertEqual(cap_lora.gpio_claims(),{22:'backing'});close.assert_called_once_with(123)
        with patch.object(cap_lora.os.path,'exists',return_value=True), patch.object(cap_lora.os,'access',return_value=True), patch.object(cap_lora,'gpio_claims',return_value={22:'backing'}), patch.object(cap_lora,'_write_text') as write:
            with self.assertRaisesRegex(cap_lora.CapError,'NFC SPI-CS2'):cap_lora.preflight()
            write.assert_not_called()
