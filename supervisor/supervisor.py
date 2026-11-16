#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Jose Di Lenola
#
# Licensed GPL-3.0-or-later because this process links the GPL-3.0 `meshtastic`
# Python library. The C++ UI it serves is a separate process (MIT) that talks to
# it only over a local socket. meshtasticd and the meshtastic library are
# separate GPL-3.0 projects, installed by the user, not bundled here.
"""
Mesh supervisor: the one process that owns the radio and serves the UI.

The LVGL app never touches the Cap, meshtasticd, or the mesh protocol. It
connects to THIS process over a local Unix socket and exchanges newline-
delimited JSON — intents in ("radio.start", "message.send", ...), events out
("status", "nodes", "message", ...). The contract is in
../docs/supervisor-protocol.md.

Why a supervisor:
- It keeps the privileged/hardware bits in one small, auditable place (the
  intent-not-mechanism rule): the UI asks for an action by name, never a command.
- It's plain Python, so it runs and is testable on the device on its own.

Pieces it owns:
- Cap power + RF bring-up  -> cap_lora.powered_cap()  (held while the radio is on)
- the meshtasticd daemon   -> a child process, started on radio.start, bound to a
                              FIXED loopback port (firewall-confined; see
                              firewall-setup.sh) and killed with the supervisor
                              via a pipe-monitored guardian
- the mesh itself          -> the official `meshtastic` client over localhost TCP

Linux-only; runs ON the device with a venv python that has `meshtastic`:
    venv/bin/python supervisor/supervisor.py
"""

from __future__ import annotations

import fcntl
import hashlib
import stat
import sys
import uuid
import json
import math
import os
import queue
import re
import shutil
import signal
import socket
import subprocess
import threading
import tempfile
import time
from pathlib import Path


HERE = Path(__file__).resolve().parent
CONFIG_YAML = HERE / "meshtasticd-cardputerzero.yaml"

# meshtasticd's TCP API binds all interfaces (upstream has no loopback option),
# so we pin it to ONE fixed port and confine that port to loopback with a
# firewall rule (firewall-setup.sh, installed by the package). Never expose it.
API_PORT = 47400

# Resource bounds so a hostile or stuck local client cannot exhaust memory.
MAX_CLIENTS = 8
MAX_LINE_BYTES = 64 * 1024        # one intent line
MAX_CLIENT_QUEUE = 64            # pending outbound events per client
MAX_EVENT_BYTES = 2 * 1024 * 1024
MAX_QUEUE_BYTES = 4 * 1024 * 1024
MAX_MESSAGES = 2000               # global cap on the session message log


# --- the daemon lifecycle (shared shape with radio_up.py) --------------------
READY_RE = re.compile(r"sx126[12x].*(init success|init result:?\s*0|initiali[sz]ed)", re.I)
ERROR_RE = re.compile(r"(cannot open|resource busy|already in use|permission denied|"
                      r"no lora (radio|chip)|radio init failed|"
                      r"sx126[12x].*init.*(fail|result:?\s*-))", re.I)


def find_meshtasticd() -> str:
    found = shutil.which("meshtasticd")
    if found:
        return found
    for c in ("/usr/bin/meshtasticd", "/usr/sbin/meshtasticd"):
        if os.path.exists(c):
            return c
    raise FileNotFoundError("meshtasticd not installed")


def socket_dir() -> Path:
    """An owner-only (0700) directory for the control socket."""
    run = os.environ.get("XDG_RUNTIME_DIR")
    if run:
        base = Path(run)
        st = base.lstat()
        if not stat.S_ISDIR(st.st_mode) or st.st_uid != os.getuid() or st.st_mode & 0o077:
            raise PermissionError("XDG_RUNTIME_DIR must be a private, owned real directory")
        d = base / "cz-mesh"
    else:
        d = Path(f"/tmp/cz-mesh-{os.getuid()}")
    d.mkdir(mode=0o700, parents=True, exist_ok=True)
    st = d.lstat()
    if not stat.S_ISDIR(st.st_mode) or st.st_uid != os.getuid():
        raise PermissionError("control directory must be an owned, real directory")
    os.chmod(d, 0o700)
    return d


def bounded_text(value, limit):
    return str(value).encode("utf-8", "replace")[:limit].decode("utf-8", "ignore")


def finite_number(value):
    return value if isinstance(value, (int, float)) and math.isfinite(value) else None


def thread_key(msg: dict) -> str:
    """The conversation a message belongs to — channel-qualified for broadcasts so
    a public-channel broadcast never lands in a private channel's thread."""
    to = msg.get("to", "")
    if to in ("broadcast", "", None):
        return f"ch:{int(msg.get('channel', 0))}"
    peer = msg.get("to") if msg.get("outgoing") else msg.get("from")
    return f"dm:{peer}"


class Client:
    """One connected UI. Writes go through a bounded queue drained by a single
    writer thread, so a slow/stuck reader can never block the supervisor's state
    lock or interleave JSON records. A full queue means the client is too slow —
    we drop it rather than stall everyone."""

    def __init__(self, conn: socket.socket) -> None:
        self.conn = conn
        self.q: queue.Queue = queue.Queue(maxsize=MAX_CLIENT_QUEUE)
        self.alive = True
        self._bytes = 0
        self._qlock = threading.Lock()
        self._writer = threading.Thread(target=self._run, daemon=True)
        self._writer.start()

    def send(self, obj: dict) -> None:
        if not self.alive:
            return
        line = (json.dumps(obj, separators=(",", ":"), ensure_ascii=False, allow_nan=False) + "\n").encode()
        with self._qlock:
            if len(line) > MAX_EVENT_BYTES or self._bytes + len(line) > MAX_QUEUE_BYTES:
                self.close()
                return
            try:
                self.q.put_nowait(line)
                self._bytes += len(line)
            except queue.Full:
                self.close()

    def _run(self) -> None:
        while True:
            try:
                line = self.q.get(timeout=0.5)
            except queue.Empty:
                if not self.alive:
                    break
                continue
            if line is None:
                break
            with self._qlock:
                self._bytes -= len(line)
            try:
                self.conn.sendall(line)
            except OSError:
                break
        self.close()

    def close(self) -> None:
        if not self.alive:
            return
        self.alive = False
        try:
            self.q.put_nowait(None)
        except Exception:
            pass
        try:
            self.conn.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass
        try:
            self.conn.close()
        except OSError:
            pass


