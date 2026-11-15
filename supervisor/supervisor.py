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
../../docs/software/supervisor-protocol.md.

Why a supervisor:
- It keeps the privileged/hardware bits in one small, auditable place (the
  intent-not-mechanism rule): the UI asks for an action by name, never a command.
- It's plain Python, so it runs and is testable on the device on its own.

Pieces it owns:
- Cap power + RF bring-up  -> cap_lora.powered_cap()  (held while the radio is on)
- the meshtasticd daemon   -> a child process, started on radio.start, bound to a
                              FIXED loopback port (firewall-confined; see
                              firewall-setup.sh) and killed with the supervisor
                              via PR_SET_PDEATHSIG
- the mesh itself          -> the official `meshtastic` client over localhost TCP

Linux-only; runs ON the device with a venv python that has `meshtastic`:
    venv/bin/python supervisor/supervisor.py
"""

from __future__ import annotations

import json
import os
import queue
import re
import shutil
import signal
import socket
import subprocess
import threading
import time
from pathlib import Path

import cap_lora

HERE = Path(__file__).resolve().parent
CONFIG_YAML = HERE / "meshtasticd-cardputerzero.yaml"

# meshtasticd's TCP API binds all interfaces (upstream has no loopback option),
# so we pin it to ONE fixed port and confine that port to loopback with a
# firewall rule (firewall-setup.sh, installed by the package). Never expose it.
API_PORT = 47400

# Resource bounds so a hostile or stuck local client cannot exhaust memory.
MAX_CLIENTS = 8
MAX_LINE_BYTES = 64 * 1024        # one intent line
MAX_CLIENT_QUEUE = 512            # pending outbound events per client
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
    base = Path(run) if run and Path(run).is_dir() else Path(f"/tmp/cz-mesh-{os.getuid()}")
    d = (base / "cz-mesh") if run and Path(run).is_dir() else base
    d.mkdir(parents=True, exist_ok=True)
    try:
        os.chmod(d, 0o700)
    except OSError:
        pass
    return d


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
        self._writer = threading.Thread(target=self._run, daemon=True)
        self._writer.start()

    def send(self, obj: dict) -> None:
        if not self.alive:
            return
        line = (json.dumps(obj, separators=(",", ":")) + "\n").encode()
        try:
            self.q.put_nowait(line)
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
            self.conn.close()
        except OSError:
            pass


class Supervisor:
    def __init__(self) -> None:
        self._lock = threading.RLock()          # guards clients, state, messages, iface
        self._life_lock = threading.Lock()      # serializes radio start/stop as a whole
        self._clients: list[Client] = []
        self._radio = "off"                      # off | starting | online | error
        self._detail = ""
        self._messages: list[dict] = []          # authoritative session log
        self._retention = 0                       # per-thread cap (0 = keep all, up to MAX_MESSAGES)
        self._cap = None                         # the open powered_cap() context
        self._daemon: subprocess.Popen | None = None
        self._log = None
        self._iface = None                       # meshtastic TCPInterface
        self._port = 0
        self._state_dir = Path.home() / ".local/state/cardputerzero-radio"
        self._state_dir.mkdir(parents=True, exist_ok=True)
        self._stop = threading.Event()

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
                owner = info.get("user", {}).get("longName", "")
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
            for n in (self._iface.nodes or {}).values():
                u = n.get("user", {})
                dm = n.get("deviceMetrics", {})
                pos = n.get("position", {})
                nodes.append({
                    "id": u.get("id", ""),
                    "name": u.get("longName", ""),
                    "short": u.get("shortName", ""),
                    "snr": n.get("snr"),
                    "lastHeard": n.get("lastHeard"),
                    "hops": n.get("hopsAway"),
                    "self": n.get("num") == mynum,
                    "battery": dm.get("batteryLevel"),
                    "voltage": dm.get("voltage"),
                    "lat": pos.get("latitude"),
                    "lon": pos.get("longitude"),
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
                              "role": Role.Name(ch.role)})
        return {"event": "channels", "channels": chans}

    def _messages_payload(self, channel=None) -> dict:
        with self._lock:
            msgs = (self._messages if channel is None
                    else [m for m in self._messages if m["channel"] == channel])
            return {"event": "messages", "messages": msgs[-MAX_MESSAGES:]}

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
                "position": pos, "mqtt": mqtt}

    # --- history store (authoritative) ---------------------------------------
    def _record(self, msg: dict) -> None:
        with self._lock:
            self._messages.append(msg)
            if self._retention > 0:
                key = thread_key(msg)
                same = [m for m in self._messages if thread_key(m) == key]
                if len(same) > self._retention:
                    drop = set(id(m) for m in same[:-self._retention])
                    self._messages = [m for m in self._messages if id(m) not in drop]
            if len(self._messages) > MAX_MESSAGES:
                self._messages = self._messages[-MAX_MESSAGES:]

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
                import contextlib
                self._cap = contextlib.ExitStack()
                self._cap.enter_context(cap_lora.powered_cap())
            except cap_lora.CapError as exc:
                self._set_radio("error", f"Cap: {exc}")
                self._cap = None
                return
            try:
                self._launch_daemon()
                self._connect_mesh()
            except Exception as exc:  # noqa: BLE001
                self._set_radio("error", str(exc))
                self._teardown()
                return
            self._set_radio("online")
            threading.Thread(target=self._watch_daemon, daemon=True).start()
        self._broadcast(self._nodes())
        self._broadcast(self._channels())
        self._broadcast(self._device_payload())
        self._broadcast(self._config_payload())

    def radio_stop(self) -> None:
        with self._life_lock:
            self._teardown()
            self._set_radio("off")

    def _teardown(self) -> None:
        """Release iface, daemon and Cap. Caller holds _life_lock."""
        with self._lock:
            iface, daemon, cap, log = self._iface, self._daemon, self._cap, self._log
            self._iface = self._daemon = self._cap = self._log = None
            self._port = 0
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
        if daemon is not None and daemon.poll() is None:
            daemon.terminate()
            try:
                daemon.wait(timeout=3)
            except subprocess.TimeoutExpired:
                daemon.kill()
        if cap is not None:
            try:
                cap.close()
            except Exception:
                pass
        if log is not None:
            try:
                log.close()
            except Exception:
                pass

    def _watch_daemon(self) -> None:
        """If meshtasticd dies while we think we're online, surface it and power down."""
        while not self._stop.is_set():
            d = self._daemon
            if d is None or self._radio != "online":
                return
            if d.poll() is not None:
                self._set_radio("error", "radio daemon exited")
                self.radio_stop()
                return
            time.sleep(1.0)

    def _launch_daemon(self) -> None:
        self._port = API_PORT
        log_path = self._state_dir / "meshtasticd.log"
        env = dict(os.environ, HOME=str(self._state_dir), XDG_DATA_HOME=str(self._state_dir),
                   XDG_CONFIG_HOME=str(self._state_dir), XDG_STATE_HOME=str(self._state_dir))
        cmd = [find_meshtasticd(), "-c", str(CONFIG_YAML), "-p", str(self._port),
               f"--fsdir={self._state_dir}"]
        self._log = open(log_path, "wb")
        # start_new_session so a terminal signal doesn't hit the daemon directly;
        # we own its lifecycle (radio_stop, _watch_daemon, _reap_orphans) instead.
        self._daemon = subprocess.Popen(cmd, stdout=self._log, stderr=subprocess.STDOUT,
                                        env=env, start_new_session=True)
        deadline = time.monotonic() + 30
        while time.monotonic() < deadline:
            if self._daemon.poll() is not None:
                raise RuntimeError("meshtasticd exited during startup")
            text = log_path.read_text(errors="replace")
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
            snr = packet.get("rxSnr")
            hs, hl = packet.get("hopStart"), packet.get("hopLimit")
            hops = max(hs - hl, 0) if isinstance(hs, int) and isinstance(hl, int) else None
            msg = {
                "id": str(packet.get("id", "")),
                "from": f"!{frm:08x}",
                "fromName": self._name_for(frm),
                "to": "broadcast" if to == 0xFFFFFFFF else f"!{to:08x}",
                "channel": packet.get("channel", 0),
                "text": text,
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
        for n in (self._iface.nodes or {}).values():
            if n.get("num") == num:
                return n.get("user", {}).get("longName", "")
        return ""

    def _name_for_id(self, node_id: str) -> str:
        try:
            for n in (self._iface.nodes or {}).values():
                if n.get("user", {}).get("id") == node_id:
                    return n.get("user", {}).get("longName", "") or node_id
        except Exception:
            pass
        return node_id

    # --- intent dispatch -----------------------------------------------------
    def dispatch(self, client: Client, req: dict) -> None:
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
                threading.Thread(target=self.radio_start, daemon=True).start()
                reply({"event": "notice", "text": "starting radio"})
            elif intent == "radio.stop":
                threading.Thread(target=self.radio_stop, daemon=True).start()
                reply({"event": "notice", "text": "stopping radio"})
            elif intent == "node.list":
                reply(self._nodes())
            elif intent == "channel.list":
                reply(self._channels())
            elif intent == "message.list":
                reply(self._messages_payload(req.get("channel")))
            elif intent == "message.send":
                self._require_online()
                ch = int(req["channel"])
                text = str(req["text"])
                dest = req.get("dest") or None
                cid = req.get("cid")
                want_ack = bool(req.get("wantAck", True))
                self._require_channel(ch, dest)

                def onAckNak(resp, _cid=cid):   # the lib only delivers ACKs to this name
                    try:
                        routing = resp.get("decoded", {}).get("routing", {})
                        err = routing.get("errorReason", "NONE")
                        status = "delivered" if err in ("NONE", "", None, 0) else "failed"
                        hs, hl = resp.get("hopStart"), resp.get("hopLimit")
                        hops = (hs - hl) if isinstance(hs, int) and isinstance(hl, int) else None
                    except Exception:
                        status, hops = "delivered", None
                    if _cid:
                        ev = {"event": "delivery", "cid": _cid, "status": status}
                        if hops is not None:
                            ev["hops"] = max(hops, 0)
                        self._broadcast(ev)

                self._iface.sendText(text, destinationId=(dest or "^all"), wantAck=want_ack,
                                     channelIndex=ch, onResponse=(onAckNak if want_ack else None))
                self._record({"from": "self", "fromName": "me", "to": dest or "broadcast",
                              "channel": ch, "text": text, "time": int(time.time()),
                              "outgoing": True, "id": cid or "", "status": "sending"})
                reply({"event": "notice", "text": "sent"})
            elif intent == "message.clear":
                key = str(req["thread"])
                with self._lock:
                    self._messages = [m for m in self._messages if thread_key(m) != key]
                reply({"event": "notice", "text": "cleared"})
            elif intent == "message.delete":
                mid = str(req["id"])
                if mid:
                    with self._lock:
                        self._messages = [m for m in self._messages if m.get("id") != mid]
                reply({"event": "notice", "text": "deleted"})
            elif intent == "message.retention":
                self._retention = max(0, int(req.get("cap", 0)))
                reply({"event": "notice", "text": "retention set"})
            elif intent == "channel.add":
                self._require_online()
                added = self._add_channel(str(req["url"]))
                reply({"event": "notice", "text": f"imported {added} channel(s)"})
                self._broadcast(self._channels())
            elif intent == "position.send":
                self._require_online()
                ch = int(req.get("channel", 0))
                dest = req.get("dest") or None
                self._require_channel(ch, dest)
                lat, lon = self._my_position()
                if lat is None or lon is None:
                    reply({"event": "error", "text": "no position fix yet", "intent": intent})
                else:
                    text = f"\U0001F4CD {lat:.5f}, {lon:.5f}"
                    self._iface.sendText(text, destinationId=(dest or "^all"),
                                         wantAck=False, channelIndex=ch)
                    msg = {"from": "self", "fromName": "me", "to": dest or "broadcast",
                           "channel": ch, "text": text, "time": int(time.time()),
                           "outgoing": True, "id": "", "status": ""}
                    self._record(msg)
                    reply({"event": "notice", "text": "location sent"})
                    self._broadcast({"event": "message", "message": msg})
            elif intent == "position.request":
                self._require_online()
                self._iface.sendPosition(destinationId=str(req["node"]), wantResponse=True)
                reply({"event": "notice", "text": "position requested"})
            elif intent == "traceroute":
                self._require_online()
                node = str(req["node"])
                threading.Thread(target=self._run_traceroute, args=(node,), daemon=True).start()
                reply({"event": "notice", "text": "tracing route"})
            elif intent == "channel.share":
                self._require_online()
                name = str(req.get("name", "backhaul"))
                url = self._channel_share_url(name)
                if url:
                    reply({"event": "channelshare", "name": name, "url": url})
                else:
                    reply({"event": "error", "text": f"no channel named {name}", "intent": intent})
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
                self._set_config(str(req["path"]), req["value"])
                reply({"event": "notice", "text": "saved"})
                self._broadcast(self._config_payload())
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
            reply({"event": "error", "text": f"missing field {exc}", "intent": intent})
        except Exception as exc:  # noqa: BLE001 - surface any failure to the UI
            reply({"event": "error", "text": str(exc), "intent": intent})

    def _require_online(self) -> None:
        if self._iface is None or self._radio != "online":
            raise RuntimeError("radio is not on")

    def _require_channel(self, ch: int, dest) -> None:
        """Refuse a send on a channel that is not an enabled channel on this node.
        (No silent fallback to channel 0 / the public default.)"""
        if dest:   # a DM rides the primary with a destination id; that's fine
            return
        Role = type(self._iface.localNode.channels[0]).Role
        for c in self._iface.localNode.channels:
            if c.index == ch and c.role != Role.DISABLED:
                return
        raise ValueError(f"channel {ch} is not configured")

    def _add_channel(self, url: str) -> int:
        from base64 import urlsafe_b64decode
        from meshtastic.protobuf import apponly_pb2, channel_pb2
        m = re.fullmatch(r"https://meshtastic\.org/(?:e|d|v)/(?:\?add=true)?#([A-Za-z0-9_-]+={0,2})", url)
        if not m:
            raise ValueError("not a Meshtastic channel share URL")
        enc = m.group(1).rstrip("=")
        cs = apponly_pb2.ChannelSet.FromString(urlsafe_b64decode(enc + "=" * (-len(enc) % 4)))
        node = self._iface.localNode
        names = {ch.settings.name for ch in node.channels if ch.role}
        free = [ch for ch in node.channels if not ch.role and ch.index != 0]
        added = 0
        for settings in cs.settings:
            if settings.name and settings.name not in names and free:
                ch = free.pop(0)
                ch.settings.CopyFrom(settings)
                ch.role = channel_pb2.Channel.SECONDARY
                node.writeChannel(ch.index)
                names.add(settings.name)
                added += 1
        if added == 0:
            raise ValueError("no new named channel in the URL")
        return added

    def _my_position(self):
        try:
            info = self._iface.getMyNodeInfo() or {}
            pos = info.get("position", {}) or {}
            lat, lon = pos.get("latitude"), pos.get("longitude")
            if lat and lon:
                return float(lat), float(lon)
        except Exception:
            pass
        return None, None

    def _channel_index(self, name: str) -> int:
        try:
            for ch in self._iface.localNode.channels:
                if ch.settings.name == name and ch.role:
                    return ch.index
        except Exception:
            pass
        return 0

    def _run_traceroute(self, node: str) -> None:
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
            self._iface.sendData(r, destinationId=node,
                                 portNum=portnums_pb2.PortNum.TRACEROUTE_APP,
                                 wantResponse=True, onResponse=on_resp,
                                 channelIndex=self._channel_index("backhaul"), hopLimit=5)
        except Exception as exc:  # noqa: BLE001
            self._broadcast({"event": "traceroute", "node": node, "ok": False, "text": str(exc)})
            return
        if not done.wait(timeout=20):
            self._broadcast({"event": "traceroute", "node": node, "ok": False,
                             "text": "no response (timed out)"})
            return
        hops = [self._name_for(n) or f"!{n:08x}" for n in (result["route"] or [])]
        path = ["You"] + hops + [self._name_for_id(node)]
        self._broadcast({"event": "traceroute", "node": node, "ok": True, "path": path})

    def _channel_share_url(self, name: str):
        try:
            from base64 import urlsafe_b64encode
            from meshtastic.protobuf import apponly_pb2
            node = self._iface.localNode
            cs = apponly_pb2.ChannelSet()
            found = False
            for ch in node.channels:
                if ch.settings.name == name and ch.role:
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

    def _set_position_sharing(self, on: bool) -> None:
        """Enable/disable location sharing for real by setting each enabled
        channel's position precision (0 = off), not just the broadcast interval."""
        node = self._iface.localNode
        Role = type(node.channels[0]).Role
        precision = 16 if on else 0
        for ch in node.channels:
            if ch.role != Role.DISABLED:
                ch.settings.module_settings.position_precision = precision
                node.writeChannel(ch.index)
        node.localConfig.position.position_broadcast_smart_enabled = bool(on)
        node.localConfig.position.position_broadcast_secs = 900 if on else 0
        node.writeConfig("position")

    def _set_config(self, path: str, value) -> None:
        node = self._iface.localNode
        if path == "lora.modem_preset":
            node.localConfig.lora.use_preset = True
            node.localConfig.lora.modem_preset = \
                type(node.localConfig.lora).ModemPreset.Value(str(value))
            node.writeConfig("lora")
        elif path == "lora.hop_limit":
            node.localConfig.lora.hop_limit = int(value)
            node.writeConfig("lora")
        elif path == "position.enabled":
            self._set_position_sharing(bool(value))
        elif path == "mqtt.enabled":
            node.moduleConfig.mqtt.enabled = bool(value)
            node.writeConfig("mqtt")
        else:
            raise ValueError(f"unknown config path {path}")

    def _scan_qr(self):
        """Best-effort camera QR scan. Needs opencv + pyzbar + a camera."""
        try:
            import cv2
            from pyzbar.pyzbar import decode
        except Exception:
            return None
        cap = None
        try:
            cap = cv2.VideoCapture(0)
            for _ in range(40):
                ok, frame = cap.read()
                if not ok:
                    continue
                for sym in decode(frame):
                    data = sym.data.decode("utf-8", "replace")
                    if "meshtastic.org/e/" in data:
                        return data
        except Exception:
            return None
        finally:
            if cap is not None:
                cap.release()
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
                yield raw.decode("utf-8", "replace")
            if len(buf) > MAX_LINE_BYTES:
                return   # oversized line with no newline -> hang up

    def _handle_client(self, conn: socket.socket) -> None:
        client = Client(conn)
        with self._lock:
            if len(self._clients) >= MAX_CLIENTS:
                client.send({"event": "error", "text": "too many clients"})
                client.close()
                return
            self._clients.append(client)
        # Greet with a complete snapshot so a reconnecting UI is fully populated.
        client.send(self._status())
        if self._iface is not None and self._radio == "online":
            try:
                client.send(self._device_payload())
                client.send(self._config_payload())
                client.send(self._nodes())
                client.send(self._channels())
                client.send(self._messages_payload())
            except Exception:
                pass
        try:
            for line in self._read_lines(conn):
                line = line.strip()
                if not line:
                    continue
                try:
                    req = json.loads(line)
                except json.JSONDecodeError:
                    client.send({"event": "error", "text": "bad JSON"})
                    continue
                if not isinstance(req, dict):
                    client.send({"event": "error", "text": "intent must be a JSON object"})
                    continue
                self.dispatch(client, req)
        finally:
            self._drop_client(client)

    def _reap_orphans(self) -> None:
        """Kill any meshtasticd left running by a previous crashed supervisor
        (matched by our config path), so it can't keep using the powered radio.
        Covers the SIGKILL/OOM case where our own cleanup never ran."""
        try:
            out = subprocess.run(["pgrep", "-f", str(CONFIG_YAML)],
                                 capture_output=True, text=True)
            for pid in out.stdout.split():
                try:
                    os.kill(int(pid), signal.SIGTERM)
                except Exception:
                    pass
        except Exception:
            pass

    def run(self) -> None:
        self._reap_orphans()
        d = socket_dir()
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
                threading.Thread(target=self._handle_client, args=(conn,), daemon=True).start()
        finally:
            self.radio_stop()
            srv.close()
            if path.exists():
                path.unlink()
            print("supervisor stopped", flush=True)


if __name__ == "__main__":
    Supervisor().run()
