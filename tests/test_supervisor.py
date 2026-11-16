# SPDX-License-Identifier: MIT
"""No RF/hardware access. Run with meshtastic==2.7.11 in a temporary venv."""
import copy
import importlib.util
import json
import os
from pathlib import Path
import socket
import sys
import tempfile
import threading
import time
import unittest
from unittest.mock import patch
from types import SimpleNamespace

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'supervisor'))
import supervisor as mod
import firewall
from meshtastic.protobuf import admin_pb2, channel_pb2, localonly_pb2


class Client:
    def __init__(self): self.events = []
    def send(self, obj): self.events.append(copy.deepcopy(obj))


class Node:
    def __init__(self):
        self.localConfig = localonly_pb2.LocalConfig()
        self.localConfig.lora.tx_enabled = True
        self.localConfig.lora.region = 1
        self.moduleConfig = localonly_pb2.LocalModuleConfig()
        self.channels = [channel_pb2.Channel(index=i) for i in range(8)]
        self.channels[0].role = channel_pb2.Channel.PRIMARY
        self.channels[0].settings.psk = b'\x01'
        for i in (1, 2):
            self.channels[i].role = channel_pb2.Channel.SECONDARY
            self.channels[i].settings.name = 'Group ' + str(i)
            self.channels[i].settings.psk = bytes([i + 10]) * 32
            self.channels[i].settings.module_settings.position_precision = 16
        self.saved = None
        self.echo = True
    def writeConfig(self, name): pass
    def writeChannel(self, index): pass
    def _sendAdmin(self, request, wantResponse, onResponse):
        if not self.echo: return
        raw = admin_pb2.AdminMessage()
        field = request.WhichOneof('payload_variant')
        if field == 'get_channel_request':
            raw.get_channel_response.CopyFrom(self.channels[request.get_channel_request - 1])
        elif field == 'get_module_config_request':
            raw.get_module_config_response.mqtt.CopyFrom(self.moduleConfig.mqtt)
        else:
            name = admin_pb2.AdminMessage.ConfigType.Name(request.get_config_request).lower().removesuffix('_config')
            getattr(raw.get_config_response, name).CopyFrom(getattr(self.localConfig, name))
        onResponse({'decoded': {'admin': {'raw': raw}}})


class Radio:
    def __init__(self):
        self.localNode = Node()
        self.nodes = {}
        self.callbacks = []
        self.fail = False
        self.immediate = False
        self.sent = []
    def sendText(self, *args, onResponse=None, **kwargs):
        if self.fail: raise OSError('disconnected')
        self.sent.append((args, kwargs))
        if onResponse:
            self.callbacks.append(onResponse)
            if self.immediate: onResponse({'decoded': {'routing': {'errorReason': 'NONE'}}})
    def getMyNodeInfo(self): return {}


class SupervisorTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        with patch.object(Path, 'home', return_value=Path(self.tmp.name)):
            self.s = mod.Supervisor()
        self.s._iface = Radio()
        self.s._radio = 'online'
        self.c = Client()
        self.s._clients = [self.c]
    def tearDown(self):
        self.s._stop.set()
        self.s._stop_imports()
        self.s._worker.join(1)
        self.s._store.close()
        self.s._state_lease.close()
        self.tmp.cleanup()
    def dispatch(self, **req):
        if "channel" in req and "channel_id" not in req: req["channel_id"] = self.s._channel_identity(req["channel"])
        self.s._dispatch(self.c, req)
    def send(self, **extra):
        self.dispatch(**dict(intent='message.send', channel=1, text='Synthetic test', cid='client-1', **extra))
    def test_public_private_channels_and_strict_unicast(self):
        for ch, dest in [(True, None), (1.5, None), ('1', None), (1, False), (1, ''), (99, '^all'), (1, '^all'), (1, '!ffffffff'), (1, '!00000000'), (1, '../node'), (7, None)]:
            with self.subTest(ch=ch, dest=dest), self.assertRaises(ValueError):
                self.s._require_channel(ch, dest)
        self.s._require_channel(1, '!12345678')
        self.s._require_channel(0, None)
        self.s._require_channel(0, '!12345678')
        self.s._iface.localNode.channels[1].settings.psk = bytes.fromhex('d4f1bb3a20290759f0bcffabcf4e6901')
        self.s._require_channel(1, None)
        self.assertFalse(self.s._private_channel(self.s._iface.localNode.channels[1]))
        self.s._iface.localNode.channels[1].settings.psk = b'invalid'
        with self.assertRaises(ValueError): self.s._require_channel(1, None)

    def test_public_and_private_sends_use_exact_selected_channel(self):
        for channel, dest in [(0, None), (1, None), (2, '!12345678'), (0, '!12345678')]:
            self.dispatch(intent='message.send', channel=channel, dest=dest, text='Synthetic', cid=str((channel,dest)))
            kwargs = self.s._iface.sent[-1][1]
            self.assertEqual(kwargs['channelIndex'], channel)
            self.assertEqual(kwargs['destinationId'], dest or '^all')
        before = len(self.s._iface.sent)
        self.dispatch(intent='message.send', channel=7, text='No fallback', cid='invalid')
        self.assertEqual(len(self.s._iface.sent), before)
        self.assertEqual(self.c.events[-1]['event'], 'error')

    def test_public_key_classification_including_expanded_shorthand(self):
        ch = self.s._iface.localNode.channels[0]
        default = bytes.fromhex('d4f1bb3a20290759f0bcffabcf4e6901')
        for key in [b'', b'\x00', b'\x01', b'\x0a', bytes(16), bytes(32), default, default[:-1]+b'\x0a']:
            ch.settings.psk = key
            self.s._require_channel(0, None)
            self.assertFalse(self.s._private_channel(ch))
        ch.settings.psk = b'R' * 32
        self.assertTrue(self.s._private_channel(ch))
    def test_position_enable_only_explicit_private_channel(self):
        n = self.s._iface.localNode
        self.s._set_position_sharing(True, 2)
        self.assertEqual([c.settings.module_settings.position_precision for c in n.channels[:3]], [0, 0, 16])
        with self.assertRaises(ValueError): self.s._set_position_sharing(True, 0)
        self.s._set_position_sharing(False)
        self.assertFalse(self.s._position_sharing_on())
    def test_channel_share_contains_only_selected_secret(self):
        from base64 import urlsafe_b64decode
        from meshtastic.protobuf import apponly_pb2
        self.dispatch(intent='channel.share', channel=2, id='share-request')
        ev = self.c.events[-1]
        self.assertEqual((ev['channel'], ev['id']), (2, 'share-request'))
        enc = ev['url'].split('#')[1]
        cs = apponly_pb2.ChannelSet.FromString(urlsafe_b64decode(enc + '=' * (-len(enc) % 4)))
        self.assertEqual(len(cs.settings), 1)
        self.assertEqual(cs.settings[0].name, 'Group 2')
        self.assertEqual(cs.settings[0].psk, bytes([12]) * 32)

    def test_public_share_contains_only_selected_channel(self):
        from base64 import urlsafe_b64decode
        from meshtastic.protobuf import apponly_pb2
        self.dispatch(intent='channel.share', channel=0, id='public-share')
        ev = self.c.events[-1]
        enc = ev['url'].split('#')[1]
        cs = apponly_pb2.ChannelSet.FromString(urlsafe_b64decode(enc + '=' * (-len(enc) % 4)))
        self.assertEqual((ev['channel'], ev['id']), (0, 'public-share'))
        self.assertEqual(len(cs.settings), 1)
        self.assertEqual(cs.settings[0].psk, b'\x01')

    def test_public_location_send_requires_separate_confirmation(self):
        with patch.object(self.s, '_my_position', return_value=(1.25, 2.5)):
            for confirmation in [None, False, 1, 'true']:
                self.dispatch(intent='position.send', channel=0, public_confirmed=confirmation)
                self.assertFalse(self.s._iface.sent)
            self.dispatch(intent='position.send', channel=0, public_confirmed=True)
            self.assertEqual(self.s._iface.sent[-1][1]['channelIndex'], 0)
            self.dispatch(intent='position.send', channel=2)
            self.assertEqual(self.s._iface.sent[-1][1]['channelIndex'], 2)

    def test_public_position_opt_in_is_confirmed_and_revocable(self):
        n = self.s._iface.localNode
        self.dispatch(intent='config.set', path='position.enabled', value=True, channel=0, public_confirmed=True, id='public-pos')
        self.assertTrue(self.s._config_confirmed)
        self.assertEqual([ch.settings.module_settings.position_precision for ch in n.channels[:3]], [15, 0, 0])
        self.assertTrue(self.s._public_position_approved(n.channels))
        consent = self.s._state_dir / 'public-position-consent'
        self.assertEqual(consent.stat().st_mode & 0o777, 0o600)
        n.channels[0].settings.name = 'Changed'
        self.assertFalse(self.s._public_position_approved(n.channels))
        n.channels[0].settings.name = ''
        self.dispatch(intent='config.set', path='position.enabled', value=False, id='pos-off')
        self.assertFalse(any(ch.settings.module_settings.position_precision for ch in n.channels))
        self.assertEqual(consent.read_text(), '')

    def test_unconfirmed_position_write_stops_radio(self):
        self.s._iface.localNode.echo = False
        self.dispatch(intent='config.set', path='position.enabled', value=True, channel=0, public_confirmed=True)
        self.assertIsNone(self.s._iface)
        self.assertEqual(self.s._radio, 'error')
        self.assertFalse((self.s._state_dir / 'public-position-consent').exists())
    def test_ack_authoritative_and_retry_old_ack_ignored(self):
        self.send(); old = self.s._iface.callbacks[-1]
        record = self.s._record
        def interleaved(msg, *args):
            record(msg, *args)
            # A prior attempt can ACK precisely between replacing its message
            # and calling the library for the retry.
            old({'decoded': {'routing': {'errorReason': 'NONE'}}})
        self.s._record = interleaved
        self.send(); new = self.s._iface.callbacks[-1]
        self.assertEqual(self.s._messages[0]['status'], 'sending')
        self.assertEqual(len(self.s._messages), 1)
        old({'decoded': {'routing': {'errorReason': 'NONE'}}})
        self.assertEqual(self.s._messages[0]['status'], 'sending')
        new({'decoded': {'routing': {'errorReason': 'NONE'}}})
        self.assertEqual(self.s._messages_payload()['messages'][0]['status'], 'mesh')
    def test_immediate_ack_does_not_get_overwritten(self):
        self.s._iface.immediate = True
        self.send()
        self.assertEqual(self.s._messages[0]['status'], 'mesh')
        order = [e['event'] for e in self.c.events if e['event'] in ('message', 'delivery')]
        self.assertEqual(order, ['message', 'delivery'])
    def test_snapshot_queueing_cannot_race_a_delivery(self):
        self.send(); callback = self.s._iface.callbacks[-1]
        self.c.events.clear()
        snapshot_started, ack_started, ack_done = threading.Event(), threading.Event(), threading.Event()
        send = self.c.send
        def intercepted(obj):
            if obj['event'] == 'messages':
                snapshot_started.set()
                self.assertTrue(ack_started.wait(1))
                ack_done.wait(.05)
            send(obj)
        self.c.send = intercepted
        def ack():
            snapshot_started.wait(1)
            ack_started.set()
            callback({'decoded': {'routing': {'errorReason': 'NONE'}}})
            ack_done.set()
        worker = threading.Thread(target=ack, daemon=True); worker.start()
        self.dispatch(intent='message.list', id='snapshot')
        worker.join(2); self.assertFalse(worker.is_alive())
        self.assertEqual([e['event'] for e in self.c.events], ['messages', 'delivery'])
        self.assertEqual(self.c.events[0]['messages'][0]['status'], 'sending')
        self.assertEqual(self.s._messages[0]['status'], 'mesh')

    def test_no_ack_and_send_failure(self):
        self.send(wantAck=False)
        self.assertEqual(self.s._messages[0]['status'], 'sent')
        self.s._iface.fail = True
        self.send()
        self.assertEqual(self.s._messages[0]['status'], 'failed')
        self.assertEqual(self.c.events[-1]['cid'], 'client-1')
    def test_malformed_ack_never_reports_success(self):
        self.send(); self.s._iface.callbacks[-1]({'decoded': {}})
        self.assertEqual(self.s._messages[0]['status'], 'sending')
    def test_existing_history_trims_every_thread_and_broadcasts(self):
        for i in range(15):
            self.s._record({'id': str(i), 'from': 'rf', 'channel': i % 3, 'to': 'broadcast'})
        self.dispatch(intent='message.retention', cap=2)
        self.assertEqual(len(self.s._messages), 6)
        self.assertEqual(self.c.events[-2]['event'], 'messages')
        self.dispatch(intent='message.clear', thread='ch:legacy:1')
        self.assertEqual(len(self.s._messages), 4)
    def test_global_history_and_attempt_bounds(self):
        for i in range(mod.MAX_MESSAGES + 5):
            self.s._record({'id': str(i), 'from': 'self', 'outgoing': True, 'channel': i % 3, 'to': 'broadcast'})
        self.assertEqual(len(self.s._messages), mod.MAX_MESSAGES)
    def test_real_admin_protobuf_readback_and_config_validation(self):
        self.s._verify_config()
        self.dispatch(intent='config.set', path='position.enabled', value=True, channel=2, id='cfg')
        events = [e for e in self.c.events if e['event'] == 'config']
        self.assertTrue(events[-1]['confirmed'])
        self.assertTrue(events[-1]['position'])
        self.dispatch(intent='config.set', path='mqtt.enabled', value='false', id='bad')
        self.assertFalse(self.s._config_confirmed)
        self.assertEqual(self.c.events[-1]['event'], 'error')
    def test_failed_firewall_never_launches_hardware(self):
        self.s._radio = 'off'; self.s._iface = None
        with patch.object(self.s, '_check_firewall', side_effect=RuntimeError('missing')), patch.object(self.s, '_launch_daemon') as launch:
            self.s.radio_start(); launch.assert_not_called()
        self.assertEqual(self.s._radio, 'error')
    def test_dependency_system_daemon_cannot_bypass_ownership(self):
        def result(code, state): return SimpleNamespace(returncode=code, stdout=state)
        for enabled, active in [('enabled', 'inactive'), ('masked', 'active')]:
            with patch.object(mod.subprocess, 'run', side_effect=[result(0 if enabled == 'enabled' else 1, enabled), result(0 if active == 'active' else 3, active)]):
                with self.assertRaises(RuntimeError): self.s._check_system_daemon()
        with patch.object(mod.subprocess, 'run', side_effect=[result(1, 'masked'), result(3, 'inactive')]):
            self.s._check_system_daemon()

    def test_profile_is_validated_before_firmware_can_transmit(self):
        from meshtastic.protobuf import deviceonly_pb2
        prefs = self.s._state_dir / 'prefs'; prefs.mkdir()
        config = localonly_pb2.LocalConfig(version=24); config.lora.tx_enabled = True
        channels = deviceonly_pb2.ChannelFile(version=24)
        for ch in self.s._iface.localNode.channels: channels.channels.add().CopyFrom(ch)
        def save():
            (prefs / 'config.proto').write_bytes(config.SerializeToString())
            (prefs / 'channels.proto').write_bytes(channels.SerializeToString())
        channels.channels[0].settings.module_settings.position_precision = 13
        save(); self.s._check_persisted_profile()
        actual = deviceonly_pb2.ChannelFile.FromString((prefs / 'channels.proto').read_bytes())
        self.assertEqual(actual.channels[0].settings.module_settings.position_precision, 0)
        self.assertEqual(actual.channels[1].settings.module_settings.position_precision, 16)
        config.lora.tx_enabled = False; save(); self.s._check_persisted_profile()
        config.lora.tx_enabled = True
        channels.channels[0].settings.psk = b'P' * 32
        save(); self.s._check_persisted_profile()
        channels.ClearField('channels'); save()
        with self.assertRaises(RuntimeError): self.s._check_persisted_profile()

    def test_approved_public_position_survives_restart_but_changed_consent_does_not(self):
        from meshtastic.protobuf import deviceonly_pb2
        self.dispatch(intent='config.set', path='position.enabled', value=True, channel=0, public_confirmed=True)
        prefs = self.s._state_dir / 'prefs'; prefs.mkdir()
        config = localonly_pb2.LocalConfig(version=24); config.lora.tx_enabled = True
        channels = deviceonly_pb2.ChannelFile(version=24)
        for ch in self.s._iface.localNode.channels: channels.channels.add().CopyFrom(ch)
        (prefs / 'config.proto').write_bytes(config.SerializeToString())
        path = prefs / 'channels.proto'; path.write_bytes(channels.SerializeToString())
        self.s._check_persisted_profile()
        self.assertEqual(deviceonly_pb2.ChannelFile.FromString(path.read_bytes()).channels[0].settings.module_settings.position_precision, 15)
        channels.channels[0].settings.psk = b'\x02'; path.write_bytes(channels.SerializeToString())
        self.s._check_persisted_profile()
        self.assertEqual(deviceonly_pb2.ChannelFile.FromString(path.read_bytes()).channels[0].settings.module_settings.position_precision, 0)

    def test_empty_and_stale_profiles_cannot_masquerade_as_receive_only(self):
        from meshtastic.protobuf import deviceonly_pb2
        prefs = self.s._state_dir / 'prefs'; prefs.mkdir()
        for version in (0, 23, 25, 24):
            config = localonly_pb2.LocalConfig(version=version)
            channels = deviceonly_pb2.ChannelFile(version=version)
            channels.channels.add(role=1)
            if version != 24: config.lora.tx_enabled = False
            (prefs / 'config.proto').write_bytes(config.SerializeToString())
            (prefs / 'channels.proto').write_bytes(channels.SerializeToString())
            with self.subTest(version=version), self.assertRaises(RuntimeError):
                self.s._check_persisted_profile()

    def test_operation_queue_is_bounded(self):
        self.s._stop.set(); self.s._worker.join(1)
        for _ in range(30): self.s.dispatch(self.c, {'intent': 'radio.start'})
        self.assertEqual(self.s._work.qsize(), 16)
        self.assertIn('busy', self.c.events[-1]['text'])
    def test_complete_oversized_frame_and_partial_line_disconnect(self):
        a, b = socket.socketpair()
        try:
            def send():
                a.sendall(b'x' * (mod.MAX_LINE_BYTES + 1) + b'\n')
                a.shutdown(socket.SHUT_WR)
            thread = threading.Thread(target=send, daemon=True); thread.start()
            self.assertEqual(list(self.s._read_lines(b)), [])
            thread.join(1)
        finally: a.close(); b.close()
    def test_confirmed_cache_write_is_not_radio_readback(self):
        def unchanged_radio():
            self.s._iface.localNode.moduleConfig.mqtt.enabled = False
        with patch.object(self.s, '_verify_config', side_effect=unchanged_radio):
            self.dispatch(intent='config.set', path='mqtt.enabled', value=True, id='write')
        self.assertFalse(self.s._config_confirmed)
        self.assertEqual(self.c.events[-1]['event'], 'error')
        self.assertFalse(any(e['event'] == 'config' for e in self.c.events))
    def test_readback_timeout_does_not_confirm_privacy(self):
        self.s._iface.localNode.echo = False
        self.dispatch(intent='config.set', path='mqtt.enabled', value=False, id='write')
        self.assertFalse(self.s._config_confirmed)
        self.assertIn('unknown', self.c.events[-1]['text'])
    def test_channel_import_supports_public_private_and_resets_location_and_mqtt(self):
        from meshtastic.protobuf import apponly_pb2
        from base64 import urlsafe_b64encode
        cs = apponly_pb2.ChannelSet()
        setting = cs.settings.add(name='', psk=b'\x02')
        def url(): return 'https://meshtastic.org/e/#' + urlsafe_b64encode(cs.SerializeToString()).decode().rstrip('=')
        self.assertEqual(self.s._add_channel(url()), 1)
        self.assertEqual(self.s._iface.localNode.channels[3].settings.psk, b'\x02')
        self.assertFalse(self.s._private_channel(self.s._iface.localNode.channels[3]))
        setting.name = 'Import'
        setting.psk = b'Z' * 32
        setting.module_settings.position_precision = 32
        setting.uplink_enabled = setting.downlink_enabled = True
        self.assertEqual(self.s._add_channel(url()), 1)
        imported = self.s._iface.localNode.channels[4].settings
        self.assertEqual(imported.module_settings.position_precision, 0)
        self.assertFalse(imported.uplink_enabled or imported.downlink_enabled)
        self.assertEqual(self.s._add_channel(url()), 0)
        setting.id = 42
        self.assertEqual(self.s._add_channel(url()), 1)
        self.assertEqual(self.s._iface.localNode.channels[5].settings.id, 42)

    def test_import_validates_entire_set_and_capacity_before_writes(self):
        from meshtastic.protobuf import apponly_pb2
        from base64 import urlsafe_b64encode
        cs = apponly_pb2.ChannelSet()
        cs.settings.add(name='Public', psk=b'\x01')
        bad = cs.settings.add(name='Bad', psk=b'invalid')
        def url(): return 'https://meshtastic.org/e/#'+urlsafe_b64encode(cs.SerializeToString()).decode().rstrip('=')
        before = [ch.SerializeToString() for ch in self.s._iface.localNode.channels]
        with self.assertRaises(ValueError): self.s._add_channel(url())
        self.assertEqual(before, [ch.SerializeToString() for ch in self.s._iface.localNode.channels])
        bad.psk = b'B'*32
        for ch in self.s._iface.localNode.channels[3:7]: ch.role = 2; ch.settings.psk=b'C'*32
        before = [ch.SerializeToString() for ch in self.s._iface.localNode.channels]
        with self.assertRaises(ValueError): self.s._add_channel(url())
        self.assertEqual(before, [ch.SerializeToString() for ch in self.s._iface.localNode.channels])

    def test_frequency_slot_validation_and_readback(self):
        for value in [-1, 256, True, '20', 1.5]:
            with self.assertRaises(ValueError): self.s._set_config('lora.channel_num', value)
        def restart(path, value):
            self.s._set_config(path, value)
        with patch.object(self.s, '_restart_with_config', side_effect=restart):
            self.dispatch(intent='config.set', path='lora.channel_num', value=20, id='slot')
        self.assertTrue(self.s._config_confirmed)
        self.assertEqual(self.c.events[-1]['frequency_slot'], 20)
    def test_socket_bad_json_recovers_and_deep_input_disconnects(self):
        a, b = socket.socketpair()
        a.settimeout(2)
        client = mod.Client(b)
        self.s._clients.append(client)
        worker = threading.Thread(target=self.s._handle_client, args=(b, client), daemon=True)
        worker.start()
        try:
            a.sendall(b'[]\n{"intent":"status","x":NaN}\n{"intent":"status"}\n')
            data = b''
            while data.count(b'"event":"status"') < 2 or b'bad JSON' not in data:
                data += a.recv(65536)
            self.assertIn(b'intent must be a JSON object', data)
            a.sendall(b'{"intent":"status","id":"\\ud800"}\n')
            while a.recv(65536): pass
            worker.join(2); self.assertFalse(worker.is_alive())
        finally:
            client.close(); a.close(); worker.join(2)
    def test_state_directory_is_private(self):
        self.assertEqual(self.s._state_dir.stat().st_mode & 0o777, 0o700)

    def test_runtime_symlink_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            real = Path(tmp) / 'real'; real.mkdir()
            (Path(tmp) / 'cz-mesh').symlink_to(real)
            with patch.dict(os.environ, XDG_RUNTIME_DIR=tmp), self.assertRaises(PermissionError): mod.socket_dir()


