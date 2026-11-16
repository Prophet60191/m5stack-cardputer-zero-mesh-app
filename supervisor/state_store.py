# SPDX-License-Identifier: GPL-3.0-or-later
"""Private, bounded session persistence. Never stores channel keys or share URLs."""
from __future__ import annotations

import json
import os
import re
import sqlite3
import stat
import threading
from pathlib import Path


class StateStore:
    def __init__(self, directory: Path):
        self._lock = threading.RLock()
        path = directory / "conversations.sqlite3"
        if not path.exists():
            fd = os.open(path, os.O_CREAT | os.O_EXCL | os.O_RDWR, 0o600)
            os.close(fd)
        info = path.lstat()
        if not stat.S_ISREG(info.st_mode) or info.st_uid != os.getuid():
            raise PermissionError("conversation database must be an owned regular file")
        path.chmod(0o600)
        self._db = sqlite3.connect(path, check_same_thread=False, timeout=3)
        self._db.execute("PRAGMA journal_mode=WAL")
        self._db.execute("PRAGMA synchronous=FULL")
        self._db.execute("PRAGMA secure_delete=ON")
        self._db.executescript("""
            CREATE TABLE IF NOT EXISTS messages (
                sender TEXT NOT NULL, id TEXT NOT NULL, payload TEXT NOT NULL,
                seq INTEGER NOT NULL DEFAULT 0,
                PRIMARY KEY(sender,id));
            CREATE TABLE IF NOT EXISTS preferences (
                name TEXT PRIMARY KEY, payload TEXT NOT NULL);
            CREATE TABLE IF NOT EXISTS threads (
                name TEXT PRIMARY KEY, payload TEXT NOT NULL);
        """)
        columns = [row[1] for row in self._db.execute("PRAGMA table_info(messages)")]
        if "seq" not in columns: self._db.execute("ALTER TABLE messages ADD COLUMN seq INTEGER NOT NULL DEFAULT 0")
        self._message_cache = {(sender, ident): (payload, seq) for sender, ident, payload, seq in
                               self._db.execute("SELECT sender,id,payload,seq FROM messages")}
        self._sequence = max((value[1] for value in self._message_cache.values()), default=0)
        self._db.commit()
        # Remove metadata left by older versions of the opt-out policy as well.
        if not self.preference("history.enabled", True):
            self.set_preference("history.enabled", False)

    def preference(self, name, default=None):
        with self._lock:
            row = self._db.execute("SELECT payload FROM preferences WHERE name=?", (name,)).fetchone()
            return json.loads(row[0]) if row else default

    def set_preference(self, name, value):
        with self._lock:
            with self._db:
                self._db.execute("INSERT OR REPLACE INTO preferences VALUES (?,?)", (name, json.dumps(value)))
                if name == "history.enabled" and value is False:
                    self._db.execute("DELETE FROM messages")
                    self._db.execute("DELETE FROM threads")
            if name == "history.enabled" and value is False:
                self._message_cache.clear()
                self._db.execute("PRAGMA wal_checkpoint(TRUNCATE)")


    def load_messages(self):
        with self._lock:
            if not self.preference("history.enabled", True): return []
            rows = self._db.execute("SELECT payload FROM messages ORDER BY seq,rowid LIMIT 2000").fetchall()
            return [json.loads(row[0]) for row in rows]

    def replace_messages(self, messages):
        # One transaction makes retention/deletion/retry state authoritative on
        # disk as well as in a reconnect snapshot. Hard global cap also applies
        # to "All"; storing data never schedules or replays an RF action.
        with self._lock:
            if not self.preference("history.enabled", True): return
            updated = {}
            changed = []
            previous = -1
            for message in messages[-2000:]:
                if not message.get("id"): continue
                key = (str(message.get("from", "")), str(message["id"]))
                cached = self._message_cache.get(key)
                sequence = cached[1] if cached else -1
                if sequence <= previous:
                    self._sequence += 1
                    sequence = self._sequence
                previous = sequence
                payload = json.dumps(message, ensure_ascii=False, allow_nan=False)
                updated[key] = (payload, sequence)
                if cached != (payload, sequence): changed.append((*key, payload, sequence))
            removed = self._message_cache.keys() - updated.keys()
            with self._db:
                self._db.executemany("DELETE FROM messages WHERE sender=? AND id=?", list(removed))
                self._db.executemany("INSERT INTO messages VALUES (?,?,?,?) ON CONFLICT(sender,id) DO UPDATE SET payload=excluded.payload,seq=excluded.seq", changed)
            self._message_cache = updated

    def load_threads(self):
        with self._lock:
            if not self.preference("history.enabled", True): return {}
            return {name: json.loads(payload) for name, payload in
                    self._db.execute("SELECT name,payload FROM threads LIMIT 256")}

    @staticmethod
    def validate_thread_name(name):
        if (not isinstance(name, str) or len(name) <= 3 or len(name) > 160 or
                not name.startswith(("ch:", "dm:")) or
                (name.startswith("dm:") and not re.fullmatch(r"dm:![0-9a-fA-F]{8}(?:@[A-Za-z0-9_-]{1,64})?", name))):
            raise ValueError("invalid conversation identity")

    @classmethod
    def clean_thread(cls, name, value):
        cls.validate_thread_name(name)
        if not isinstance(value, dict): raise ValueError("invalid conversation state")
        draft = value.get("draft", "")
        if not isinstance(draft, str) or len(draft.encode("utf-8")) > 200:
            raise ValueError("draft must be at most 200 UTF-8 bytes")
        if type(value.get("pinned", False)) is not bool:
            raise ValueError("pinned must be boolean")
        return {"draft": draft, "pinned": value.get("pinned", False)}

    def set_thread(self, name, value):
        clean = self.clean_thread(name, value)
        with self._lock, self._db:
            if not self.preference("history.enabled", True): return
            payload = json.dumps(clean)
            existing = self._db.execute("SELECT payload FROM threads WHERE name=?", (name,)).fetchone()
            if existing and existing[0] == payload: return
            if not existing and self._db.execute("SELECT COUNT(*) FROM threads").fetchone()[0] >= 256:
                raise ValueError("conversation state limit reached")
            self._db.execute("INSERT OR REPLACE INTO threads VALUES (?,?)", (name, payload))

    def delete_thread(self, name):
        self.validate_thread_name(name)
        with self._lock, self._db:
            keys = [key for (key,) in self._db.execute("SELECT name FROM threads") if key == name or key.startswith(name + "@")]
            self._db.executemany("DELETE FROM threads WHERE name=?", [(key,) for key in keys])

    def close(self):
        with self._lock:
            self._db.close()
