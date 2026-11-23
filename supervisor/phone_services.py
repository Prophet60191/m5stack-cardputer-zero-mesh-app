# SPDX-License-Identifier: GPL-3.0-or-later
"""Phone messaging/viewing via the official Meshtastic protocol and private VPN."""
from __future__ import annotations
import ipaddress
import hashlib
import json
import subprocess
import time
from google.protobuf.json_format import ParseDict, ParseError
from meshtastic.protobuf import mesh_pb2, portnums_pb2, config_pb2, module_config_pb2, admin_pb2
from phone_gateway import PhoneGateway


def vpn_ip(value):
    ip = ipaddress.ip_address(value)
    if ip not in ipaddress.ip_network('100.64.0.0/10') and ip not in ipaddress.ip_network('fd7a:115c:a1e0::/48'):
        raise ValueError('select a Tailscale peer')
    return str(ip)


class PhoneServices:
    def _init_phone(self):
        self._phone = PhoneGateway(self)
        self._phone_enabled = self._store.preference('phone.enabled', False) is True
        self._phone_peer = self._store.preference('phone.peer', '')
        self._phone_error = ''
        self._phone_seen = {}  # bounded, no automatic resend after reconnect

    def _tailscale_json(self, *args):
        try:
            result = subprocess.run(['/usr/bin/tailscale', *args], capture_output=True, timeout=3)
            if result.returncode or len(result.stdout) > 2 * 1024 * 1024:
                raise ValueError()
            value = json.loads(result.stdout)
            if not isinstance(value, dict): raise ValueError()
            return value
        except (OSError, ValueError, subprocess.TimeoutExpired):
            raise ValueError('Tailscale unavailable; install it and sign in on Cardputer') from None

    def _phone_network(self):
        status = self._tailscale_json('status', '--json')
        if status.get('BackendState') != 'Running':
            raise ValueError('Sign in to Tailscale on Cardputer first')
        peers = []
        peer_map = status.get('Peer') or {}
        if not isinstance(peer_map, dict): raise ValueError('Tailscale returned an invalid peer list')
        for peer in list(peer_map.values())[:256]:
            if not isinstance(peer, dict): continue
            ident = peer.get('ID')
            if not isinstance(ident, str) or not 0 < len(ident) <= 64: continue
            addresses = []
            for ip in (peer.get('TailscaleIPs') or []):
                try: addresses.append(vpn_ip(ip))
                except (ValueError, TypeError): pass
            if addresses:
                name = str(peer.get('HostName') or peer.get('DNSName') or 'Device')[:80]
                name = ''.join(c if c.isprintable() else ' ' for c in name)
                peers.append({'id': ident, 'name': name,
                              'addresses': addresses, 'online': peer.get('Online') is True})
        own = next((ip for ip in (status.get('TailscaleIPs') or []) if isinstance(ip, str) and ':' not in ip), '')
        if not own: raise ValueError('Cardputer has no Tailscale IPv4 address')
        own = vpn_ip(own)
        return own, peers

    def _phone_forwarder(self):
        config = self._tailscale_json('serve', 'status', '--json')
        # The app never configures Serve or Funnel, broadens VPN ACLs or grants
        # itself operator/root rights. Verify only our dedicated TCP endpoint.
        tcp = config.get('TCP') or {}
        if not isinstance(tcp, dict): raise ValueError('Invalid Tailscale Serve configuration')
        entry = tcp.get('4403') or {}
        if not isinstance(entry, dict): raise ValueError('Invalid Tailscale Serve endpoint')
        if (entry.get('TCPForward') != '127.0.0.1:47401' or entry.get('ProxyProtocol') != 2
                or entry.get('TerminateTLS') or entry.get('HTTPS') or entry.get('HTTP')):
            raise ValueError('Configure private Tailscale Serve TCP 4403 with PROXY v2; see Phone access guide')
        funnel = config.get('AllowFunnel') or {}
        if not isinstance(funnel, dict) or any(funnel.values()):
            raise ValueError('Public Funnel is enabled; disable it before phone access')

    def _phone_selected(self, peers):
        peer = next((p for p in peers if p['id'] == self._phone_peer), None)
        if not peer: raise ValueError('Select your phone; the saved VPN device is unavailable')
        return peer

    def _phone_payload(self):
        result = {'event': 'phone', 'enabled': self._phone_enabled, 'connected': self._phone.connected,
                  'peer': self._phone_peer, 'peers': [], 'endpoint': '', 'detail': '', 'ready': False}
        try:
            own, peers = self._phone_network()
            result.update(peers=peers, endpoint=own + ':4403')
            self._phone_forwarder()
            self._phone_selected(peers)
            result['ready'] = True
            result['detail'] = ('Access Off' if not self._phone_enabled else 'Connected' if self._phone.connected else 'Ready for phone') if self._radio == 'online' else 'Radio is off; turn it on on Cardputer'
            if self._phone_error: result['detail'] = self._phone_error
        except ValueError as exc:
            result['detail'] = str(exc)
        return result

    def _phone_set(self, req):
        enabled = req.get('enabled')
        if type(enabled) is not bool: raise ValueError('phone access requires boolean enabled')
        if enabled:
            _, peers = self._phone_network()
            self._phone_selected(peers)
            self._phone_forwarder()
        self._phone.stop()
        self._phone_enabled = False
        self._store.set_preference('phone.enabled', enabled)
        self._phone_enabled = enabled
        self._phone_resume()
        return self._phone_payload()

    def _phone_select(self, req):
        _, peers = self._phone_network()
        ident = req.get('peer')
        if not isinstance(ident, str) or not any(p['id'] == ident for p in peers):
            raise ValueError('select a device from your current Tailscale devices')
        self._phone.stop()
        self._phone_enabled = False
        self._store.set_preference('phone.enabled', False)
        self._store.set_preference('phone.peer', ident)
        self._phone_peer = ident
        return self._phone_payload()

    def _phone_resume(self):
        self._phone_error = ''
        if not self._phone_enabled or self._radio != 'online': return
        try:
            _, peers = self._phone_network()
            peer = self._phone_selected(peers)
            self._phone_forwarder()
            # iPhone connects to the displayed IPv4 endpoint, so Serve sends its
            # matching IPv4 source identity. IPv6 is handled by the parser too.
            address = next((ip for ip in peer['addresses'] if ':' not in ip), peer['addresses'][0])
            self._phone.start(address)
        except Exception:
            self._phone_error = 'Phone access unavailable; refresh Phone access settings'
            self._phone.stop()

    def _phone_context(self):
        return (self._iface, self._config_fingerprint(), self._phone_peer)

    def _phone_snapshot(self, nonce):
        iface = self._iface
        node = iface.localNode
        rows = []
        def add(field, value):
            record = mesh_pb2.FromRadio()
            getattr(record, field).CopyFrom(value)
            record.DiscardUnknownFields()  # newer firmware fields have not been qualified for export
            rows.append(record)
        add('my_info', iface.myInfo)
        if getattr(iface, 'metadata', None): add('metadata', iface.metadata)
        # Authentication authorizes channel access, not export of the node's
        # private signing key, Wi-Fi password or MQTT login/session credentials.
        for field in node.localConfig.DESCRIPTOR.fields:
            if field.name == 'version': continue
            value = config_pb2.Config()
            getattr(value, field.name).CopyFrom(getattr(node.localConfig, field.name))
            if field.name == 'security':
                value.security.ClearField('private_key'); value.security.ClearField('admin_key')
            if field.name == 'network': value.network.ClearField('wifi_psk')
            add('config', value)
        for field in node.moduleConfig.DESCRIPTOR.fields:
            if field.name == 'version': continue
            value = module_config_pb2.ModuleConfig()
            getattr(value, field.name).CopyFrom(getattr(node.moduleConfig, field.name))
            if field.name == 'mqtt':
                value.mqtt.ClearField('username'); value.mqtt.ClearField('password')
                value.mqtt.proxy_to_client_enabled = False
            add('moduleConfig', value)
        for channel in node.channels: add('channel', channel)
        # Python keeps derived lat/lon values alongside the real wire fields.
        # Ignore those extras; never synthesize a position from an invalid fix.
        nodes = list(getattr(iface, 'nodesByNum', {}).values())[:256]
        if not nodes: nodes = list((iface.nodes or {}).values())[:256]
        own = next((n for n in nodes if n.get('num') == iface.myInfo.my_node_num), None)
        if own is None:
            own = dict(iface.getMyNodeInfo() or {})
            own['num'] = iface.myInfo.my_node_num
            nodes.insert(0, own)
        for info in nodes[:256]:
            try: add('node_info', ParseDict(info, mesh_pb2.NodeInfo(), ignore_unknown_fields=True))
            except (ValueError, TypeError, ParseError): continue
        # Current Apple clients request configuration and the node database in
        # separate phases, using the firmware's reserved nonce values.
        if nonce == 69420:
            rows = [r for r in rows if not r.HasField('node_info')]
        elif nonce == 69421:
            rows = [r for r in rows if r.HasField('node_info')]
        rows.append(mesh_pb2.FromRadio(config_complete_id=nonce))
        return rows

    def phone_request(self, session, message):
        self._require_online()
        if not self._phone_enabled or not self._config_confirmed:
            session.close(); return
        if message.WhichOneof('payload_variant') == 'heartbeat':
            # Probe the real daemon over the one existing connection. Its actual
            # queue-status reply satisfies the Apple app's heartbeat watchdog;
            # an invented local receipt could hide a dead radio API.
            if session.ready:
                if session.context != self._phone_context(): session.close(); return
                clean = mesh_pb2.ToRadio()
                clean.heartbeat.nonce = message.heartbeat.nonce
                self._iface._sendToRadio(clean)
            return
        if message.WhichOneof('payload_variant') == 'want_config_id':
            now = time.monotonic()
            session.config_requests = [t for t in session.config_requests if now - t < 10]
            if len(session.config_requests) >= 6 or not message.want_config_id:
                session.close(); return
            session.pause()
            _, peers = self._phone_network()
            peer = self._phone_selected(peers)
            if session.peer not in peer['addresses']:
                session.close(); return
            self._phone_forwarder()
            session.config_requests.append(now)
            session.context = self._phone_context()
            session.complete(self._phone_snapshot(message.want_config_id))
            return
        if not session.ready or session.context != self._phone_context():
            session.close(); return
        p = message.packet
        if p.to == self._iface.myInfo.my_node_num and p.decoded.portnum == portnums_pb2.ADMIN_APP:
            # Apple's automatic clock hint is not a user setting operation.
            # Deny it without a recurring toast; do not change either clock or
            # claim that a write succeeded. All other admin writes stay visible.
            try:
                admin = admin_pb2.AdminMessage()
                admin.ParseFromString(p.decoded.payload)
                if admin.WhichOneof('payload_variant') == 'set_time_only':
                    session.reject(message, mesh_pb2.Routing.NOT_AUTHORIZED, quiet=True)
                    return
            except Exception: pass
        if (p.WhichOneof('payload_variant') != 'decoded' or p.decoded.portnum != portnums_pb2.TEXT_MESSAGE_APP
                or not p.id or p.to in (0, self._iface.myInfo.my_node_num)):
            session.reject(message, mesh_pb2.Routing.NOT_AUTHORIZED); return
        if not 0 < len(p.decoded.payload) <= 200:
            session.reject(message, mesh_pb2.Routing.TOO_LARGE); return
        try: text = p.decoded.payload.decode('utf-8', 'strict')
        except UnicodeError:
            session.reject(message, mesh_pb2.Routing.BAD_REQUEST); return
        # Copy only fields whose purpose has been checked. Never forward admin,
        # phone-position, MQTT, OTA, raw encrypted or arbitrary ToRadio records.
        safe = mesh_pb2.MeshPacket(id=p.id, channel=p.channel, to=p.to, want_ack=p.want_ack)
        safe.decoded.portnum = portnums_pb2.TEXT_MESSAGE_APP
        safe.decoded.payload = p.decoded.payload
        safe.decoded.reply_id = p.decoded.reply_id
        safe.decoded.emoji = int(bool(p.decoded.emoji))
        safe.priority = mesh_pb2.MeshPacket.RELIABLE
        dest = None if p.to == 0xffffffff else f'!{p.to:08x}'
        if p.pki_encrypted or p.public_key:
            if not dest or len(p.public_key) not in (0, 32):
                session.reject(message, mesh_pb2.Routing.BAD_REQUEST); return
            safe.pki_encrypted = p.pki_encrypted
            safe.public_key = p.public_key
        cid = f'phone:{p.id:08x}'
        identity = self._channel_identity(p.channel)
        payload_hash = hashlib.sha256(safe.SerializeToString(deterministic=True)).hexdigest()
        now = time.monotonic()
        self._phone_seen = {k: t for k, t in self._phone_seen.items() if now - t < 900}
        previous = cid in self._phone_seen
        with self._lock:
            old = next((m for m in self._messages if m.get('id') == cid and m.get('outgoing')), None)
        if previous or old:
            # Apple's explicit Retry reuses its packet ID. Permit a matching
            # failed message after a short cooldown, never a pending/completed
            # duplicate or an ID reused for different text/channel/recipient.
            if not (old and old.get('status') == 'failed' and old.get('channel_id') == identity
                    and old.get('phone_payload_hash') == payload_hash):
                session.reject(message, mesh_pb2.Routing.BAD_REQUEST); return
            if cid in self._phone_seen and now - self._phone_seen[cid] < 5:
                session.reject(message, mesh_pb2.Routing.RATE_LIMIT_EXCEEDED); return
        if p.id in getattr(self._iface, "responseHandlers", {}) or p.id in getattr(self._iface, "queue", {}):
            session.reject(message, mesh_pb2.Routing.BAD_REQUEST); return
        if len(self._phone_seen) >= 1024:
            session.reject(message, mesh_pb2.Routing.RATE_LIMIT_EXCEEDED); return
        if self._phone_seen and now - max(self._phone_seen.values()) < 1:
            session.reject(message, mesh_pb2.Routing.RATE_LIMIT_EXCEEDED); return
        req = {'channel': p.channel, 'channel_id': identity, 'dest': dest,
               'text': text, 'cid': cid, 'wantAck': p.want_ack}
        try:
            self._validate_target(req, dest)
        except ValueError:
            session.reject(message, mesh_pb2.Routing.NOT_AUTHORIZED if not self._iface.localNode.localConfig.lora.tx_enabled else mesh_pb2.Routing.NO_CHANNEL)
            return
        try:
            self._phone_seen[cid] = now
            self._send_text(req, phone_packet=safe)
        except Exception:
            session.reject(message, mesh_pb2.Routing.NO_INTERFACE)

    def _phone_frame(self, iface, raw):
        if iface is not self._iface or not self._phone_enabled: return
        record = mesh_pb2.FromRadio()
        record.ParseFromString(raw)
        kind = record.WhichOneof('payload_variant')
        if kind == 'packet':
            p = record.packet
            if p.WhichOneof('payload_variant') != 'decoded' or p.decoded.portnum == portnums_pb2.ADMIN_APP: return
        elif kind not in ('node_info', 'queueStatus'): return
        record.DiscardUnknownFields()
        self._phone.publish(record)

    def _phone_echo(self, packet):
        if not packet or not self._iface: return
        record = mesh_pb2.FromRadio()
        record.packet.CopyFrom(packet)
        setattr(record.packet, 'from', self._iface.myInfo.my_node_num)
        record.packet.rx_time = int(time.time())
        self._phone.publish(record)