class PackageMetadataTests(unittest.TestCase):
    def test_version_display_name_and_licenses(self):
        import re
        manifest = json.loads((ROOT / 'app-builder.json').read_text())
        cmake = (ROOT / 'CMakeLists.txt').read_text()
        from app_version import APP_VERSION
        self.assertEqual(APP_VERSION, manifest['version'])
        self.assertIn('string(JSON APP_METADATA_VERSION', cmake)
        self.assertNotRegex(cmake, r'project\(template_app VERSION [0-9]')
        self.assertIn('Cardputer Zero Mesh', (ROOT / 'cmake/cm0-package.cmake').read_text())
        self.assertEqual(manifest['store']['license'], 'MIT AND GPL-3.0-or-later')
        self.assertTrue((ROOT / 'supervisor/LICENSE.GPL').is_file())


class FirewallTests(unittest.TestCase):
    def test_exact_rule_and_chain_required(self):
        doc = {'nftables': [
            {'chain': {'family': 'inet', 'table': 'cz_mesh', 'name': 'input', 'type': 'filter', 'hook': 'input', 'prio': -1}},
            {'rule': {'family': 'inet', 'table': 'cz_mesh', 'chain': 'input', 'expr': [
                {'match': {'op': '!=', 'left': {'meta': {'key': 'iifname'}}, 'right': 'lo'}},
                {'match': {'op': '==', 'left': {'payload': {'protocol': 'tcp', 'field': 'dport'}}, 'right': 47400}},
                {'drop': None}]}}]}
        self.assertTrue(firewall.protected(doc))
        doc['nftables'][-1]['rule']['expr'].insert(0, {'accept': None})
        self.assertFalse(firewall.protected(doc))



