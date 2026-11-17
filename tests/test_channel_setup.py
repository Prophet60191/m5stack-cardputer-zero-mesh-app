# SPDX-License-Identifier: MIT
"""Synthetic protobuf/storage/radio-owner tests. No hardware, network or RF."""
import base64
import copy
import json
from pathlib import Path
import tempfile
import threading
import time
import unittest
from types import SimpleNamespace
from unittest.mock import patch
import test_supervisor as fixtures
from meshtastic.protobuf import apponly_pb2, channel_pb2, localonly_pb2
from radio_settings import atomic_write


class ChannelSetupTests(unittest.TestCase):
    setUp = fixtures.SupervisorTests.setUp
    tearDown = fixtures.SupervisorTests.tearDown

    def prepare(self, online=False, tx=False):
        self.s._iface = None
        self.s._radio = 'off'
        self.s._settings.initialize('US', True, 'Original')
        cfg, chs = self.s._settings.load()
        cfg.lora.channel_num = 20
        cfg.lora.tx_enabled = tx
        self.s._settings.apply(cfg, chs)
        if online: self.connect_fake()

    def connect_fake(self):
        cfg, chs = self.s._settings.load()
        radio = fixtures.Radio()
        radio.localNode.localConfig.CopyFrom(cfg)
        radio.localNode.channels = list(chs.channels)
        radio.localNode.moduleConfig.ParseFromString((self.s._settings.prefs/'module.proto').read_bytes())
        self.s._iface = radio
        self.s._radio = 'online'
        self.s._config_confirmed = True

    def request(self, **changes):
        context = self.s._setup_context()
        result = dict(id='preview', source='private', name='New group', tuning=context['tuning'],
                      base=context['base'], tx=context['tx'], position=False, mqtt=False,
                      profile_only=False, profile_name='')
        result.update(changes)
        return result

    def preview(self, **changes):
        req=self.request(**changes);self.s._setup_begin(self.c,req['id'])
        return self.s._setup_preview(self.c,req)

    def commit(self, preview, client=None, **extra):
        req=dict(token=preview['token'], confirmed=True, tuning_confirmed=True,
                 antenna_confirmed=True, public_position_confirmed=True, mqtt_confirmed=True)
        req.update(extra)
        return self.s._setup_commit(client or self.c, req)

    def disk(self):
        return {str(p.relative_to(self.s._state_dir)): p.read_bytes() for p in self.s._state_dir.rglob('*')
                if p.is_file() and p.name not in ('state.lock','conversations.sqlite3','conversations.sqlite3-wal','conversations.sqlite3-shm')}

    def test_prepare_cancel_write_nothing_and_keep_keys_off_socket(self):
        self.prepare(); before=self.disk()
        preview=self.preview(profile_name='Field')
        self.assertEqual(self.disk(), before)
        text=json.dumps(preview)
        for word in ['psk','secret','url']: self.assertNotIn(word,text.lower())
        self.s._setup_cancel(self.c)
        with self.assertRaises(ValueError): self.commit(preview)
        self.assertEqual(self.disk(),before)

    def test_offline_full_save_verified_and_named_profile(self):
        self.prepare(); cfg,chs=self.s._settings.load(); original=chs.channels[0].SerializeToString()
        tuning=self.s._setup_context()['tuning'];tuning.update(preset='MEDIUM_FAST',slot=12,hop=4)
        preview=self.preview(tuning=tuning,profile_name='Field')
        result=self.commit(preview)
        self.assertEqual(result['verified'],'stored');self.assertEqual(self.s._radio,'off')
        cfg,chs=self.s._settings.load()
        self.assertEqual((cfg.lora.modem_preset,cfg.lora.channel_num,cfg.lora.hop_limit),(cfg.lora.ModemPreset.Value('MEDIUM_FAST'),12,4))
        self.assertEqual(chs.channels[0].SerializeToString(),original)
        self.assertEqual(chs.channels[1].role,2);self.assertEqual(len(chs.channels[1].settings.psk),32)
        self.assertFalse(chs.channels[1].settings.module_settings.position_precision)
        self.assertFalse(chs.channels[1].settings.uplink_enabled)
        self.assertEqual(len(self.s._settings.list_profiles()),1)
        self.assertEqual((self.s._settings.profiles/(result['profile']+'.json')).stat().st_mode & 0o777,0o600)
        with self.assertRaises(ValueError): self.commit(preview)  # Cannot create a second channel on repeat Return.

    def test_separate_profile_never_stops_or_changes_active_radio(self):
        self.prepare(online=True,tx=True);before=self.disk();iface=self.s._iface
        preview=self.preview(profile_only=True,profile_name='Other mesh',tx=False)
        with patch.object(self.s,'radio_stop',side_effect=AssertionError('radio must stay untouched')):
            result=self.commit(preview)
        self.assertIs(self.s._iface,iface);self.assertEqual(self.s._radio,'online')
        self.assertTrue(result['profile_only'])
        self.assertEqual({k:v for k,v in self.disk().items() if not k.startswith('profiles/')},before)
        _,cfg,chs=self.s._settings.profile(result['profile'])
        self.assertFalse(cfg.lora.tx_enabled)
        self.assertFalse(any(ch.settings.module_settings.position_precision or ch.settings.uplink_enabled or ch.settings.downlink_enabled for ch in chs.channels))

    def test_stale_context_review_and_replaced_preview_are_rejected(self):
        self.prepare();req=self.request();self.s._setup_begin(self.c,req['id']);cfg,chs=self.s._settings.load();cfg.lora.hop_limit=4;self.s._settings.apply(cfg,chs)
        with self.assertRaisesRegex(ValueError,'reload'): self.s._setup_preview(self.c,req)
        first=self.preview();second=self.preview(name='Other');before=self.disk()
        with self.assertRaises(ValueError):self.commit(first)
        cfg,chs=self.s._settings.load();cfg.lora.channel_num=21;self.s._settings.apply(cfg,chs)
        with self.assertRaisesRegex(ValueError,'changed'):self.commit(second)

    def test_wrong_client_expiry_confirmations_and_strict_fields(self):
        self.prepare();p=self.preview();before=self.disk()
        with self.assertRaises(ValueError):self.commit(p,client=fixtures.Client())
        for confirmed in [False,1,'true']:
            with self.assertRaises(ValueError):self.commit(p,confirmed=confirmed)
        self.s._setup_plans[self.c]['expires']=time.monotonic()-1
        with self.assertRaises(ValueError):self.commit(p)
        self.assertEqual(self.disk(),before)
        for changes in [dict(source='bad'),dict(name='x'*12),dict(name='line\nbreak'),dict(tx=1),dict(position='true'),dict(profile_only=0),dict(profile_name='../bad\n'),dict(profile_only=True),dict(source='private',name='')]:
            with self.subTest(changes=changes),self.assertRaises(ValueError):self.preview(**changes)
        for field,value in [('slot',True),('slot',256),('hop',0),('region','UNSET'),('preset','UNKNOWN'),('use_preset','true')]:
            tuning=self.s._setup_context()['tuning'];tuning[field]=value
            with self.subTest(field=field,value=value),self.assertRaises(ValueError):self.preview(tuning=tuning)
        self.assertEqual(self.disk(),before)

    def test_import_offline_uses_explicit_tuning_suggestion_and_preserves_primary(self):
        self.prepare();before=self.disk();url_data=apponly_pb2.ChannelSet()
        imported=url_data.settings.add(name='Imported',psk=b'T'*32)
        imported.uplink_enabled=True;imported.module_settings.position_precision=32
        url_data.lora_config.use_preset=True;url_data.lora_config.modem_preset=4;url_data.lora_config.channel_num=15
        url='https://meshtastic.org/e/#'+base64.urlsafe_b64encode(url_data.SerializeToString()).decode().rstrip('=')
        self.s._begin_import(self.c,'import')
        imported=self.s._preview_import(self.c,{'id':'import','setup':True},url)
        self.assertEqual(imported['slot'],20);self.assertEqual(imported['suggested_tuning']['slot'],15)
        self.assertEqual(self.disk(),before);self.assertNotIn('url',imported)
        p=self.preview(source='import',import_token=imported['token'])
        result=self.commit(p);cfg,chs=self.s._settings.load()
        self.assertEqual(cfg.lora.channel_num,20);self.assertEqual(chs.channels[1].settings.psk,b'T'*32)
        self.assertFalse(chs.channels[1].settings.uplink_enabled);self.assertFalse(chs.channels[1].settings.module_settings.position_precision)
        self.assertEqual(result['added'],1)

    def test_public_location_and_mqtt_need_own_consent_and_clamp_precision(self):
        self.prepare()
        p=self.preview(source='public',name='LongFast',position=True,mqtt=True,tx=True)
        before=self.disk()
        for key in ['antenna_confirmed','public_position_confirmed','mqtt_confirmed']:
            with self.subTest(key=key),self.assertRaises(ValueError):self.commit(p,**{key:False})
        self.assertEqual(self.disk(),before)
        result=self.commit(p);cfg,chs=self.s._settings.load()
        self.assertEqual(chs.channels[1].settings.module_settings.position_precision,15)
        self.assertEqual(chs.channels[1].settings.psk,b'\x01')
        self.assertTrue(self.s._public_position_approved(chs.channels))
        modules=localonly_pb2.LocalModuleConfig.FromString((self.s._settings.prefs/'module.proto').read_bytes())
        self.assertTrue(modules.mqtt.enabled);self.assertTrue(chs.channels[1].settings.uplink_enabled)
        self.assertEqual(result['verified'],'stored')

    def test_online_one_restart_verifies_actual_radio_and_mismatch_rolls_back(self):
        self.prepare(online=True);before=self.disk();stop=self.s.radio_stop
        calls=[]
        def stopping():calls.append('stop');stop()
        def starting():calls.append('start');self.connect_fake()
        with patch.object(self.s,'radio_stop',side_effect=stopping),patch.object(self.s,'radio_start',side_effect=starting):
            result=self.commit(self.preview())
        self.assertEqual(result['verified'],'radio');self.assertEqual(calls,['stop','start'])
        cfg,chs=self.s._settings.load();self.assertEqual(chs.channels[1].role,2)
        previous=self.disk()
        def mismatch():
            self.connect_fake();self.s._iface.localNode.channels[2].settings.name='Wrong'
        with patch.object(self.s,'radio_start',side_effect=mismatch):
            with self.assertRaisesRegex(RuntimeError,'previous settings restored'):self.commit(self.preview(name='Third'))
        self.assertEqual(self.disk(),previous);self.assertEqual(self.s._radio,'error');self.assertIsNone(self.s._iface)

    def test_storage_failure_restores_previous_pair_and_never_restarts(self):
        self.prepare();before=self.disk();original=self.s._settings.apply;calls=[]
        def fail_once(*args):
            calls.append(1)
            if len(calls)==1:
                atomic_write(self.s._settings.prefs/'config.proto',args[0].SerializeToString());raise OSError('synthetic full disk')
            return original(*args)
        with patch.object(self.s._settings,'apply',side_effect=fail_once),patch.object(self.s,'radio_start',side_effect=AssertionError('must stay Off')):
            with self.assertRaisesRegex(RuntimeError,'previous settings restored'):self.commit(self.preview())
        self.assertEqual(self.disk(),before);self.assertEqual(self.s._radio,'error')

    def test_failed_recovery_is_explicit_and_optional_profile_failure_is_partial_success(self):
        self.prepare();p=self.preview()
        with patch.object(self.s._settings,'apply',side_effect=OSError('synthetic write error')):
            with self.assertRaisesRegex(RuntimeError,'recovery could not be confirmed'):self.commit(p)
        self.assertEqual(self.s._radio,'error')
        p=self.preview(profile_name='Optional')
        with patch.object(self.s._settings,'save_profile',side_effect=OSError('synthetic full disk')):
            result=self.commit(p)
        self.assertEqual(result['event'],'channel.setup.saved');self.assertFalse(result['profile']);self.assertIn('not saved',result['profile_error'])

    def test_cancellation_during_readback_wins_before_writes(self):
        self.prepare();p=self.preview();before=self.disk();capture=self.s._setup_snapshot
        def cancel_and_capture():self.s._setup_cancel(self.c);return capture()
        with patch.object(self.s,'_setup_snapshot',side_effect=cancel_and_capture):
            with self.assertRaisesRegex(ValueError,'cancelled'):self.commit(p)
        self.assertEqual(self.disk(),before)

    def test_dispatch_result_correlation_and_disconnect_cleanup(self):
        self.prepare();self.s._dispatch(self.c,dict(intent='channel.setup.context',id='context'))
        self.assertEqual(self.c.events[-1]['id'],'context')
        self.s._setup_begin(self.c,'preview')
        self.s._dispatch(self.c,dict(intent='channel.setup.preview',**self.request()))
        p=self.c.events[-1];self.assertEqual(p['event'],'channel.setup.preview')
        self.s._cancel_import(self.c)
        self.assertNotIn(self.c,self.s._setup_plans)

    def test_cancelled_queued_or_disconnected_preview_does_not_reappear(self):
        self.prepare();req=self.request();self.s._setup_begin(self.c,req['id']);self.s._setup_cancel(self.c,req['id'])
        with self.assertRaisesRegex(ValueError,'cancelled'):self.s._setup_preview(self.c,req)
        self.s._setup_begin(self.c,req['id']);self.c.alive=False
        with self.assertRaisesRegex(ValueError,'cancelled'):self.s._setup_preview(self.c,req)
        self.assertNotIn(self.c,self.s._setup_plans)
        self.c.alive=True;self.s._setup_begin(self.c,req['id']);snapshot=self.s._setup_snapshot
        def cancel_while_preparing():self.s._setup_cancel(self.c);return snapshot()
        with patch.object(self.s,'_setup_snapshot',side_effect=cancel_while_preparing):
            with self.assertRaisesRegex(ValueError,'cancelled'):self.s._setup_preview(self.c,req)
        self.assertNotIn(self.c,self.s._setup_plans)

    def test_fixed_frequency_is_visible_preserved_or_cleared_not_injected(self):
        self.prepare();cfg,chs=self.s._settings.load();cfg.lora.override_frequency=913.124939;cfg.lora.frequency_offset=0.0125;self.s._settings.apply(cfg,chs)
        context=self.s._setup_context();self.assertGreater(context['tuning']['override_frequency'],913)
        p=self.preview();self.commit(p);cfg,chs=self.s._settings.load();self.assertEqual(cfg.lora.override_frequency,context['tuning']['override_frequency'])
        tune=self.s._setup_context()['tuning'];tune['override_frequency']=999
        with self.assertRaises(ValueError):self.preview(tuning=tune)
        tune=self.s._setup_context()['tuning'];tune['region']='EU_868'
        with self.assertRaisesRegex(ValueError,'clear'):self.preview(tuning=tune)
        tune=self.s._setup_context()['tuning'];tune['override_frequency']=0
        p=self.preview(tuning=tune);self.commit(p);cfg,chs=self.s._settings.load();self.assertEqual(cfg.lora.override_frequency,0);self.assertAlmostEqual(cfg.lora.frequency_offset,0.0125)

    def test_quota_duplicate_invalid_layout_and_existing_public_consent(self):
        self.prepare();cfg,chs=self.s._settings.load();chs.channels[0].settings.psk=b'\x01';chs.channels[0].settings.name=''
        self.s._settings.apply(cfg,chs)
        with self.assertRaisesRegex(ValueError,'already present'):self.preview(source='public',name='')
        chs.channels[0].settings.module_settings.position_precision=16;self.s._settings.apply(cfg,chs)
        with self.assertRaisesRegex(ValueError,'unconfirmed'):self.preview()
        chs.channels[0].settings.module_settings.position_precision=0
        for ch in chs.channels[1:]:ch.role=2;ch.settings.name='Occupied';ch.settings.psk=b'O'*32
        self.s._settings.apply(cfg,chs)
        with self.assertRaisesRegex(ValueError,'free'):self.preview()

    def test_history_refresh_failure_reports_confirmed_channel_as_partial_success(self):
        self.prepare();p=self.preview()
        with patch.object(self.s,'_publish_history',side_effect=OSError('synthetic storage fault')):
            result=self.commit(p)
        self.assertEqual(result['event'],'channel.setup.saved')
        self.assertEqual(result['verified'],'stored');self.assertIn('Channel saved',result['warning'])
        self.assertEqual(self.s._settings.load()[1].channels[1].role,2)

if __name__=='__main__':unittest.main()
