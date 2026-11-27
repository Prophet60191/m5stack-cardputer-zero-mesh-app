# SPDX-License-Identifier: MIT
"""SQLite transaction failures and live RF callback outcomes; no hardware/RF."""
import copy
import unittest
from unittest.mock import Mock, patch
import test_supervisor as fixtures


class HistoryFailureTests(unittest.TestCase):
    setUp = fixtures.SupervisorTests.setUp
    tearDown = fixtures.SupervisorTests.tearDown

    def message(self, ident='synthetic', outgoing=False):
        return {'id': ident, 'from': 'self' if outgoing else '!12345678',
                'to': 'broadcast', 'channel': 1, 'channel_id': 'synthetic',
                'text': 'Synthetic test', 'outgoing': outgoing, 'status': 'sending' if outgoing else ''}

    def fail_sql(self, table='messages', action='INSERT'):
        self.s._store._db.execute(f"CREATE TEMP TRIGGER fail_write BEFORE {action} ON {table} BEGIN SELECT RAISE(ABORT, 'synthetic storage failure'); END")

    def restore_sql(self):
        self.s._store._db.execute('DROP TRIGGER fail_write')

    def test_incoming_message_remains_visible_when_history_write_fails(self):
        self.fail_sql()
        self.s._on_receive_text({'from': 0x12345678, 'id': 1, 'channel': 1,
                                'decoded': {'text': 'Synthetic test'}}, self.s._iface)
        self.assertTrue(any(e.get('event') == 'message' for e in self.c.events))
        self.assertTrue(any('memory only' in e.get('text', '') for e in self.c.events))
        self.assertEqual(len(self.s._messages), 1)
        self.assertEqual(self.s._store.load_messages(), [])

    def test_delivery_is_published_and_later_persisted_after_storage_recovers(self):
        token = ('synthetic-attempt', 0)
        self.s._record(self.message(outgoing=True), token)
        self.fail_sql()
        self.s._delivery('synthetic', token, 'delivered')
        self.assertTrue(any(e.get('event') == 'delivery' and e.get('status') == 'delivered' for e in self.c.events))
        self.assertEqual(self.s._messages[0]['status'], 'delivered')
        self.assertEqual(self.s._store.load_messages()[0]['status'], 'sending')
        self.restore_sql()
        self.s._publish_history(self.c)
        self.assertEqual(self.s._store.load_messages()[0]['status'], 'delivered')
        self.assertTrue(any('saving recovered' in e.get('text', '') for e in self.c.events))

    def test_timeout_storage_failure_cannot_kill_the_radio_health_monitor(self):
        token = ('synthetic-attempt', 0)
        self.s._record(self.message(outgoing=True), token)
        self.fail_sql()
        self.s._daemon = Mock(); self.s._daemon.poll.return_value = None
        daemon = self.s._daemon
        try:
            with patch.object(self.s, '_check_firewall'), patch('supervisor.time.monotonic', return_value=61), patch.object(self.s._stop, 'wait', side_effect=lambda _: self.s._stop.set()):
                self.s._watch_daemon(daemon)
            self.assertEqual(self.s._radio, 'online')
            self.assertEqual(self.s._messages[0]['status'], 'failed')
        finally:
            self.s._daemon = None

    def test_repeated_write_failures_warn_once_but_do_not_block_snapshots(self):
        self.fail_sql()
        self.s._messages = [self.message()]
        for _ in range(3): self.s._publish_history()
        warnings = [e for e in self.c.events if 'memory only' in e.get('text', '')]
        self.assertEqual(len(warnings), 1)
        self.assertEqual(sum(e.get('event') == 'messages' for e in self.c.events), 3)
        self.assertNotIn('synthetic storage failure', str(self.c.events))

    def test_failed_delete_and_clear_keep_original_ram_and_disk(self):
        for intent, extra in [('message.delete', {'message_id': 'synthetic'}),
                              ('message.clear', {'thread': 'ch:synthetic'})]:
            with self.subTest(intent=intent):
                self.s._record(self.message())
                original = copy.deepcopy(self.s._messages)
                self.fail_sql(action='DELETE')
                self.s._dispatch(self.c, {'intent': intent, **extra})
                self.assertEqual(self.c.events[-1]['event'], 'error')
                self.assertEqual(self.s._messages, original)
                self.assertEqual(self.s._store.load_messages(), original)
                self.restore_sql()

    def test_failed_retention_keeps_history_limit_and_pending_attempts(self):
        self.s._record(self.message('first', outgoing=True), ('first-attempt', 0))
        self.s._record(self.message('second', outgoing=True), ('second-attempt', 0))
        original, attempts = copy.deepcopy(self.s._messages), dict(self.s._attempts)
        self.fail_sql(action='DELETE')
        self.s._dispatch(self.c, {'intent': 'message.retention', 'cap': 1})
        self.assertEqual(self.c.events[-1]['event'], 'error')
        self.assertEqual(self.s._retention, 0)
        self.assertEqual(self.s._messages, original)
        self.assertEqual(self.s._attempts, attempts)

    def test_enabling_history_is_one_transaction_including_ram_drafts(self):
        self.s._store.set_preference('history.enabled', False)
        self.s._messages = [self.message()]
        self.s._threads = {'ch:synthetic': {'draft': 'Synthetic draft', 'pinned': True}}
        self.fail_sql(table='threads')
        self.s._dispatch(self.c, {'intent': 'history.set', 'enabled': True})
        self.assertEqual(self.c.events[-1]['event'], 'error')
        self.assertFalse(self.s._store.preference('history.enabled'))
        self.assertEqual(self.s._store._db.execute('SELECT COUNT(*) FROM messages').fetchone()[0], 0)
        self.assertEqual(self.s._store.load_threads(), {})
        self.restore_sql()
        self.s._dispatch(self.c, {'intent': 'history.set', 'enabled': True})
        self.assertTrue(self.s._store.preference('history.enabled'))
        self.assertEqual(self.s._store.load_messages(), self.s._messages)
        self.assertEqual(self.s._store.load_threads(), self.s._threads)

    def test_failed_opt_in_does_not_falsely_acknowledge_ram_drafts_as_durable(self):
        self.s._store.set_preference('history.enabled', False)
        state = {'draft': 'Synthetic draft', 'pinned': False}
        self.s._dispatch(self.c, {'intent': 'thread.state', 'thread': 'ch:synthetic', 'state': state})
        self.fail_sql(table='threads')
        self.s._dispatch(self.c, {'intent': 'history.set', 'enabled': True})
        self.s._dispatch(self.c, {'intent': 'thread.state', 'id': 'draft-result',
                                 'thread': 'ch:synthetic', 'state': state})
        self.assertEqual(self.c.events[-1]['event'], 'thread.saved')
        self.assertFalse(self.c.events[-1]['durable'])

    def test_duplicate_radio_packet_does_not_publish_an_extra_message(self):
        packet = {'from': 0x12345678, 'id': 1, 'channel': 1,
                  'decoded': {'text': 'Synthetic test'}}
        self.s._on_receive_text(packet, self.s._iface)
        self.s._on_receive_text(packet, self.s._iface)
        self.assertEqual(sum(e.get('event') == 'message' for e in self.c.events), 1)

    def test_diagnostics_exposes_failed_history_save_without_sensitive_details(self):
        self.fail_sql(); self.s._messages = [self.message()]
        self.s._publish_history()
        with patch.object(self.s, '_daemon_version', return_value='synthetic'), patch.object(self.s, '_check_firewall', return_value={'ok': True}):
            data = self.s._diagnostics()
        self.assertFalse(data['history_storage_ok'])
        self.assertNotIn('synthetic storage failure', str(data))
        self.restore_sql(); self.s._publish_history()
        self.assertFalse(self.s._history_storage_error)

    def test_rf_duplicates_do_not_reorder_history_or_rewrite_database(self):
        first, second = self.message('first'), self.message('second')
        self.s._record(first); self.s._record(second)
        changes = self.s._store._db.total_changes
        self.s._record(first)
        self.assertEqual([m['id'] for m in self.s._messages], ['first', 'second'])
        self.assertEqual(self.s._store._db.total_changes, changes)


if __name__ == '__main__':
    unittest.main()