class GuardianProcessTests(unittest.TestCase):
    def test_supervisor_sigkill_stops_daemon_and_restores_switches(self):
        import subprocess
        import signal
        with tempfile.TemporaryDirectory() as tmp:
            runtime = Path(tmp)
            (runtime / 'power').write_text('0')
            (runtime / 'mux').write_text('1')
            controller = subprocess.Popen([sys.executable, str(ROOT / 'tests/guardian_fixture.py'),
                                           'controller', tmp, str(ROOT / 'supervisor')])
            guardian_pid = daemon_pid = None
            try:
                deadline = time.monotonic() + 5
                while not (runtime / 'daemon.pid').exists() and time.monotonic() < deadline: time.sleep(.02)
                self.assertTrue((runtime / 'daemon.pid').exists())
                guardian_pid = int((runtime / 'guardian.pid').read_text())
                daemon_pid = int((runtime / 'daemon.pid').read_text())
                self.assertEqual((runtime / 'power').read_text(), '1')
                controller.kill(); controller.wait()
                deadline = time.monotonic() + 6
                while (runtime / 'power.json').exists() and time.monotonic() < deadline: time.sleep(.02)
                self.assertFalse((runtime / 'power.json').exists())
                self.assertEqual((runtime / 'power').read_text(), '0')
                self.assertEqual((runtime / 'mux').read_text(), '1')
                with self.assertRaises(ProcessLookupError): os.kill(daemon_pid, 0)
            finally:
                if controller.poll() is None: controller.terminate(); controller.wait()
                for pid in (guardian_pid, daemon_pid):
                    if pid:
                        try: os.kill(pid, signal.SIGTERM)
                        except ProcessLookupError: pass

