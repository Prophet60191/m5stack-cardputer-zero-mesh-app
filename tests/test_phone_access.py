# SPDX-License-Identifier: MIT
"""Synthetic PhoneAPI/VPN boundaries; no device, VPN account or RF access."""
import copy
import json
from pathlib import Path
import queue
import socket
import struct
import sys
import tempfile
import threading
import time
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'supervisor'))
from meshtastic.protobuf import mesh_pb2, portnums_pb2, admin_pb2
from meshtastic.tcp_interface import TCPInterface
from pubsub import pub
from phone_gateway import frame, read_exact, proxy_peer, PROXY_MAGIC, MAX_FRAME
from phone_services import vpn_ip
import supervisor as mod
from test_supervisor import Radio, Client

PHONE = '100.100.0.2'

def proxy_header(ip=PHONE):
    address = socket.inet_pton(socket.AF_INET, ip)
    return PROXY_MAGIC + bytes([0x21, 0x11]) + struct.pack('>H', 12) + address + socket.inet_aton('100.100.0.1') + struct.pack('>HH', 12345, 4403)


def receive(conn):
    header = read_exact(conn, 4, time.monotonic() + 2)
    assert header[:2] == b'\x94\xc3'
    packet = mesh_pb2.FromRadio()
    packet.ParseFromString(read_exact(conn, struct.unpack('>H', header[2:])[0], time.monotonic() + 2))
    return packet


class Session:
    def __init__(self, owner):
        self.context = owner._phone_context()
        self.peer = PHONE
        self.ready = True
        self.config_requests = []
        self.records, self.errors = [], []
        self.closed = False
    def pause(self): self.ready = False
    def complete(self, records): self.send(*records); self.ready = True
    def send(self, *records): self.records.extend(copy.deepcopy(records))
    def reject(self, request, reason, **kwargs): self.errors.append(reason)
    def close(self): self.closed = True


class PhoneAccessTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        with patch.object(Path, 'home', return_value=Path(self.tmp.name)):
            self.s = mod.Supervisor()
        self.s._iface = Radio()
        self.s._iface.myInfo = mesh_pb2.MyNodeInfo(my_node_num=0x12345678)
        self.s._iface.metadata = mesh_pb2.DeviceMetadata(firmware_version='synthetic')
        self.s._iface.nodesByNum = {0x12345678: {'num':0x12345678, 'user':{'id':'!12345678','longName':'Synthetic'}}}
        self.s._iface.responseHandlers = {}
        self.s._iface.queue = {}
        self.s._radio = 'online'
        self.s._config_confirmed = True
        self.s._phone_enabled = True
        self.s._phone_peer = 'nSynthetic'
        self.s._iface._addResponseHandler = lambda ident, cb, **kw: self.s._iface.responseHandlers.update({ident:cb})
        self.sent = []
        def send(packet, destinationId, wantAck, **kwargs):
            p = copy.deepcopy(packet)
            p.to = 0xffffffff if destinationId == '^all' else int(destinationId[1:],16)
            p.want_ack = wantAck
            self.sent.append(p)
            record = mesh_pb2.FromRadio()
            record.queueStatus.free, record.queueStatus.maxlen = 16, 16
            record.queueStatus.mesh_packet_id = p.id
            self.s._phone_frame(self.s._iface, record.SerializeToString())
            return p
        self.s._iface._sendPacket = send
        self.heartbeats = []
        def heartbeat(message):
            self.heartbeats.append(copy.deepcopy(message))
            record = mesh_pb2.FromRadio()
            record.queueStatus.free, record.queueStatus.maxlen = 16, 16
            self.s._phone_frame(self.s._iface, record.SerializeToString())
        self.s._iface._sendToRadio = heartbeat
        self.network = {'BackendState':'Running','TailscaleIPs':['100.100.0.1'],
                        'Peer':{'synthetic':{'ID':'nSynthetic','HostName':'Test phone','TailscaleIPs':[PHONE],'Online':True}}}
        self.forward = {'TCP':{'4403':{'TCPForward':'127.0.0.1:47401','ProxyProtocol':2}}}
        self.s._tailscale_json = lambda *args: copy.deepcopy(self.forward if args[0] == 'serve' else self.network)
        self.c = Client(); self.s._clients = [self.c]
        self.connections = []

    def tearDown(self):
        self.s._phone.stop()
        for conn in self.connections:
            try: conn.shutdown(socket.SHUT_RDWR)
            except OSError: pass
            conn.close()
        self.s._stop.set(); self.s._stop_imports(); self.s._worker.join(2)
        self.s._store.close(); self.s._state_lease.close(); self.tmp.cleanup()

    def request(self, ident=100, channel=1, text=b'Synthetic phone message', dest=0xffffffff):
        request = mesh_pb2.ToRadio()
        request.packet.id, request.packet.channel, request.packet.to = ident, channel, dest
        request.packet.want_ack = True
        request.packet.decoded.portnum = portnums_pb2.TEXT_MESSAGE_APP
        request.packet.decoded.payload = text
        return request

    def connect(self, ip=PHONE):
        if self.s._phone._server is None: self.s._phone.start(PHONE, port=0)
        conn = socket.create_connection(self.s._phone._server.getsockname(), timeout=2)
        self.connections.append(conn); conn.sendall(proxy_header(ip)); return conn

    def handshake(self, conn):
        conn.sendall(frame(mesh_pb2.ToRadio(want_config_id=123)))
        records = []
        while True:
            record = receive(conn); records.append(record)
            if record.config_complete_id == 123: return records

    def test_snapshot_scrubs_host_secrets_but_keeps_authorized_channels(self):
        node = self.s._iface.localNode
        node.localConfig.security.private_key = b'synthetic-private-key'
        node.localConfig.security.admin_key.append(b'synthetic-admin-key')
        node.localConfig.network.wifi_psk = 'synthetic-wifi-password'
        node.moduleConfig.mqtt.password = 'synthetic-mqtt-password'
        node.moduleConfig.mqtt.username = 'synthetic-mqtt-user'
        node.moduleConfig.mqtt.proxy_to_client_enabled = True
        original = node.localConfig.SerializeToString()
        records = self.s._phone_snapshot(5)
        wire = b''.join(frame(r) for r in records)
        for secret in (b'synthetic-private-key',b'synthetic-admin-key',b'synthetic-wifi-password',b'synthetic-mqtt-password',b'synthetic-mqtt-user'):
            self.assertNotIn(secret, wire)
        self.assertEqual(node.localConfig.SerializeToString(), original)
        self.assertTrue(any(r.HasField('channel') and r.channel.index == 1 and r.channel.settings.psk for r in records))
        self.assertEqual(records[-1].config_complete_id, 5)

    def test_handshake_real_wire_and_split_headers(self):
        conn = self.connect()
        request = frame(mesh_pb2.ToRadio(want_config_id=55))
        for byte in request: conn.sendall(bytes([byte]))
        found = []
        while True:
            record = receive(conn); found.append(record)
            if record.config_complete_id == 55: break
        self.assertTrue(any(r.HasField('my_info') for r in found))
        self.assertTrue(self.s._phone.connected)
        self.assertEqual(self.sent, [])

    def test_apple_two_phase_configuration_and_database_handshake(self):
        conn = self.connect()
        for nonce in (69420, 69421):
            conn.sendall(frame(mesh_pb2.ToRadio(want_config_id=nonce)))
            records = []
            while True:
                record=receive(conn); records.append(record)
                if record.config_complete_id == nonce: break
            if nonce == 69420:
                self.assertTrue(any(r.HasField('channel') for r in records))
                self.assertFalse(any(r.HasField('node_info') for r in records))
            else:
                self.assertTrue(any(r.HasField('node_info') for r in records))
                self.assertFalse(any(r.HasField('config') for r in records))
        self.assertTrue(self.s._phone.connected)

    def test_apple_heartbeat_probes_daemon_and_returns_actual_status(self):
        conn = self.connect(); self.handshake(conn)
        request = mesh_pb2.ToRadio(); request.heartbeat.nonce = 42
        conn.sendall(frame(request))
        self.assertTrue(receive(conn).HasField('queueStatus'))
        self.assertEqual(self.heartbeats[0].heartbeat.nonce, 42)
        self.assertFalse(self.sent)

    def test_phone_request_error_never_enters_radio_emergency_teardown(self):
        conn = self.connect(); self.handshake(conn)
        original = self.s.phone_request
        self.s.phone_request = lambda *args: (_ for _ in ()).throw(RuntimeError('synthetic failure'))
        conn.sendall(frame(self.request()))
        record = receive(conn)
        self.assertEqual(record.packet.decoded.portnum, portnums_pb2.ROUTING_APP)
        self.assertEqual(self.s._radio, 'online')
        self.s.phone_request = original

    def test_live_packet_during_handshake_is_buffered_after_config(self):
        entered, release = threading.Event(), threading.Event()
        network = self.s._phone_network
        def blocked():
            entered.set(); release.wait(2); return network()
        self.s._phone_network = blocked
        conn = self.connect(); conn.sendall(frame(mesh_pb2.ToRadio(want_config_id=123)))
        self.assertTrue(entered.wait(1))
        record=mesh_pb2.FromRadio(); record.packet.decoded.portnum=portnums_pb2.TEXT_MESSAGE_APP
        record.packet.decoded.payload=b'Synthetic during handshake'
        self.s._phone_frame(self.s._iface,record.SerializeToString()); release.set()
        while receive(conn).config_complete_id != 123: pass
        self.assertEqual(receive(conn).packet.decoded.payload,b'Synthetic during handshake')

    def test_partial_frame_has_total_deadline_and_radio_survives(self):
        conn=self.connect(); conn.sendall(b'\x94\xc3\0\2\x08')
        conn.settimeout(7)
        self.assertEqual(conn.recv(1),b'')
        self.assertEqual(self.s._radio,'online')
        self.assertFalse(self.sent)

    def test_slow_reader_is_dropped_without_blocking_owner(self):
        conn=self.connect(); self.handshake(conn)
        session=self.s._phone._session
        record=mesh_pb2.FromRadio(); record.packet.decoded.portnum=portnums_pb2.TEXT_MESSAGE_APP
        record.packet.decoded.payload=b'x'*450
        for _ in range(10000):
            self.s._phone_frame(self.s._iface,record.SerializeToString())
            if session.closed.is_set(): break
        self.assertTrue(session.closed.is_set())
        self.assertEqual(self.s._radio,'online')
        result=Client(); self.s._dispatch(result,{'intent':'status'})
        self.assertEqual(result.events[0]['radio'],'online')

    def test_repeated_reconnect_and_heartbeat_do_not_replace_radio(self):
        physical=self.s._iface
        for _ in range(25):
            conn=self.connect(); self.handshake(conn)
            request=mesh_pb2.ToRadio(); request.heartbeat.nonce=45
            conn.sendall(frame(request)); self.assertTrue(receive(conn).HasField('queueStatus'))
            conn.sendall(frame(mesh_pb2.ToRadio(disconnect=True))); conn.close()
            time.sleep(.01)
        self.assertIs(self.s._iface,physical)
        self.assertEqual(len(self.heartbeats),25)
        self.assertFalse(self.sent)

    def test_nonselected_peer_receives_no_data(self):
        conn = self.connect('100.100.0.3')
        conn.sendall(frame(mesh_pb2.ToRadio(want_config_id=1)))
        try: data = conn.recv(4)
        except ConnectionResetError: data = b''
        self.assertEqual(data, b'')
        self.assertEqual(self.s._radio, 'online')

    def test_missing_proxy_header_and_malformed_wire_cannot_touch_radio(self):
        self.s._phone.start(PHONE, port=0)
        for payload in (b'no proxy header!', PROXY_MAGIC + b'\x20\x11\0\0', proxy_header() + b'\x94\xc3\xff\xff', proxy_header() + b'\x94\xc3\0\1\xff'):
            conn = socket.create_connection(self.s._phone._server.getsockname(), timeout=2)
            self.connections.append(conn); conn.sendall(payload); conn.shutdown(socket.SHUT_WR)
            try: data = conn.recv(4)
            except ConnectionResetError: data = b''
            self.assertEqual(data, b'')
        self.assertEqual(self.s._radio, 'online'); self.assertFalse(self.sent)

    def test_phone_send_uses_shared_history_identity_and_real_ack_id(self):
        session = Session(self.s); self.s.phone_request(session, self.request())
        self.assertEqual(len(self.sent), 1)
        self.assertEqual(self.sent[0].id, 100)
        self.assertEqual(self.s._messages[-1]['id'], 'phone:00000064')
        self.assertEqual(self.s._messages[-1]['channel_id'], self.s._channel_identity(1))
        self.s._iface.responseHandlers[100]({'from':0x87654321,'decoded':{'routing':{'errorReason':'NONE'}}})
        self.assertEqual(self.s._messages[-1]['status'], 'mesh')
        self.assertTrue(any(e['event'] == 'message' for e in self.c.events))

    def test_recipient_ack_and_failure_remain_truthful(self):
        session = Session(self.s)
        self.s.phone_request(session, self.request(dest=0x87654321))
        callback = self.s._iface.responseHandlers[100]
        callback({'from':0x87654321,'decoded':{'routing':{'errorReason':'NONE'}}})
        self.assertEqual(self.s._messages[-1]['status'], 'delivered')
        callback({'decoded':{'routing':{'errorReason':'TIMEOUT'}}})
        self.assertEqual(self.s._messages[-1]['status'], 'delivered')  # late replies cannot reverse a completed send
        self.s._phone_seen.clear()
        self.s.phone_request(session, self.request(ident=101,dest=0x87654321))
        self.s._iface.responseHandlers[101]({'decoded':{'routing':{'errorReason':'TIMEOUT'}}})
        self.assertEqual(self.s._messages[-1]['status'], 'failed')

    def test_receive_only_invalid_channel_and_stale_setup_never_send(self):
        for change in ('tx', 'channel', 'identity'):
            session = Session(self.s)
            if change == 'tx': self.s._iface.localNode.localConfig.lora.tx_enabled = False; session.context = self.s._phone_context()
            if change == 'channel': self.s._iface.localNode.channels[1].role = 0; session.context = self.s._phone_context()
            if change == 'identity': self.s._iface.localNode.channels[1].settings.name = 'Changed'
            self.s.phone_request(session, self.request())
            self.assertFalse(self.sent)
            self.assertTrue(session.errors or session.closed)
            self.s._iface.localNode.localConfig.lora.tx_enabled = True
            self.s._iface.localNode.channels[1].role = 2

    def test_admin_position_mqtt_and_opaque_payload_are_rejected(self):
        for port in (portnums_pb2.ADMIN_APP, portnums_pb2.POSITION_APP, portnums_pb2.TRACEROUTE_APP):
            session = Session(self.s); request = self.request(); request.packet.decoded.portnum = port
            self.s.phone_request(session, request)
            self.assertIn(mesh_pb2.Routing.NOT_AUTHORIZED, session.errors)
        session = Session(self.s); request = self.request(); request.packet.encrypted = b'arbitrary'
        self.s.phone_request(session, request)
        self.assertFalse(self.sent)

    def test_invalid_text_and_destinations_are_rejected(self):
        for payload in (b'', b'x'*201, b'\xff'):
            session = Session(self.s); self.s.phone_request(session, self.request(text=payload))
            self.assertTrue(session.errors)
        for dest in (0,0x12345678):
            session = Session(self.s); self.s.phone_request(session,self.request(dest=dest)); self.assertTrue(session.errors)
        self.assertFalse(self.sent)

    def test_duplicate_frames_across_new_sessions_are_not_retransmitted(self):
        self.s.phone_request(Session(self.s),self.request())
        self.s.phone_request(Session(self.s),self.request())
        self.assertEqual(len(self.sent),1)
        self.s._phone_seen.clear()  # durable history still protects a restart
        self.s.phone_request(Session(self.s),self.request())
        self.assertEqual(len(self.sent),1)

    def test_apple_retry_reuses_id_only_for_matching_failed_message(self):
        session=Session(self.s); request=self.request()
        self.s.phone_request(session,request)
        self.s._iface.responseHandlers.pop(100)({'decoded':{'routing':{'errorReason':'TIMEOUT'}}})
        self.s._phone_seen['phone:00000064'] -= 6
        self.s.phone_request(session,request)
        self.assertEqual(len(self.sent),2)
        self.assertEqual(self.sent[0].id,self.sent[1].id)
        self.assertEqual(len(self.s._messages),1)
        self.s._iface.responseHandlers.pop(100)({'decoded':{'routing':{'errorReason':'TIMEOUT'}}})
        self.s._phone_seen['phone:00000064'] -= 6
        self.s.phone_request(session,self.request(text=b'Changed payload'))
        self.assertEqual(len(self.sent),2)
        self.assertEqual(session.errors[-1],mesh_pb2.Routing.BAD_REQUEST)

    def test_failed_retry_has_cooldown_and_revalidates_channel(self):
        session=Session(self.s); request=self.request()
        self.s.phone_request(session,request)
        self.s._iface.responseHandlers.pop(100)({'decoded':{'routing':{'errorReason':'NO_ROUTE'}}})
        self.s.phone_request(session,request)
        self.assertEqual(session.errors[-1],mesh_pb2.Routing.RATE_LIMIT_EXCEEDED)
        self.s._phone_seen['phone:00000064'] -= 6
        self.s._iface.localNode.channels[1].settings.psk=b'x'*32
        session.context=self.s._phone_context()
        self.s.phone_request(session,request)
        self.assertEqual(len(self.sent),1)

    def test_collision_with_lockon_request_cannot_replace_ack_callback(self):
        self.s._iface.responseHandlers[100] = 'existing callback'
        self.s.phone_request(Session(self.s), self.request())
        self.assertFalse(self.sent)
        self.assertEqual(self.s._iface.responseHandlers[100], 'existing callback')

    def test_reused_vpn_address_with_different_node_identity_is_denied(self):
        session = Session(self.s); session.ready = False
        self.network['Peer']['synthetic']['ID'] = 'differentNode'
        with self.assertRaises(ValueError): self.s.phone_request(session, mesh_pb2.ToRadio(want_config_id=1))
        self.assertFalse(session.records)

    def test_changed_vpn_address_does_not_authorize_old_address(self):
        session = Session(self.s); session.ready = False
        self.network['Peer']['synthetic']['TailscaleIPs'] = ['100.100.0.3']
        self.s.phone_request(session, mesh_pb2.ToRadio(want_config_id=1))
        self.assertTrue(session.closed); self.assertFalse(session.records)

    def test_vpn_forwarder_must_be_private_and_supply_identity(self):
        for config in ({}, {'TCP':{'4403':{'TCPForward':'127.0.0.1:47400','ProxyProtocol':2}}},
                       {'TCP':{'4403':{'TCPForward':'127.0.0.1:47401'}}},
                       {**self.forward,'AllowFunnel':{'some-host:4403':True}}):
            self.forward = config
            with self.assertRaises(ValueError): self.s._phone_forwarder()
        for ip in ('192.0.2.5','127.0.0.1','0.0.0.0','::1'):
            with self.assertRaises(ValueError): vpn_ip(ip)

    def test_resume_and_phone_failure_do_not_stop_radio(self):
        with patch.object(self.s._phone, 'start') as start:
            self.s._phone_resume()
            start.assert_called_once_with(PHONE)
        with patch.object(self.s._phone, 'start', side_effect=RuntimeError('synthetic bind failure')):
            self.s._phone_resume()
        self.assertEqual(self.s._radio, 'online')
        self.assertIn('unavailable', self.s._phone_error)

    def test_storage_failure_still_closes_runtime_access(self):
        conn = self.connect(); self.handshake(conn)
        with patch.object(self.s._store, 'set_preference', side_effect=OSError('synthetic full disk')):
            with self.assertRaises(OSError): self.s._phone_set({'enabled':False})
        self.assertFalse(self.s._phone_enabled)
        self.assertEqual(conn.recv(1),b'')
        self.assertEqual(self.s._radio,'online')

    def test_disable_closes_client_and_never_changes_radio_or_tx(self):
        conn = self.connect(); self.handshake(conn)
        payload = self.s._phone_set({'enabled':False})
        self.assertFalse(payload['enabled']); self.assertFalse(self.s._phone.connected)
        self.assertEqual(self.s._radio,'online')
        self.assertTrue(self.s._iface.localNode.localConfig.lora.tx_enabled)
        self.assertEqual(conn.recv(1),b'')

    def test_reconnect_replaces_phone_only(self):
        first = self.connect(); self.handshake(first)
        second = self.connect(); self.handshake(second)
        self.assertEqual(first.recv(1),b'')
        self.assertEqual(self.s._radio,'online')
        self.assertTrue(self.s._phone.connected)

    def test_admin_responses_and_private_material_are_never_broadcast(self):
        conn = self.connect(); self.handshake(conn)
        record = mesh_pb2.FromRadio(); record.packet.decoded.portnum = portnums_pb2.ADMIN_APP
        record.packet.decoded.payload = b'synthetic-secret'
        self.s._phone_frame(self.s._iface, record.SerializeToString())
        conn.settimeout(.1)
        with self.assertRaises(socket.timeout): conn.recv(1)

    def test_official_python_client_handshake_and_message_over_same_protocol(self):
        # A local transport fixture supplies exactly the identity header that
        # Tailscale Serve adds. The unmodified official TCP client does the rest.
        self.s._phone.start(PHONE,port=0)
        listener = socket.socket(); listener.bind(('127.0.0.1',0)); listener.listen(1); listener.settimeout(3)
        address = listener.getsockname()
        def proxy():
            try:
                client,_ = listener.accept(); upstream = socket.create_connection(self.s._phone._server.getsockname(),timeout=2)
                self.connections.extend((client,upstream)); upstream.sendall(proxy_header())
                def relay(source,target):
                    try:
                        while True:
                            raw=source.recv(4096)
                            if not raw: break
                            target.sendall(raw)
                    except OSError: pass
                    finally:
                        try: target.shutdown(socket.SHUT_WR)
                        except OSError: pass
                threading.Thread(target=relay,args=(client,upstream),daemon=True).start()
                relay(upstream,client)
            except OSError: pass
        threading.Thread(target=proxy,daemon=True).start()
        iface = None
        try:
            iface = TCPInterface(hostname=address[0],portNumber=address[1],timeout=3)
            self.assertEqual(iface.myInfo.my_node_num,0x12345678)
            self.assertEqual(iface.localNode.channels[1].settings.name,'Group 1')
            packet=iface.sendText('Synthetic SDK send',channelIndex=1,wantAck=True)
            end=time.monotonic()+2
            while not self.sent and time.monotonic()<end: time.sleep(.01)
            self.assertEqual(self.sent[0].id,packet.id)
            self.assertEqual(self.s._messages[-1]['text'],'Synthetic SDK send')
            self.assertEqual(self.s._radio,'online')
        finally:
            if iface: iface.close()
            listener.close()
