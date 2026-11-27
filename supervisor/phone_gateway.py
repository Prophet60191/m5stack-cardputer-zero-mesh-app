# SPDX-License-Identifier: GPL-3.0-or-later
"""Bounded Meshtastic PhoneAPI endpoint behind Tailscale Serve, never a radio proxy.

Only the supervisor talks to meshtasticd. The gateway parses protobufs and asks
that owner to perform allowed operations. A PROXY-v2 identity from the local VPN
forwarder must match the explicitly selected phone before any data is released.
"""
from __future__ import annotations
import ipaddress
import queue
import socket
import struct
import threading
import time
from google.protobuf.message import DecodeError
from meshtastic.protobuf import mesh_pb2, portnums_pb2

PORT = 47401
MAX_FRAME = 512
MAX_BUFFER = 256 * 1024
RESPONSE_LIFETIME = 90  # longer than app send/trace/admin waits; never replay RF
PROXY_MAGIC = b'\r\n\r\n\0\r\nQUIT\n'


def frame(message):
    raw = message.SerializeToString()
    if not 0 < len(raw) <= MAX_FRAME:
        raise ValueError('invalid PhoneAPI frame size')
    return b'\x94\xc3' + struct.pack('>H', len(raw)) + raw


def read_exact(conn, count, deadline):
    data = bytearray()
    while len(data) < count:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise TimeoutError('phone frame timed out')
        conn.settimeout(min(remaining, 1))
        try:
            chunk = conn.recv(count - len(data))
        except socket.timeout:
            continue
        if not chunk:
            raise EOFError()
        data.extend(chunk)
    return bytes(data)


def proxy_peer(conn):
    deadline = time.monotonic() + 3
    head = read_exact(conn, 16, deadline)
    if head[:12] != PROXY_MAGIC or head[12] != 0x21:
        raise ValueError('VPN proxy identity required')
    length = struct.unpack('>H', head[14:])[0]
    if length > 216:
        raise ValueError('oversized proxy identity')
    raw = read_exact(conn, length, deadline)
    if head[13] == 0x11 and length >= 12:
        return str(ipaddress.IPv4Address(raw[:4]))
    if head[13] == 0x21 and length >= 36:
        return str(ipaddress.IPv6Address(raw[:16]))
    raise ValueError('unsupported proxy identity')