class GuardianDeathTests(unittest.TestCase):
    @unittest.skipUnless(sys.platform.startswith('linux'), 'Linux parent-death signal')
    def test_guardian_sigkill_stops_child_before_recovery(self):
        import subprocess
        import signal
        import radio_guardian
        with tempfile.TemporaryDirectory() as tmp:
            runtime = Path(tmp)
            (runtime / 'power').write_text('0'); (runtime / 'mux').write_text('1')
            controller = subprocess.Popen([sys.executable, str(ROOT / 'tests/guardian_fixture.py'),
                                           'controller', tmp, str(ROOT / 'supervisor')])
            try:
                deadline = time.monotonic() + 5
                while not (runtime / 'daemon.pid').exists() and time.monotonic() < deadline: time.sleep(.02)
                self.assertTrue((runtime / 'daemon.pid').exists())
                child = int((runtime / 'daemon.pid').read_text())
                guardian = int((runtime / 'guardian.pid').read_text())
                os.kill(guardian, signal.SIGKILL)
                deadline = time.monotonic() + 5
                def alive():
                    try: return Path(f'/proc/{child}/stat').read_text().split(') ')[1][0] != 'Z'
                    except FileNotFoundError: return False
                while alive() and time.monotonic() < deadline: time.sleep(.02)
                self.assertFalse(alive())
                # The supervisor/service recovers saved switch values after
                # kernel parent-death enforcement has stopped the daemon.
                with patch.object(radio_guardian.cap_lora, 'POWER_LED', str(runtime / 'power')), patch.object(radio_guardian.cap_lora, 'MUX_LED', str(runtime / 'mux')):
                    radio_guardian.cleanup(runtime)
                self.assertEqual((runtime / 'power').read_text(), '0')
                self.assertEqual((runtime / 'mux').read_text(), '1')
            finally:
                controller.kill(); controller.wait()


if __name__ == '__main__': unittest.main()