class Supervisor:
    def __init__(self) -> None:
        self._lock = threading.RLock()          # guards clients, state, messages, iface
        self._life_lock = threading.RLock()      # serializes radio start/stop as a whole
        self._clients: list[Client] = []
        self._radio = "off"                      # off | starting | online | error
        self._detail = ""
        self._messages: list[dict] = []          # authoritative session log
        self._retention = 0                       # per-thread cap (0 = keep all, up to MAX_MESSAGES)
        self._daemon: subprocess.Popen | None = None
        self._log = None
        self._iface = None                       # meshtastic TCPInterface
        self._port = 0
        self._state_dir = Path.home() / ".local/state/cardputerzero-radio"
        self._state_dir.mkdir(mode=0o700, parents=True, exist_ok=True)
        state = self._state_dir.lstat()
        if not stat.S_ISDIR(state.st_mode) or state.st_uid != os.getuid():
            raise PermissionError("radio state must be an owned, real directory")
        self._state_dir.chmod(0o700)
        self._stop = threading.Event()
        self._work = queue.Queue(maxsize=16)
        self._trace_slots = threading.BoundedSemaphore(2)
        self._attempts = {}
        self._config_confirmed = False
        self._worker = threading.Thread(target=self._work_loop, daemon=True)
        self._worker.start()

    def _work_loop(self):
        while not self._stop.is_set():
            try:
                fn = self._work.get(timeout=0.2)
            except queue.Empty:
                continue
            try:
                with self._life_lock:
                    fn()
            except Exception:
                # Dispatch reports errors to the requesting client; lifecycle
                # methods report radio errors themselves.
                with self._life_lock:
                    self._teardown()
                    self._set_radio("error", "radio operation failed")

    def _submit(self, fn):
        self._work.put_nowait(fn)  # bounded, including lifecycle/config operations

    def _check_system_daemon(self):
        """The dependency's system unit must not bypass our owned lifecycle/API.

        Installing meshtasticd may enable its default all-interface API service.
        Operators mask it before installing the dependency; the binary remains
        available to our unprivileged guardian.
        """
        unit = "meshtasticd.service"
        enabled = subprocess.run(["/usr/bin/systemctl", "is-enabled", unit],
                                 capture_output=True, text=True, timeout=3)
        active = subprocess.run(["/usr/bin/systemctl", "is-active", unit],
                                capture_output=True, text=True, timeout=3)
        safe = enabled.stdout.strip() in ("disabled", "masked", "masked-runtime", "not-found")
        if not safe and enabled.returncode != 4:
            raise RuntimeError("mask the system meshtasticd.service first; the app owns its radio daemon")
        if active.returncode not in (3, 4) or active.stdout.strip() in ("active", "activating", "deactivating", "reloading"):
            raise RuntimeError("system meshtasticd.service is running or cannot be verified; keep radio off")

    def _check_persisted_profile(self):
        """Validate before powering hardware: firmware can transmit at boot."""
        from meshtastic.protobuf import localonly_pb2, deviceonly_pb2
        try:
            config = localonly_pb2.LocalConfig.FromString((self._state_dir / "prefs/config.proto").read_bytes())
            channels = deviceonly_pb2.ChannelFile.FromString((self._state_dir / "prefs/channels.proto").read_bytes())
        except (OSError, ValueError) as exc:
            raise RuntimeError("prepare a safe radio profile before startup") from exc
        # Qualified meshtasticd uses preference schema 24. Empty/old protobufs
        # parse successfully in Python, but firmware discards them and installs
        # transmitting public defaults. Never treat their implicit false as RX.
        if (config.version != 24 or channels.version != 24 or
                not config.HasField("lora") or not 1 <= len(channels.channels) <= 8):
            raise RuntimeError("unsupported or incomplete radio profile; verify schema 24 preferences before power-on")
        enabled = [ch for ch in channels.channels if ch.role]
        if (not enabled or channels.channels[0].role != 1 or
                any(ch.index != i or ch.role not in (0, 1, 2) or
                    (i != 0 and ch.role == 1) or
                    (ch.role and (not self._valid_channel_key(ch.settings.psk) or len(ch.settings.name.encode("utf-8")) > 11))
                    for i, ch in enumerate(channels.channels))):
            raise RuntimeError("invalid radio channel profile; verify channel indices, roles and keys before power-on")
        # Public channels are supported. Factory profiles may enable location by
        # default, before we can connect: disable only unapproved PUBLIC sharing
        # on disk before firmware starts. Private settings remain unchanged.
        if not self._public_position_approved(channels.channels):
            changed = False
            for ch in enabled:
                if not self._private_channel(ch) and ch.settings.module_settings.position_precision:
                    ch.settings.module_settings.position_precision = 0
                    changed = True
            if changed:
                self._write_private_file(self._state_dir / "prefs/channels.proto", channels.SerializeToString())

    @staticmethod
    def _write_private_file(path, data):
        fd, temp = tempfile.mkstemp(prefix=".mesh-", dir=path.parent)
        try:
            with os.fdopen(fd, "wb") as out:
                out.write(data)
                out.flush()
                os.fsync(out.fileno())
            os.replace(temp, path)
            directory = os.open(path.parent, os.O_RDONLY | os.O_DIRECTORY)
            try: os.fsync(directory)
            finally: os.close(directory)
        finally:
            if os.path.exists(temp): os.unlink(temp)

    def _public_position_digest(self, channels):
        public = [(ch.index, ch.settings.name, bytes(ch.settings.psk).hex(),
                   ch.settings.module_settings.position_precision)
                  for ch in channels if ch.role and not self._private_channel(ch)
                  and ch.settings.module_settings.position_precision]
        return hashlib.sha256(json.dumps(public, separators=(",", ":")).encode()).hexdigest() if public else ""

    def _public_position_approved(self, channels):
        digest = self._public_position_digest(channels)
        if not digest: return True
        try:
            return (self._state_dir / "public-position-consent").read_text() == digest
        except OSError:
            return False

    def _check_firewall(self):
        helper = Path("/usr/lib/cardputerzero-mesh/firewall-setup.sh")
        if not helper.is_file() or helper.stat().st_uid != 0 or helper.stat().st_mode & 0o022:
            raise RuntimeError("install the root-owned mesh firewall helper first")
        result = subprocess.run(["/usr/bin/sudo", "-n", str(helper), "check"],
                                timeout=5, capture_output=True, text=True)
        if result.returncode:
            reason = bounded_text(result.stderr.strip(), 200)
            raise RuntimeError("radio firewall check failed: " + (reason or "verification refused"))

    # --- JSON line I/O (never touches a socket under self._lock) --------------
    def _send(self, client: Client, obj: dict) -> None:
        client.send(obj)

    def _broadcast(self, obj: dict) -> None:
        with self._lock:
            clients = list(self._clients)
        for c in clients:
            c.send(obj)

    # --- payload builders ----------------------------------------------------
    def _status(self) -> dict:
        region, owner = "UNSET", ""
        if self._iface is not None:
            try:
                node = self._iface.localNode
                region = type(node.localConfig.lora).RegionCode.Name(node.localConfig.lora.region)
                info = self._iface.getMyNodeInfo() or {}
                owner = bounded_text(info.get("user", {}).get("longName", ""), 80)
            except Exception:
                pass
        s = {"event": "status", "radio": self._radio, "region": region, "owner": owner}
        if self._port:
            s["port"] = self._port
        if self._detail:
            s["detail"] = self._detail
        return s

    def _nodes(self) -> dict:
        nodes = []
        if self._iface is not None:
            try:
                mynum = (self._iface.getMyNodeInfo() or {}).get("num")
            except Exception:
                mynum = None
            for n in list((self._iface.nodes or {}).values())[:256]:
                u = n.get("user", {})
                dm = n.get("deviceMetrics", {})
                pos = n.get("position", {})
                nodes.append({
                    "id": bounded_text(u.get("id", ""), 16),
                    "name": bounded_text(u.get("longName", ""), 80),
                    "short": bounded_text(u.get("shortName", ""), 16),
                    "snr": finite_number(n.get("snr")),
                    "lastHeard": finite_number(n.get("lastHeard")),
                    "hops": finite_number(n.get("hopsAway")),
                    "self": n.get("num") == mynum,
                    "battery": finite_number(dm.get("batteryLevel")),
                    "voltage": finite_number(dm.get("voltage")),
                    "lat": finite_number(pos.get("latitude")),
                    "lon": finite_number(pos.get("longitude")),
                })
        return {"event": "nodes", "nodes": nodes}

    def _channels(self) -> dict:
        chans = []
        if self._iface is not None:
            Role = type(self._iface.localNode.channels[0]).Role
            for ch in self._iface.localNode.channels:
                if ch.role == Role.DISABLED:
                    continue
                chans.append({"index": ch.index, "name": ch.settings.name,
                              "role": Role.Name(ch.role), "private": self._private_channel(ch),
                              "position_precision": ch.settings.module_settings.position_precision})
        return {"event": "channels", "channels": chans}

    def _messages_payload(self, channel=None) -> dict:
        with self._lock:
            msgs = (self._messages if channel is None
                    else [m for m in self._messages if m["channel"] == channel])
            return {"event": "messages", "messages": [dict(m) for m in msgs[-MAX_MESSAGES:]]}

    def _publish_history(self, client=None, request=None, channel=None):
        # Snapshot capture AND queue insertion share the delivery state lock.
        # Client.send only enqueues: no blocking network write occurs here.
        with self._lock:
            payload = self._messages_payload(channel)
            if request is not None:
                payload["id"] = request
            if client is None:
                self._broadcast(payload)
            else:
                self._send(client, payload)

    def _device_payload(self) -> dict:
        info = self._iface.getMyNodeInfo() or {}
        nid = info.get("user", {}).get("id", "")
        model = info.get("user", {}).get("hwModel", "")
        fw = ""
        md = getattr(self._iface, "metadata", None)
        if md is not None:
            fw = getattr(md, "firmware_version", "") or ""
        return {"event": "device", "node_id": nid, "firmware": fw, "model": str(model)}

    def _config_payload(self) -> dict:
        node = self._iface.localNode
        lora = node.localConfig.lora
        preset = type(lora).ModemPreset.Name(lora.modem_preset)
        pos = self._position_sharing_on()
        mqtt = False
        try:
            mqtt = bool(node.moduleConfig.mqtt.enabled)
        except Exception:
            pass
        return {"event": "config", "preset": preset, "hop": lora.hop_limit,
                "position": pos, "mqtt": mqtt, "confirmed": self._config_confirmed,
                "frequency_slot": lora.channel_num}

    # --- history store (authoritative) ---------------------------------------
    def _trim_history(self):
        counts = {}
        kept = []
        for m in reversed(self._messages):
            key = thread_key(m)
            counts[key] = counts.get(key, 0) + 1
            if self._retention == 0 or counts[key] <= self._retention:
                kept.append(m)
            if len(kept) == MAX_MESSAGES:
                break
        self._messages = list(reversed(kept))
        live = {m["id"] for m in self._messages if m.get("outgoing")}
        self._attempts = {k: v for k, v in self._attempts.items() if k in live}

    def _record(self, msg: dict, attempt=None) -> None:
        with self._lock:
            if attempt is not None:
                self._attempts[msg["id"]] = attempt
            # Retry updates the same bubble; duplicate RF packets are idempotent.
            self._messages = [m for m in self._messages
                              if not (m.get("id") == msg.get("id") and
                                      m.get("from") == msg.get("from"))]
            before = {(m.get("from"), m.get("id")) for m in self._messages}
            self._messages.append(msg)
            self._trim_history()
            removed = before - {(m.get("from"), m.get("id")) for m in self._messages}
        if removed:
            self._publish_history()

    def _delivery(self, cid, attempt, status, hops=None):
        with self._lock:
            if self._attempts.get(cid) != attempt:
                return  # late ACK from an older retry
            for m in self._messages:
                if m.get("outgoing") and m.get("id") == cid:
                    m["status"] = status
                    if hops is not None:
                        m["hops"] = max(hops, 0)
            self._attempts.pop(cid, None)
            ev = {"event": "delivery", "cid": cid, "status": status}
            if hops is not None:
                ev["hops"] = max(hops, 0)
            self._broadcast(ev)

    # --- radio lifecycle (serialized through _life_lock) ---------------------
    def _set_radio(self, state: str, detail: str = "") -> None:
        with self._lock:
            self._radio = state
            self._detail = detail
        self._broadcast(self._status())

    def radio_start(self) -> None:
        with self._life_lock:
            with self._lock:
                if self._radio in ("starting", "online"):
                    return
                self._radio = "starting"
                self._detail = "powering the Cap"
            self._broadcast(self._status())
            try:
                self._check_firewall()  # fail closed BEFORE power or daemon startup
                self._check_system_daemon()
                self._check_persisted_profile()
                self._launch_daemon()
                self._connect_mesh()
                # Startup snapshots were read from the daemon, not edited cache.
                self._config_confirmed = True
                if not self._public_position_approved(self._iface.localNode.channels):
                    raise RuntimeError("unconfirmed public position sharing; radio stopped")
            except Exception as exc:  # noqa: BLE001
                self._set_radio("error", str(exc))
                self._teardown()
                return
            self._set_radio("online")
            has_public = any(not self._private_channel(ch) for ch in self._iface.localNode.channels if ch.role)
            threading.Thread(target=self._watch_daemon, args=(self._daemon,), daemon=True).start()
        self._broadcast(self._nodes())
        self._broadcast(self._channels())
        self._broadcast(self._device_payload())
        self._broadcast(self._config_payload())
        if has_public:
            self._broadcast({"event": "notice", "text": "Public channels: messages and firmware identity/telemetry can be public."})

    def radio_stop(self) -> None:
        with self._life_lock:
            self._teardown()
            self._set_radio("off")

    def _teardown(self) -> None:
        """Release iface, daemon and Cap. Caller holds _life_lock."""
        with self._lock:
            iface, daemon, log = self._iface, self._daemon, self._log
            self._iface = self._daemon = self._log = None
            self._port = 0
            self._config_confirmed = False
        with self._lock:
            attempts = list(self._attempts.items())
        for cid, attempt in attempts:
            self._delivery(cid, attempt, "failed")
        if iface is not None:
            try:
                from pubsub import pub
                pub.unsubscribe(self._on_receive_text, "meshtastic.receive.text")
                pub.unsubscribe(self._on_receive_position, "meshtastic.receive.position")
            except Exception:
                pass
            try:
                iface.close()
            except Exception:
                pass
        if daemon is not None:
            if daemon.stdin:
                daemon.stdin.close()  # EOF: guardian stops child, THEN restores switches
            try:
                daemon.wait(timeout=5)
            except subprocess.TimeoutExpired:
                daemon.terminate()
                try:
                    daemon.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    daemon.kill()
                    daemon.wait()
                    # The service's ExecStopPost also covers simultaneous kills.
                    import radio_guardian
                    radio_guardian.cleanup(socket_dir())
            # A killed guardian leaves a recovery record. Its Linux child-death
            # binding stops the daemon even before we recover the switches.
            import radio_guardian
            radio_guardian.cleanup(socket_dir())
        if log is not None:
            try:
                log.close()
            except Exception:
                pass

    def _watch_daemon(self, owned_daemon) -> None:
        """If meshtasticd dies while we think we're online, surface it and power down."""
        while not self._stop.is_set():
            d = self._daemon
            if d is None or d is not owned_daemon or self._radio != "online":
                return
            try:
                self._check_firewall()
                if d.poll() is not None:
                    raise RuntimeError("radio daemon exited")
            except Exception:
                with self._life_lock:
                    if self._daemon is not owned_daemon:
                        return
                    self._teardown()
                    self._set_radio("error", "radio stopped: daemon or firewall unavailable")
                return
            with self._lock:
                expired = [(cid, token) for cid, token in self._attempts.items()
                           if time.monotonic() - token[1] > 60]
            for cid, token in expired:
                self._delivery(cid, token, "failed")
            self._stop.wait(2)

    def _launch_daemon(self) -> None:
        self._port = API_PORT
        log_path = self._state_dir / "meshtasticd.log"
        env = dict(os.environ, HOME=str(self._state_dir), XDG_DATA_HOME=str(self._state_dir),
                   XDG_CONFIG_HOME=str(self._state_dir), XDG_STATE_HOME=str(self._state_dir))
        cmd = [find_meshtasticd(), "-c", str(CONFIG_YAML), "-p", str(self._port),
               f"--fsdir={self._state_dir}"]
        self._log = open(log_path, "wb")
        # The guardian holds hardware and watches this pipe. Supervisor SIGKILL
        # closes its only writer; no unsafe preexec_fn in this threaded process.
        command = [sys.executable, str(HERE / "radio_guardian.py"),
                   "--runtime", str(socket_dir()), "--", *cmd]
        self._daemon = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=self._log,
                                        stderr=subprocess.STDOUT, env=env, close_fds=True)
        deadline = time.monotonic() + 30
        while time.monotonic() < deadline and not self._stop.is_set():
            if self._daemon.poll() is not None:
                raise RuntimeError("meshtasticd exited during startup")
            with open(log_path, "rb") as log:
                log.seek(max(0, log_path.stat().st_size - 128 * 1024))
                text = log.read(128 * 1024).decode(errors="replace")
            if READY_RE.search(text):
                return
            if ERROR_RE.search(text):
                raise RuntimeError("radio init failed (see meshtasticd.log)")
            time.sleep(0.25)
        raise TimeoutError("radio did not initialize in time")

    def _connect_mesh(self) -> None:
        from meshtastic.tcp_interface import TCPInterface
        from pubsub import pub
        self._iface = TCPInterface(hostname="127.0.0.1", portNumber=self._port, timeout=8)
        pub.subscribe(self._on_receive_text, "meshtastic.receive.text")
        pub.subscribe(self._on_receive_position, "meshtastic.receive.position")

    # --- incoming messages ---------------------------------------------------
    def _on_receive_text(self, packet, interface):  # meshtastic thread
        try:
            d = packet.get("decoded", {})
            text = d.get("text")
            if not text:
                return
            frm = packet.get("from", 0)
            to = packet.get("to", 0xFFFFFFFF)
            snr = finite_number(packet.get("rxSnr"))
            hs, hl = packet.get("hopStart"), packet.get("hopLimit")
            hops = max(hs - hl, 0) if isinstance(hs, int) and isinstance(hl, int) else None
            msg = {
                "id": f"rf:{frm:08x}:{packet.get('id', '')}",
                "from": f"!{frm:08x}",
                "fromName": bounded_text(self._name_for(frm), 80),
                "to": "broadcast" if to == 0xFFFFFFFF else f"!{to:08x}",
                "channel": packet.get("channel", 0),
                "text": bounded_text(text, 512),
                "time": packet.get("rxTime") or int(time.time()),
                "outgoing": False,
                "status": "",
                "snr": snr,
                "hops": hops,
            }
            self._record(msg)
            self._broadcast({"event": "message", "message": msg})
        except Exception:
            pass

    def _on_receive_position(self, packet, interface):  # meshtastic thread
        try:
            self._broadcast(self._nodes())
        except Exception:
            pass

    def _name_for(self, num: int) -> str:
        if self._iface is None:
            return ""
        for n in list((self._iface.nodes or {}).values())[:256]:
            if n.get("num") == num:
                return n.get("user", {}).get("longName", "")
        return ""

    def _name_for_id(self, node_id: str) -> str:
        try:
            for n in list((self._iface.nodes or {}).values())[:256]:
                if n.get("user", {}).get("id") == node_id:
                    return n.get("user", {}).get("longName", "") or node_id
        except Exception:
            pass
        return node_id

    # --- intent dispatch -----------------------------------------------------
    def dispatch(self, client: Client, req: dict) -> None:
        intent = req.get("intent")
        if not isinstance(intent, str) or len(intent) > 64:
            client.send({"event": "error", "text": "invalid intent"})
            return
        for key in ("id", "cid"):
            if key in req and (not isinstance(req[key], str) or len(req[key]) > 128):
                client.send({"event": "error", "text": "invalid request identifier"})
                return
        try:
            self._submit(lambda: self._dispatch(client, req))
        except queue.Full:
            client.send({"event": "error", "intent": intent, "id": req.get("id"),
                         "cid": req.get("cid"), "text": "radio busy; retry later"})

    def _dispatch(self, client: Client, req: dict) -> None:
        intent = req.get("intent", "")
        rid = req.get("id")

        def reply(obj):
            if rid is not None:
                obj = {**obj, "id": rid}
            self._send(client, obj)

        try:
            if intent == "status":
                reply(self._status())
            elif intent == "radio.start":
                self.radio_start()
                reply({"event": "notice", "text": "starting radio"})
            elif intent == "radio.stop":
                self.radio_stop()
                reply({"event": "notice", "text": "stopping radio"})
            elif intent == "node.list":
                reply(self._nodes())
            elif intent == "channel.list":
                reply(self._channels())
            elif intent == "message.list":
                self._publish_history(client, rid, req.get("channel"))
            elif intent == "message.send":
                self._require_online()
                ch = req["channel"]
                text = req["text"]
                if not isinstance(text, str) or not text or len(text.encode("utf-8")) > 200:
                    raise ValueError("message must contain 1..200 UTF-8 bytes")
                dest = req.get("dest")
                cid = req.get("cid")
                if not cid:
                    raise ValueError("message requires a unique cid")
                attempt = (uuid.uuid4().hex, time.monotonic())
                want_ack = req.get("wantAck", True)
                if type(want_ack) is not bool:
                    raise ValueError("wantAck must be boolean")
                self._require_channel(ch, dest)

                msg = {"from": "self", "fromName": "me", "to": dest or "broadcast",
                       "channel": ch, "text": text, "time": int(time.time()),
                       "outgoing": True, "id": cid, "status": "sending" if want_ack else ""}
                self._record(msg, attempt)
                # Publish the initial state BEFORE the library can invoke an
                # ACK callback. Otherwise a delayed initial echo can overwrite
                # an already delivered bubble at a connected client.
                self._broadcast({"event": "message", "message": dict(msg)})

                def onAckNak(resp):
                    routing = resp.get("decoded", {}).get("routing")
                    if not isinstance(routing, dict) or "errorReason" not in routing:
                        return  # unrelated/malformed response is never success
                    err = routing["errorReason"]
                    hs, hl = resp.get("hopStart"), resp.get("hopLimit")
                    hops = hs - hl if isinstance(hs, int) and isinstance(hl, int) else None
                    self._delivery(cid, attempt, "delivered" if err in ("NONE", 0) else "failed", hops)
                try:
                    self._iface.sendText(text, destinationId=(dest or "^all"), wantAck=want_ack,
                                         channelIndex=ch, onResponse=(onAckNak if want_ack else None))
                except Exception:
                    self._delivery(cid, attempt, "failed")
                    raise
                if not want_ack:
                    self._delivery(cid, attempt, "")
                reply({"event": "notice", "text": "sent"})
            elif intent == "message.clear":
                key = str(req["thread"])
                with self._lock:
                    self._messages = [m for m in self._messages if thread_key(m) != key]
                self._publish_history()
                reply({"event": "notice", "text": "cleared"})
            elif intent == "message.delete":
                mid = str(req["id"])
                if mid:
                    with self._lock:
                        self._messages = [m for m in self._messages if m.get("id") != mid]
                self._publish_history()
                reply({"event": "notice", "text": "deleted"})
            elif intent == "message.retention":
                with self._lock:
                    self._retention = max(0, min(MAX_MESSAGES, int(req.get("cap", 0))))
                    self._trim_history()
                self._publish_history()
                reply({"event": "notice", "text": "retention set"})
            elif intent == "channel.add":
                self._require_online()
                added = self._add_channel(str(req["url"]))
                reply({"event": "notice", "text": f"imported {added} channel(s)"})
                self._broadcast(self._channels())
            elif intent == "position.send":
                self._require_online()
                ch = req["channel"]
                dest = req.get("dest")
                self._require_channel(ch, dest)
                if not self._private_channel(self._iface.localNode.channels[ch]) and req.get("public_confirmed") is not True:
                    raise ValueError("confirm sending precise coordinates on a public channel")
                lat, lon = self._my_position()
                if lat is None or lon is None:
                    reply({"event": "error", "text": "no position fix yet", "intent": intent})
                else:
                    text = f"\U0001F4CD {lat:.5f}, {lon:.5f}"
                    self._iface.sendText(text, destinationId=(dest or "^all"),
                                         wantAck=False, channelIndex=ch)
                    msg = {"from": "self", "fromName": "me", "to": dest or "broadcast",
                           "channel": ch, "text": text, "time": int(time.time()),
                           "outgoing": True, "id": "pos:" + uuid.uuid4().hex, "status": ""}
                    self._record(msg)
                    reply({"event": "notice", "text": "location sent"})
                    self._broadcast({"event": "message", "message": msg})
            elif intent == "position.request":
                self._require_online()
                ch = req["channel"]
                self._require_channel(ch, req["node"])
                self._iface.sendPosition(destinationId=req["node"], wantResponse=True, channelIndex=ch)
                reply({"event": "notice", "text": "position requested"})
            elif intent == "traceroute":
                self._require_online()
                node, ch = req["node"], req["channel"]
                self._require_channel(ch, node)
                if not self._trace_slots.acquire(blocking=False):
                    raise RuntimeError("two traces already running")
                threading.Thread(target=self._trace_worker, args=(self._iface, node, ch, rid), daemon=True).start()
                reply({"event": "notice", "text": "tracing route"})
            elif intent == "channel.share":
                self._require_online()
                ch = req["channel"]
                self._require_channel(ch, None)
                name = self._iface.localNode.channels[ch].settings.name
                reply({"event": "channelshare", "channel": ch, "name": name,
                       "url": self._channel_share_url(ch)})
            elif intent == "region.set":
                self._require_online()
                self._set_region(str(req["region"]))
                reply({"event": "notice", "text": f"region set to {req['region']}"})
                self._broadcast(self._status())
            elif intent == "owner.set":
                self._require_online()
                self._iface.localNode.setOwner(long_name=str(req["name"]),
                                               short_name=req.get("short"))
                reply({"event": "notice", "text": "name saved"})
                self._broadcast(self._status())
                self._broadcast(self._device_payload())
            elif intent == "config.set":
                self._require_online()
                self._config_confirmed = False
                self._set_config(str(req["path"]), req["value"], req.get("channel"), req.get("public_confirmed", False))
                expected = self._config_fingerprint()
                self._verify_config()
                if self._config_fingerprint() != expected:
                    raise RuntimeError("radio settings readback differs from requested values")
                if req["path"] == "position.enabled":
                    self._write_private_file(self._state_dir / "public-position-consent",
                        self._public_position_digest(self._iface.localNode.channels).encode())
                self._config_confirmed = True
                self._broadcast(self._channels())
                reply(self._config_payload())
                self._broadcast(self._config_payload())
            elif intent == "config.get":
                self._require_online()
                self._config_confirmed = False
                self._verify_config()
                self._config_confirmed = True
                reply(self._config_payload())
            elif intent == "channel.scan":
                self._require_online()
                url = self._scan_qr()
                if url:
                    added = self._add_channel(url)
                    reply({"event": "notice", "text": f"imported {added} channel(s)"})
                    self._broadcast(self._channels())
                else:
                    reply({"event": "error", "text": "no QR code found", "intent": "channel.scan"})
            else:
                reply({"event": "error", "text": f"unknown intent: {intent}", "intent": intent})
        except KeyError as exc:
            reply({"event": "error", "text": f"missing field {exc}", "intent": intent, "cid": req.get("cid")})
        except (Exception, SystemExit) as exc:  # library validation may raise SystemExit
            if intent.startswith("config."):
                self._config_confirmed = False
                self._broadcast({"event": "config.unknown"})
                if intent == "config.set" and req.get("path") == "position.enabled":
                    with self._life_lock:
                        self._teardown()
                        self._set_radio("error", "position sharing was not confirmed; radio stopped")
            reply({"event": "error", "text": str(exc)[:256], "intent": intent, "cid": req.get("cid")})

    def _require_online(self) -> None:
        if self._iface is None or self._radio != "online":
            raise RuntimeError("radio is not on")

    @staticmethod
    def _valid_channel_key(key):
        return len(key) in (0, 1, 16, 32)

    @staticmethod
    def _private_channel(ch):
        # Shorthand keys and all expanded variants of the published default
        # key are public. A custom PSK is only private while its owner keeps it so.
        key = bytes(ch.settings.psk)
        default = bytes.fromhex("d4f1bb3a20290759f0bcffabcf4e6901")
        return bool(ch.role) and len(key) in (16, 32) and any(key) and not (
            len(key) == 16 and key[:15] == default[:15])

    def _require_channel(self, ch: int, dest) -> None:
        if type(ch) is not int or not 0 <= ch <= 7:
            raise ValueError("channel must be an integer index 0..7")
        if dest is not None and (not isinstance(dest, str) or
                not re.fullmatch(r"![0-9a-fA-F]{8}", dest) or
                dest.lower() in ("!00000000", "!ffffffff")):
            raise ValueError("destination must be a unicast node ID")
        for c in self._iface.localNode.channels:
            if c.index == ch and c.role in (1, 2) and self._valid_channel_key(c.settings.psk):
                return
        raise ValueError("select an enabled channel with a valid key")

    def _add_channel(self, url: str) -> int:
        from base64 import urlsafe_b64decode
        from meshtastic.protobuf import apponly_pb2, channel_pb2
        m = re.fullmatch(r"https://meshtastic\.org/(?:e|d|v)/(?:\?add=true)?#([A-Za-z0-9_-]+={0,2})", url)
        if not m:
            raise ValueError("not a Meshtastic channel share URL")
        enc = m.group(1).rstrip("=")
        cs = apponly_pb2.ChannelSet.FromString(urlsafe_b64decode(enc + "=" * (-len(enc) % 4)))
        if not cs.settings or len(cs.settings) > 8:
            raise ValueError("URL must contain 1..8 channels")
        for settings in cs.settings:
            if not self._valid_channel_key(settings.psk) or len(settings.name.encode("utf-8")) > 11:
                raise ValueError("invalid channel key or name (maximum 11 UTF-8 bytes)")
        node = self._iface.localNode
        identities = {(ch.settings.name, bytes(ch.settings.psk)) for ch in node.channels if ch.role}
        free = [ch for ch in node.channels if not ch.role and ch.index != 0]
        pending = []
        for settings in cs.settings:
            identity = (settings.name, bytes(settings.psk))
            if identity not in identities:
                identities.add(identity)
                pending.append(settings)
        if not pending:
            raise ValueError("no new channel in the URL")
        if len(pending) > len(free):
            raise ValueError("not enough free channel slots")
        for settings, ch in zip(pending, free):
            ch.settings.CopyFrom(settings)
            ch.settings.module_settings.position_precision = 0
            ch.settings.uplink_enabled = False
            ch.settings.downlink_enabled = False
            ch.role = channel_pb2.Channel.SECONDARY
            node.writeChannel(ch.index)
        expected = tuple(ch.SerializeToString() for ch in node.channels)
        self._config_confirmed = False
        try:
            self._verify_config()
            if tuple(ch.SerializeToString() for ch in node.channels) != expected:
                raise RuntimeError("channel readback mismatch")
            self._config_confirmed = True
        except Exception:
            self._teardown()
            self._set_radio("error", "channel import was not confirmed; radio stopped")
            raise
        return len(pending)

    def _my_position(self):
        try:
            info = self._iface.getMyNodeInfo() or {}
            pos = info.get("position", {}) or {}
            lat, lon = pos.get("latitude"), pos.get("longitude")
            if lat is not None and lon is not None:
                return float(lat), float(lon)
        except Exception:
            pass
        return None, None

    def _trace_worker(self, iface, node, channel, request):
        try:
            self._run_traceroute(iface, node, channel, request)
        finally:
            self._trace_slots.release()

    def _run_traceroute(self, iface, node: str, channel: int, request) -> None:
        from meshtastic.protobuf import mesh_pb2, portnums_pb2
        result = {"route": None}
        done = threading.Event()

        def on_resp(p):
            try:
                rd = mesh_pb2.RouteDiscovery()
                rd.ParseFromString(p["decoded"]["payload"])
                result["route"] = list(rd.route)
            except Exception:
                result["route"] = []
            done.set()

        try:
            r = mesh_pb2.RouteDiscovery()
            iface.sendData(r, destinationId=node,
                                 portNum=portnums_pb2.PortNum.TRACEROUTE_APP,
                                 wantResponse=True, onResponse=on_resp,
                                 channelIndex=channel, hopLimit=5)
        except Exception as exc:  # noqa: BLE001
            self._broadcast({"event": "traceroute", "node": node, "id": request, "ok": False, "text": str(exc)})
            return
        if not done.wait(timeout=20):
            self._broadcast({"event": "traceroute", "node": node, "id": request, "ok": False,
                             "text": "no response (timed out)"})
            return
        hops = [self._name_for(n) or f"!{n:08x}" for n in (result["route"] or [])]
        path = ["You"] + hops + [self._name_for_id(node)]
        self._broadcast({"event": "traceroute", "node": node, "id": request, "ok": True, "path": path})

    def _channel_share_url(self, index: int):
        try:
            from base64 import urlsafe_b64encode
            from meshtastic.protobuf import apponly_pb2
            node = self._iface.localNode
            cs = apponly_pb2.ChannelSet()
            found = False
            for ch in node.channels:
                if ch.index == index and ch.role in (1, 2) and self._valid_channel_key(ch.settings.psk):
                    cs.settings.append(ch.settings)
                    found = True
            if not found:
                return None
            cs.lora_config.CopyFrom(node.localConfig.lora)
            enc = urlsafe_b64encode(cs.SerializeToString()).decode().rstrip("=")
            return "https://meshtastic.org/e/#" + enc
        except Exception:
            return None

    def _set_region(self, region: str) -> None:
        from meshtastic.protobuf import config_pb2
        lora = self._iface.localNode.localConfig.lora
        lora.region = config_pb2.Config.LoRaConfig.RegionCode.Value(region)
        self._iface.localNode.writeConfig("lora")

    def _position_sharing_on(self) -> bool:
        """Position is actually shared only when a channel carries non-zero
        position precision. (position_broadcast_secs=0 means 'default', not off.)"""
        try:
            Role = type(self._iface.localNode.channels[0]).Role
            for ch in self._iface.localNode.channels:
                if ch.role != Role.DISABLED and ch.settings.module_settings.position_precision > 0:
                    return True
        except Exception:
            pass
        return False

    def _set_position_sharing(self, on: bool, channel=None, public_confirmed=False) -> None:
        node = self._iface.localNode
        if on:
            if channel is None:
                raise ValueError("enabling position requires an explicit channel")
            self._require_channel(channel, None)
            if not self._private_channel(node.channels[channel]) and public_confirmed is not True:
                raise ValueError("confirm automatic position sharing on a public channel")
        for ch in node.channels:
            if not ch.role:
                continue
            old = ch.settings.module_settings.position_precision
            # Only the selected channel; cap public automatic sharing at 15 bits.
            precision = ((old or 16) if self._private_channel(ch) else 15) if on and ch.index == channel else 0
            if old != precision:
                ch.settings.module_settings.position_precision = precision
                node.writeChannel(ch.index)
        node.localConfig.position.position_broadcast_smart_enabled = bool(on)
        node.localConfig.position.position_broadcast_secs = 900 if on else 0
        node.writeConfig("position")

    def _config_fingerprint(self):
        node = self._iface.localNode
        return (node.localConfig.lora.SerializeToString(),
                node.localConfig.position.SerializeToString(),
                node.moduleConfig.mqtt.SerializeToString(),
                tuple(ch.SerializeToString() for ch in node.channels))

    def _verify_config(self):
        """Read back real daemon state; writing the library cache is not an ACK.

        Uses the pinned 2.7.11 admin request/response API. A timeout leaves UI
        privacy state unknown. Local requests never transmit over RF.
        """
        from meshtastic.protobuf import admin_pb2
        node = self._iface.localNode

        def read(request, field):
            done, result = threading.Event(), []
            def response(packet):
                raw = packet.get("decoded", {}).get("admin", {}).get("raw")
                if raw is not None and raw.HasField(field):
                    result.append(getattr(raw, field))
                    done.set()
            node._sendAdmin(request, wantResponse=True, onResponse=response)
            deadline = time.monotonic() + 3
            while not done.wait(.1) and time.monotonic() < deadline and not self._stop.is_set():
                pass
            if not done.is_set() or not result or self._stop.is_set():
                raise TimeoutError("radio did not confirm settings; state unknown")
            return result[0]

        for name in ("lora", "position"):
            request = admin_pb2.AdminMessage()
            request.get_config_request = admin_pb2.AdminMessage.ConfigType.Value(name.upper() + "_CONFIG")
            config = read(request, "get_config_response")
            getattr(node.localConfig, name).CopyFrom(getattr(config, name))
        request = admin_pb2.AdminMessage()
        request.get_module_config_request = admin_pb2.AdminMessage.ModuleConfigType.Value("MQTT_CONFIG")
        config = read(request, "get_module_config_response")
        node.moduleConfig.mqtt.CopyFrom(config.mqtt)
        for index in range(len(node.channels)):
            request = admin_pb2.AdminMessage(get_channel_request=index + 1)
            ch = read(request, "get_channel_response")
            if ch.index != index:
                raise RuntimeError("channel readback mismatch")
            node.channels[index].CopyFrom(ch)

    def _set_config(self, path: str, value, channel=None, public_confirmed=False) -> None:
        node = self._iface.localNode
        if path in ("position.enabled", "mqtt.enabled") and type(value) is not bool:
            raise ValueError("privacy settings require boolean values")
        if path == "lora.modem_preset":
            node.localConfig.lora.use_preset = True
            node.localConfig.lora.modem_preset = \
                type(node.localConfig.lora).ModemPreset.Value(str(value))
            node.writeConfig("lora")
        elif path == "lora.hop_limit":
            if type(value) is not int or not 0 <= value <= 7:
                raise ValueError("hop limit must be 0..7")
            node.localConfig.lora.hop_limit = value
            node.writeConfig("lora")
        elif path == "lora.channel_num":
            if type(value) is not int or not 0 <= value <= 255:
                raise ValueError("frequency slot must be 0..255 (0 = Auto)")
            node.localConfig.lora.channel_num = value
            node.writeConfig("lora")
        elif path == "position.enabled":
            if type(public_confirmed) is not bool:
                raise ValueError("public position confirmation must be boolean")
            self._set_position_sharing(value, channel, public_confirmed)
        elif path == "mqtt.enabled":
            node.moduleConfig.mqtt.enabled = bool(value)
            node.writeConfig("mqtt")
        else:
            raise ValueError(f"unknown config path {path}")

    def _scan_qr(self):
        """A blocked camera must not strand the serialized radio worker."""
        try:
            result = subprocess.run([sys.executable, str(HERE / "qr_scan.py")],
                                    capture_output=True, text=True, timeout=10)
            value = result.stdout.strip()
            return value if result.returncode == 0 and len(value) <= 4096 else None
        except (OSError, subprocess.TimeoutExpired):
            return None

    # --- socket server -------------------------------------------------------
    def _drop_client(self, client: Client) -> None:
        with self._lock:
            if client in self._clients:
                self._clients.remove(client)
        client.close()

    def _read_lines(self, conn: socket.socket):
        """Yield complete UTF-8 lines, each capped at MAX_LINE_BYTES. A line that
        runs past the cap (or a peer that never sends a newline) drops the
        connection instead of buffering without bound."""
        buf = bytearray()
        while not self._stop.is_set():
            try:
                chunk = conn.recv(4096)
            except OSError:
                return
            if not chunk:
                return
            buf.extend(chunk)
            while b"\n" in buf:
                raw, _, rest = buf.partition(b"\n")
                buf = bytearray(rest)
                if len(raw) > MAX_LINE_BYTES:
                    return
                try:
                    yield raw.decode("utf-8", "strict")
                except UnicodeDecodeError:
                    return
            if len(buf) > MAX_LINE_BYTES:
                return   # oversized line with no newline -> hang up

    def _handle_client(self, conn: socket.socket, client: Client) -> None:
        # Capture/queue greeting config under the lifecycle lock too, so a
        # reconnect cannot enqueue old confirmed values after a stop/write.
        with self._life_lock:
            # Greet with a complete snapshot so a reconnecting UI is fully populated.
            client.send(self._status())
            if self._iface is not None and self._radio == "online":
                try:
                    client.send(self._device_payload())
                    client.send(self._config_payload())
                    client.send(self._nodes())
                    client.send(self._channels())
                    self._publish_history(client)
                except Exception:
                    pass
        try:
            for line in self._read_lines(conn):
                line = line.strip()
                if not line:
                    continue
                try:
                    req = json.loads(line, parse_constant=lambda _: (_ for _ in ()).throw(ValueError("non-finite JSON")))
                except (ValueError, RecursionError):
                    client.send({"event": "error", "text": "bad JSON"})
                    continue
                if not isinstance(req, dict):
                    client.send({"event": "error", "text": "intent must be a JSON object"})
                    continue
                # json.loads can accept deep structures; bound before dispatch.
                def shallow(value, depth=0):
                    if depth > 16:
                        return False
                    if isinstance(value, str):
                        try:
                            value.encode("utf-8", "strict")
                            return True
                        except UnicodeEncodeError:
                            return False
                    if isinstance(value, dict):
                        return all(shallow(k, depth + 1) and shallow(v, depth + 1)
                                   for k, v in value.items())
                    if isinstance(value, list):
                        return all(shallow(v, depth + 1) for v in value)
                    return True
                if not shallow(req):
                    return
                self.dispatch(client, req)
        finally:
            self._drop_client(client)

    def run(self) -> None:
        d = socket_dir()
        lease = open(d / "supervisor.lock", "a")
        try:
            fcntl.flock(lease, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            raise SystemExit("a supervisor already owns this runtime directory")
        # The guardian lease prevents recovery from touching a live radio.
        import radio_guardian
        radio_guardian.cleanup(d)
        path = d / "supervisor.sock"
        if path.exists():
            # Refuse to replace a socket we don't own (impersonation guard).
            try:
                st = path.stat()
                if st.st_uid != os.getuid():
                    raise SystemExit(f"refusing: {path} is owned by another user")
            except FileNotFoundError:
                pass
            path.unlink()
        srv = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        srv.bind(str(path))
        try:
            os.chmod(path, 0o600)
        except OSError:
            pass
        srv.listen(8)
        srv.settimeout(0.5)
        print(f"supervisor listening on {path}", flush=True)

        def shutdown(*_):
            self._stop.set()
        signal.signal(signal.SIGTERM, shutdown)
        signal.signal(signal.SIGINT, shutdown)

        try:
            while not self._stop.is_set():
                try:
                    conn, _ = srv.accept()
                except socket.timeout:
                    continue
                except OSError:
                    break
                with self._lock:
                    if len(self._clients) >= MAX_CLIENTS:
                        conn.close()
                        continue
                    client = Client(conn)
                    self._clients.append(client)
                threading.Thread(target=self._handle_client, args=(conn, client), daemon=True).start()
        finally:
            self.radio_stop()
            with self._lock:
                clients = list(self._clients)
            for client in clients:
                client.close()
            srv.close()
            if path.exists():
                path.unlink()
            print("supervisor stopped", flush=True)


if __name__ == "__main__":
    os.umask(0o077)
    Supervisor().run()
