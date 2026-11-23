# SPDX-License-Identifier: GPL-3.0-or-later
"""Read current node positions without RF broadcasts or a second API client.

The qualified native daemon supports the node-only configuration nonce 69421.
Its local GPS updates can stay inside NodeDB when location broadcasts are off;
the Python interface's getMyNodeInfo() otherwise returns its old cached copy.
"""
import threading
import time
from meshtastic.protobuf import mesh_pb2

NODE_ONLY_NONCE = 69421


class LocalNodeRefresh:
    INTERVAL = 30
    TIMEOUT = 10

    def __init__(self, clock=time.monotonic):
        self._clock = clock
        self._lock = threading.Lock()
        self.reset()

    def reset(self):
        with self._lock:
            self._iface = None
            self._due = 0
            self._deadline = None
            self._state = 'off'

    def tick(self, iface):
        # Called by the serialized radio owner, never by the reader thread.
        if iface is None:
            self.reset()
            return
        now = self._clock()
        with self._lock:
            if iface is not self._iface:
                self._iface, self._due, self._deadline = iface, now, None
            if self._deadline is not None:
                if now >= self._deadline:
                    self._deadline = None
                    self._due = now + self.INTERVAL
                    self._state = 'timeout'
                return
            if now < self._due:
                return
            self._due = now + self.INTERVAL
            self._deadline = now + self.TIMEOUT
            self._state = 'waiting'
        try:
            # Do not use _startConfig(): that clears SDK node/channel caches.
            # Node-only replies contain no channels; configId is left intact so
            # the SDK cannot append/reapply channels or re-run its handshake.
            iface._sendToRadio(mesh_pb2.ToRadio(want_config_id=NODE_ONLY_NONCE))
        except Exception:
            with self._lock:
                if self._iface is iface:
                    self._deadline = None
                    self._state = 'unavailable'

    def observe(self, iface, raw):
        # The SDK has decoded/updated its cache before this reader callback.
        with self._lock:
            if self._iface is not iface or self._deadline is None:
                return False
        record = mesh_pb2.FromRadio()
        record.ParseFromString(raw)
        with self._lock:
            if self._iface is not iface or self._deadline is None:
                return False
            if record.WhichOneof('payload_variant') == 'config_complete_id' and record.config_complete_id == NODE_ONLY_NONCE:
                self._deadline = None
                self._state = 'current'
                return True
        return False

    @property
    def status(self):
        with self._lock:
            return self._state
