# SPDX-License-Identifier: MIT
"""Idempotence and strict validation during the maintenance pass; no hardware."""
import unittest
from unittest.mock import patch
import test_supervisor as fixtures


class MaintenanceTests(unittest.TestCase):
    setUp = fixtures.SupervisorTests.setUp
    tearDown = fixtures.SupervisorTests.tearDown

    def test_identical_thread_saves_do_not_write_or_broadcast_again(self):
        request = {'intent': 'thread.state', 'thread': 'ch:synthetic',
                   'state': {'draft': 'Synthetic draft', 'pinned': True}}
        self.s.dispatch(self.c, request)
        before = self.s._store._db.total_changes
        events = len(self.c.events)
        for _ in range(100): self.s.dispatch(self.c, request)
        self.assertEqual(self.s._store._db.total_changes, before)
        self.assertEqual(len(self.c.events), events)
        # Standalone persistence callers have the same no-write property.
        for _ in range(100): self.s._store.set_thread(request['thread'], request['state'])
        self.assertEqual(self.s._store._db.total_changes, before)
        request['state']['draft'] = 'Changed draft'
        self.s.dispatch(self.c, request)
        self.assertEqual(self.s._store.load_threads()['ch:synthetic']['draft'], 'Changed draft')
        self.assertEqual(len(self.c.events), events+1)

    def test_equal_python_values_cannot_bypass_boolean_validation(self):
        self.s.dispatch(self.c, {'intent': 'thread.state', 'thread': 'ch:synthetic',
            'state': {'draft': 'Synthetic', 'pinned': True}})
        before = self.s._store._db.total_changes
        # Python considers True == 1. Validation must precede idempotence checks.
        self.s.dispatch(self.c, {'intent': 'thread.state', 'thread': 'ch:synthetic',
            'state': {'draft': 'Synthetic', 'pinned': 1}})
        self.assertEqual(self.c.events[-1]['event'], 'error')
        self.assertEqual(self.s._store._db.total_changes, before)
        for name in ('bad', 'dm:invalid', None):
            with self.assertRaises(ValueError): self.s._store.set_thread(name, {})
            with self.assertRaises(ValueError): self.s._store.delete_thread(name)

    def test_rejected_ram_limit_never_commits_an_extra_disk_record(self):
        self.s._threads = {f'ch:synthetic-{i}': {'draft': '', 'pinned': False} for i in range(256)}
        self.s.dispatch(self.c, {'intent': 'thread.state', 'thread': 'ch:overflow', 'state': {}})
        self.assertEqual(self.c.events[-1]['event'], 'error')
        self.assertNotIn('ch:overflow', self.s._store.load_threads())
        self.assertEqual(len(self.s._threads), 256)

    def test_correlated_save_acceptance_includes_unchanged_without_new_disk_write(self):
        request = {'intent': 'thread.state', 'id': 'synthetic-save-1', 'thread': 'ch:synthetic',
                   'state': {'draft': 'Synthetic', 'pinned': True}}
        self.s.dispatch(self.c, request)
        self.assertEqual(self.c.events[-1], {'event': 'thread.saved', 'id': request['id'],
            'thread': request['thread'], 'durable': True})
        before = self.s._store._db.total_changes
        start = len(self.c.events)
        for i in range(100):
            self.s.dispatch(self.c, {**request, 'id': f'synthetic-save-{i+2}'})
        self.assertEqual(self.s._store._db.total_changes, before)
        self.assertEqual(len(self.c.events)-start, 100)
        self.assertTrue(all(e['event']=='thread.saved' for e in self.c.events[start:]))

    def test_ram_acceptance_is_explicit_and_failed_commits_never_acknowledge(self):
        self.s._store.set_preference('history.enabled', False)
        request = {'intent': 'thread.state', 'id': 'synthetic-ram', 'thread': 'ch:synthetic',
                   'state': {'draft': 'Synthetic', 'pinned': False}}
        self.s.dispatch(self.c, request)
        self.assertFalse(self.c.events[-1]['durable'])
        self.assertEqual(self.s._store.load_threads(), {})
        self.s._store.set_preference('history.enabled', True)
        with patch.object(self.s._store, 'set_thread', side_effect=OSError('synthetic disk failure')):
            self.s.dispatch(self.c, {**request, 'id':'synthetic-failed',
                'state':{'draft':'Changed', 'pinned':False}})
        self.assertEqual(self.c.events[-1]['event'], 'error')
        self.assertEqual(self.c.events[-1]['id'], 'synthetic-failed')
        self.assertEqual(self.s._threads['ch:synthetic']['draft'], 'Synthetic')

    def test_two_clients_receive_only_their_own_save_acceptance(self):
        other = fixtures.Client()
        self.s._clients.append(other)
        request = {'intent':'thread.state','thread':'ch:synthetic',
                   'state':{'draft':'First client','pinned':False},'id':'first-request'}
        self.s.dispatch(self.c, request)
        self.assertFalse(any(e.get('event')=='thread.saved' for e in other.events))
        self.s.dispatch(other, {**request,'id':'second-request',
            'state':{'draft':'Second client','pinned':True}})
        self.assertEqual(other.events[-1]['id'],'second-request')
        self.assertEqual(self.s._store.load_threads()['ch:synthetic']['draft'],'Second client')
        self.assertFalse(any(e.get('id')=='second-request' for e in self.c.events))

    def test_public_channel_creation_readback_is_classified_public(self):
        self.s._channel_manage({'action':'create','kind':'public','name':'LongFast','confirmed':True})
        channel = next(c for c in self.s._iface.localNode.channels if c.settings.name=='LongFast')
        self.assertEqual(channel.role, 2)
        self.assertEqual(bytes(channel.settings.psk), b'\x01')
        self.assertFalse(self.s._private_channel(channel))
        entry = next(c for c in self.s._channel_slots()['channels'] if c['index']==channel.index)
        self.assertFalse(entry['private'])
