# SPDX-License-Identifier: MIT
"""Idempotence and strict validation during the maintenance pass; no hardware."""
import unittest
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
