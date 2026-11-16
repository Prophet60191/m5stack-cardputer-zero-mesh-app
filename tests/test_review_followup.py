# SPDX-License-Identifier: MIT
"""Real parser/store/worker contracts with synthetic channels; no camera or RF."""
import base64
import json
import os
from pathlib import Path
import subprocess
import threading
import time
import unittest
from unittest.mock import patch

import test_supervisor as fixtures
from meshtastic.protobuf import apponly_pb2


def share_url(*settings):
    value = apponly_pb2.ChannelSet()
    for setting in settings: value.settings.add().CopyFrom(setting)
    return 'https://meshtastic.org/e/#' + base64.urlsafe_b64encode(value.SerializeToString()).decode().rstrip('=')


class ReviewTests(unittest.TestCase):
    setUp = fixtures.SupervisorTests.setUp
    tearDown = fixtures.SupervisorTests.tearDown

    def new_url(self):
        setting = apponly_pb2.ChannelSet().settings.add(name='Synthetic', psk=b'T' * 32)
        setting.module_settings.position_precision = 32
        setting.uplink_enabled = setting.downlink_enabled = True
        return share_url(setting)

    def preview(self, url=None, client=None, request=None):
        client = client or self.c
        request = request or 'preview-' + str(time.monotonic_ns())
        self.s.dispatch(client, {'intent': 'channel.preview', 'url': url or self.new_url(), 'id': request})
        self.wait_for(lambda: any(e.get('id') == request for e in client.events))
        event = next(e for e in client.events if e.get('id') == request)
        self.assertEqual(event['event'], 'channel.preview', event)
        return event

    def wait_for(self, predicate):
        deadline = time.monotonic() + 2
        while time.monotonic() < deadline:
            if predicate(): return
            time.sleep(0.005)
        self.fail('timed out waiting for synthetic worker')

    def test_duplicate_real_url_returns_zero_without_writes_or_readback(self):
        existing = share_url(self.s._iface.localNode.channels[1].settings)
        with patch.object(self.s._iface.localNode, 'writeChannel') as write, patch.object(self.s, '_verify_config') as verify:
            self.assertEqual(self.s._add_channel(existing), 0)
            event = self.preview(existing)
        write.assert_not_called(); verify.assert_not_called()
        self.assertEqual(event['new'], 0); self.assertEqual(event['token'], '')
        self.assertNotIn(existing, json.dumps(self.c.events))

    def test_preview_and_confirm_use_real_importer_preserving_tuning_and_privacy(self):
        node = self.s._iface.localNode
        before = [ch.SerializeToString() for ch in node.channels]
        lora = node.localConfig.lora.SerializeToString()
        url = self.new_url(); event = self.preview(url)
        self.assertEqual(event['new'], 1); self.assertTrue(event['channels'][0]['private'])
        self.assertEqual(before, [ch.SerializeToString() for ch in node.channels])
        self.assertNotIn(url, json.dumps(self.c.events)); self.assertNotIn('psk', json.dumps(event))
        self.s._dispatch(self.c, {'intent': 'channel.add', 'id': 'commit', 'token': event['token'], 'confirmed': True})
        result = next(e for e in self.c.events if e.get('id') == 'commit')
        self.assertEqual(result, {'event': 'channel.imported', 'id': 'commit', 'added': 1})
        added = node.channels[3]
        self.assertEqual(added.settings.name, 'Synthetic')
        self.assertFalse(added.settings.module_settings.position_precision or added.settings.uplink_enabled or added.settings.downlink_enabled)
        self.assertEqual(node.localConfig.lora.SerializeToString(), lora)
        self.assertTrue(any(e.get('event') == 'notice' and 'Imported 1' in e['text'] for e in self.c.events))
        self.assertEqual(self.preview(url)['new'], 0)  # Actual importer produced the duplicate.

    def test_missing_consent_token_connection_expiry_and_changed_settings_rejected(self):
        before = [ch.SerializeToString() for ch in self.s._iface.localNode.channels]
        event = self.preview()
        for client, req in [(self.c, {'url': self.new_url()}), (self.c, {'token': event['token']}),
                            (fixtures.Client(), {'token': event['token'], 'confirmed': True})]:
            self.s._dispatch(client, {'intent': 'channel.add', **req})
            self.assertEqual(client.events[-1]['event'], 'error')
        self.s._import_previews[self.c]['expires'] = 0
        self.s._dispatch(self.c, {'intent': 'channel.add', 'token': event['token'], 'confirmed': True})
        self.assertIn('expired', self.c.events[-1]['text'])
        event = self.preview(); self.s._iface.localNode.localConfig.lora.hop_limit = 4
        self.s._dispatch(self.c, {'intent': 'channel.add', 'token': event['token'], 'confirmed': True})
        self.assertIn('changed', self.c.events[-1]['text'])
        self.assertEqual(before, [ch.SerializeToString() for ch in self.s._iface.localNode.channels])

    def test_cancel_preview_discards_connection_bound_secret_and_stale_cancel_is_safe(self):
        first = self.preview(request='old')
        second = self.preview(request='new')
        self.s.dispatch(self.c, {'intent': 'channel.import.cancel', 'request': 'old'})
        self.assertEqual(self.s._import_previews[self.c]['token'], second['token'])
        self.s.dispatch(self.c, {'intent': 'channel.import.cancel', 'request': 'new'})
        self.assertNotIn(self.c, self.s._import_previews)
        self.s._dispatch(self.c, {'intent': 'channel.add', 'confirmed': True, 'token': first['token']})
        self.assertEqual(self.c.events[-1]['event'], 'error')

    def test_cancelled_queued_preview_and_disconnected_client_cannot_recreate_tokens(self):
        self.s._begin_import(self.c, 'queued')
        self.s.dispatch(self.c, {'intent': 'channel.import.cancel', 'request': 'queued'})
        self.s._dispatch(self.c, {'intent': 'channel.preview', 'id': 'queued', 'url': self.new_url()})
        self.assertFalse(self.c.events); self.assertFalse(self.s._import_previews)
        self.c.alive = False
        self.s._dispatch(self.c, {'intent': 'channel.preview', 'id': 'disconnected', 'url': self.new_url()})
        self.assertFalse(self.c.events); self.assertFalse(self.s._import_previews)

    def test_capture_does_not_block_radio_worker_and_cancels_without_import(self):
        entered, release = threading.Event(), threading.Event()
        def capture(cancelled):
            entered.set(); cancelled.wait(2); release.set(); return self.new_url()
        with patch.object(self.s, '_scan_qr', side_effect=capture), patch.object(self.s, '_add_channel') as add:
            self.s.dispatch(self.c, {'intent': 'channel.scan', 'id': 'scan'})
            self.assertTrue(entered.wait(1))
            self.s.dispatch(self.c, {'intent': 'channel.scan', 'id': 'second'})
            self.assertIn('already scanning', self.c.events[-1]['text'])
            self.s.dispatch(self.c, {'intent': 'status', 'id': 'status-during-camera'})
            self.wait_for(lambda: any(e.get('id') == 'status-during-camera' for e in self.c.events))
            self.s.dispatch(self.c, {'intent': 'channel.import.cancel', 'request': 'scan'})
            self.assertTrue(release.wait(1)); self.wait_for(lambda: self.s._scan_job is None)
            self.assertFalse(any(e.get('id') == 'scan' for e in self.c.events))
            add.assert_not_called(); self.assertFalse(self.s._import_previews)

    def test_failed_camera_worker_start_does_not_strand_the_camera_slot(self):
        with patch('threading.Thread.start', side_effect=RuntimeError('synthetic resource limit')):
            self.s.dispatch(self.c, {'intent': 'channel.scan', 'id': 'start-failed'})
        self.assertIsNone(self.s._scan_job); self.assertFalse(self.s._import_latest)
        self.assertEqual(self.c.events[-1]['event'], 'error')
        self.assertEqual(self.s._radio, 'online')

    def test_real_scan_preview_duplicate_and_failure_contracts(self):
        existing = share_url(self.s._iface.localNode.channels[1].settings)
        for value, failure in [(existing, None), (self.new_url(), None), (None, None), (None, RuntimeError('Camera busy'))]:
            with self.subTest(value=bool(value), failure=failure), patch.object(self.s, '_scan_qr', return_value=value, side_effect=failure):
                self.c.events.clear()
                self.s.dispatch(self.c, {'intent': 'channel.scan', 'id': 'scan-contract'})
                self.wait_for(lambda: any(e.get('id') == 'scan-contract' for e in self.c.events))
                self.wait_for(lambda: self.s._scan_job is None)
                result = self.c.events[-1]
                self.assertEqual(result['event'], 'channel.preview' if value else 'error')
                if value: self.assertEqual(result['new'], 0 if value == existing else 1)
                self.assertNotIn('https://', json.dumps(self.c.events))

    def test_drafts_do_not_fill_radio_queue_during_admin_wait(self):
        entered, release = threading.Event(), threading.Event()
        def admin_wait(): entered.set(); release.wait(2)
        self.s._submit(admin_wait); self.assertTrue(entered.wait(1))
        try:
            for i in range(30):
                self.s.dispatch(self.c, {'intent': 'thread.state', 'thread': 'ch:synthetic', 'state': {'draft': str(i), 'pinned': True}})
            self.assertEqual(self.s._work.qsize(), 0)
            self.assertEqual(self.s._store.load_threads()['ch:synthetic']['draft'], '29')
            self.assertFalse(any(e.get('event') == 'error' for e in self.c.events))
        finally: release.set()

    def test_message_identifier_is_distinct_from_request_identifier(self):
        self.s._record({'from': 'self', 'id': 'target', 'text': 'Synthetic'})
        self.s._record({'from': 'self', 'id': 'request', 'text': 'Keep'})
        self.s._dispatch(self.c, {'intent': 'message.delete', 'id': 'request', 'message_id': 'target'})
        self.assertEqual([m['id'] for m in self.s._messages], ['request'])
        self.assertEqual(self.c.events[-1]['id'], 'request')

    def test_invalid_intent_keeps_a_valid_request_identifier(self):
        self.s.dispatch(self.c, {'intent': False, 'id': 'validation'})
        self.assertEqual(self.c.events[-1]['id'], 'validation')

    def test_read_only_firewall_health_command_needs_no_privileged_child(self):
        import firewall
        with patch('sys.argv', ['firewall.py', 'health']), patch.object(firewall, 'health_status', return_value={'ok': True}), patch('subprocess.run') as run:
            self.assertEqual(firewall.main(), 0)
            run.assert_not_called()

    def test_greeting_sends_history_once_and_queries_versions_outside_lifecycle_lock(self):
        original_send = self.c.send
        def send(event):
            if event['event'] == 'threads': self.assertTrue(self.s._lock._is_owned())
            original_send(event)
        def query(*args, **kwargs):
            self.assertFalse(self.s._life_lock._is_owned())
            return subprocess.CompletedProcess(args, 0, '2.7.26-synthetic', '')
        with patch('subprocess.run', side_effect=query) as run, patch.object(self.s, '_read_lines', return_value=iter(())), \
                patch.object(self.s, '_check_firewall', return_value={'ok': True}), patch.object(self.c, 'close', create=True), \
                patch.object(self.c, 'send', side_effect=send):
            self.s._handle_client(None, self.c)
            self.assertEqual(sum(e['event'] == 'messages' for e in self.c.events), 1)
            self.s._daemon_version(); self.assertEqual(run.call_count, 1)
            self.s._daemon_version(refresh=True); self.assertEqual(run.call_count, 2)

    def test_status_captures_iface_and_survives_an_incomplete_library_node(self):
        radio = self.s._iface
        def detach(): self.s._iface = None; return {}
        radio.getMyNodeInfo = detach
        self.assertEqual(self.s._status()['mode'], 'transmit')
        self.s._iface = radio; radio.localNode = None
        self.assertEqual(self.s._status()['mode'], 'unknown')

    def test_scanner_process_and_camera_child_are_stopped_on_cancel(self):
        # Exercise the real process-group owner with two harmless sleepers.
        script = Path(self.tmp.name) / 'qr_scan.py'
        child_file = Path(self.tmp.name) / 'child.pid'
        script.write_text('import subprocess,sys,time\nfrom pathlib import Path\n' +
            'child=subprocess.Popen([sys.executable,"-c","import time; time.sleep(30)"])\n' +
            f'Path({str(child_file)!r}).write_text(str(child.pid))\ntime.sleep(30)\n')
        cancelled = threading.Event(); results = []
        with patch('supervisor.HERE', Path(self.tmp.name)):
            worker = threading.Thread(target=lambda: results.append(self.s._scan_qr(cancelled)))
            worker.start(); self.wait_for(child_file.exists); cancelled.set(); worker.join(2)
        self.assertFalse(worker.is_alive()); self.assertEqual(results, [None])
        pid = int(child_file.read_text())
        def stopped():
            try: os.kill(pid, 0)
            except ProcessLookupError: return True
            # A container without a reaping init can temporarily retain a zombie;
            # it has no running camera/process resources.
            stat = Path(f'/proc/{pid}/stat')
            return stat.exists() and stat.read_text().split()[2] == 'Z'
        self.wait_for(stopped)