class PhoneSession:
    def __init__(self, gateway, conn, peer):
        self.gateway, self.conn, self.peer = gateway, conn, peer
        self.closed = threading.Event()
        self.ready = False
        self.context = None
        self.config_requests = []
        self.queue = queue.Queue(maxsize=64)
        self._bytes = 0
        self._deferred = []
        self._deferred_bytes = 0
        self._lock = threading.Lock()
        self._pending = threading.BoundedSemaphore(4)
        self._output = conn.dup()
        self._output.settimeout(2)
        self._writer = threading.Thread(target=self._write, daemon=True)
        self._writer.start()

    def send(self, *messages):
        try:
            raw = b''.join(frame(m) for m in messages)
            with self._lock:
                if self.closed.is_set(): return
                if self._bytes + len(raw) > MAX_BUFFER:
                    raise queue.Full()
                self.queue.put_nowait(raw)
                self._bytes += len(raw)
        except (ValueError, queue.Full):
            self.close()

    def pause(self):
        with self._lock: self.ready = False

    def complete(self, messages):
        raw = b''.join(frame(m) for m in messages)
        try:
            with self._lock:
                if self.closed.is_set(): return
                raw += b''.join(self._deferred)
                if self._bytes + len(raw) > MAX_BUFFER: raise queue.Full()
                self.queue.put_nowait(raw)
                self._bytes += len(raw)
                self._deferred.clear(); self._deferred_bytes = 0
                self.ready = True
        except queue.Full:
            self.close()

    def publish(self, message):
        raw = frame(message)
        try:
            with self._lock:
                if self.closed.is_set(): return
                if self.ready:
                    if self._bytes + len(raw) > MAX_BUFFER: raise queue.Full()
                    self.queue.put_nowait(raw); self._bytes += len(raw)
                else:
                    if len(self._deferred) >= 64 or self._deferred_bytes + len(raw) > MAX_BUFFER:
                        raise queue.Full()
                    self._deferred.append(raw); self._deferred_bytes += len(raw)
        except queue.Full:
            self.close()

    def _write(self):
        while not self.closed.is_set():
            try: raw = self.queue.get(timeout=.2)
            except queue.Empty: continue
            try:
                self._output.sendall(raw)
            except OSError:
                self.close()
            finally:
                with self._lock: self._bytes -= len(raw)

    def close(self):
        if self.closed.is_set(): return
        self.closed.set()
        try: self.conn.shutdown(socket.SHUT_RDWR)
        except OSError: pass
        self.conn.close()
        self._output.close()

    def submit(self, message):
        if not self._pending.acquire(blocking=False):
            self.close()
            return
        def run():
            try:
                if self.gateway.current(self):
                    self.gateway.owner.phone_request(self, message)
            except Exception:
                # A phone failure must never unwind into the radio worker's
                # emergency teardown, or disclose an exception/config secret.
                try: self.reject(message, mesh_pb2.Routing.BAD_REQUEST)
                except Exception: self.close()
            finally:
                self._pending.release()
        try: self.gateway.owner._submit(run)
        except queue.Full:
            self._pending.release()
            self.close()

    def reject(self, request, reason, *, detail=None, quiet=False):
        if request.HasField('packet') and request.packet.id:
            response = mesh_pb2.FromRadio()
            p = response.packet
            p.to = getattr(getattr(self.gateway.owner._iface, 'myInfo', None), 'my_node_num', 0) if self.gateway.owner._iface else 0
            setattr(p, 'from', p.to)
            p.channel = request.packet.channel
            p.decoded.portnum = portnums_pb2.ROUTING_APP
            p.decoded.request_id = request.packet.id
            p.decoded.payload = mesh_pb2.Routing(error_reason=reason).SerializeToString()
            self.send(response)
        if quiet: return
        note = mesh_pb2.FromRadio()
        note.clientNotification.level = 40
        note.clientNotification.reply_id = request.packet.id if request.HasField('packet') else 0
        reasons = {
            mesh_pb2.Routing.NOT_AUTHORIZED: 'Change radio, location and channel settings on Cardputer. Phone access supports messages and viewing.',
            mesh_pb2.Routing.BAD_REQUEST: 'Request invalid or already submitted. Check Cardputer before retrying.',
            mesh_pb2.Routing.NO_INTERFACE: 'Radio unavailable or send outcome unknown. Check Cardputer before retrying.',
            mesh_pb2.Routing.TOO_LARGE: 'Message too long; shorten it to 200 UTF-8 bytes.',
            mesh_pb2.Routing.NO_CHANNEL: 'Channel unavailable or changed. Reconnect your phone.',
            mesh_pb2.Routing.RATE_LIMIT_EXCEEDED: 'Too many phone requests. Wait before trying again.'}
        note.clientNotification.message = detail or reasons.get(reason, 'Phone request rejected; check Cardputer.')
        self.send(note)


class PhoneGateway:
    def __init__(self, owner):
        self.owner = owner
        self._lock = threading.Lock()
        self._server = None
        self._session = None
        self._peer = ''
        self._candidates = set()

    def start(self, peer, *, port=PORT):
        self.stop()
        server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            server.bind(('127.0.0.1', port))
            server.listen(2)
            server.settimeout(.5)
        except Exception:
            server.close()
            raise
        with self._lock:
            self._server, self._peer = server, peer
        threading.Thread(target=self._accept, args=(server,), daemon=True).start()

    def current(self, session):
        with self._lock:
            return self._session is session and not session.closed.is_set() and self._server is not None

    def _accept(self, server):
        while True:
            with self._lock:
                if self._server is not server: return
            try: conn, _ = server.accept()
            except socket.timeout: continue
            except OSError: return
            with self._lock:
                if self._server is not server or len(self._candidates) >= 2:
                    conn.close()
                    continue
                self._candidates.add(conn)
            threading.Thread(target=self._serve, args=(server, conn), daemon=True).start()

    def _serve(self, server, conn):
        session = None
        try:
            peer = proxy_peer(conn)
            with self._lock:
                if self._server is not server or peer != self._peer:
                    return
                old = self._session
                session = PhoneSession(self, conn, peer)
                self._session = session
            if old: old.close()
            window, count, wake_bytes = time.monotonic(), 0, 0
            while not session.closed.is_set():
                # Idle is allowed, but a partial frame gets a short total deadline.
                first = read_exact(conn, 1, time.monotonic() + 900)
                # The official Python client sends a bounded UART wake preamble
                # even over TCP. Accept that exact preamble before configuration.
                while first == b'\xc3' and not session.ready and wake_bytes < 32:
                    wake_bytes += 1
                    first = read_exact(conn, 1, time.monotonic() + 5)
                header = first + read_exact(conn, 3, time.monotonic() + 5)
                if header[:2] != b'\x94\xc3': raise ValueError('invalid phone framing')
                size = struct.unpack('>H', header[2:])[0]
                if not 0 < size <= MAX_FRAME: raise ValueError('invalid phone frame length')
                raw = read_exact(conn, size, time.monotonic() + 5)
                now = time.monotonic()
                if now - window >= 1: window, count = now, 0
                count += 1
                if count > 12: raise ValueError('phone rate limit')
                message = mesh_pb2.ToRadio()
                message.ParseFromString(raw)
                kind = message.WhichOneof('payload_variant')
                if kind == 'disconnect': return
                if kind not in ('want_config_id', 'packet', 'heartbeat'): raise ValueError('unsupported phone command')
                session.submit(message)
        except (OSError, EOFError, ValueError, DecodeError):
            pass
        finally:
            if session: session.close()
            else: conn.close()
            with self._lock:
                self._candidates.discard(conn)
                if self._session is session: self._session = None

    def publish(self, message):
        with self._lock: session = self._session
        if session: session.publish(message)

    def stop(self):
        with self._lock:
            server, session = self._server, self._session
            self._server = self._session = None
            candidates = list(self._candidates)
        if server: server.close()
        if session: session.close()
        for conn in candidates:
            try: conn.shutdown(socket.SHUT_RDWR)
            except OSError: pass
            conn.close()

    @property
    def listening(self):
        with self._lock: return self._server is not None

    @property
    def connected(self):
        with self._lock:
            return self._session is not None and self._session.ready and not self._session.closed.is_set()


def observing_interface(callback, **kwargs):
    from meshtastic.tcp_interface import TCPInterface
    class ObservingInterface(TCPInterface):
        def __init__(self, **options):
            self._response_lock = threading.RLock()
            self._response_deadlines = {}
            super().__init__(**options)

        def _addResponseHandler(self, requestId, callback, ackPermitted=False):
            with self._response_lock:
                self.expire_responses()
                super()._addResponseHandler(requestId, callback, ackPermitted)
                self._response_deadlines[requestId] = (
                    time.monotonic() + RESPONSE_LIFETIME, self.responseHandlers[requestId])

        def expire_responses(self):
            # The pinned SDK retains unanswered callbacks indefinitely. Use
            # the same lock as decoding, including its get/pop/callback chain,
            # so expiry cannot race that chain or remove a replacement handler.
            with self._response_lock:
                now = time.monotonic()
                for request, (deadline, handler) in list(self._response_deadlines.items()):
                    if self.responseHandlers.get(request) is not handler:
                        self._response_deadlines.pop(request, None)
                    elif now >= deadline:
                        self.responseHandlers.pop(request, None)
                        self._response_deadlines.pop(request, None)

        def _handleFromRadio(self, raw):
            # Run the original decoder first. The callback sees the same wire
            # record, never mutated dictionaries or admin response objects.
            # Reader callbacks may take the state/store lock, never the
            # lifecycle lock. Expiry callers must not hold the state lock.
            with self._response_lock:
                super()._handleFromRadio(raw)
                try: callback(self, raw)
                except Exception: pass
    return ObservingInterface(**kwargs)
