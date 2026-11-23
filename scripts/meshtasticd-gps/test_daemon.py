# SPDX-License-Identifier: GPL-3.0-or-later
"""Real rebuilt daemon + official SDK smoke test; simulated radio, no hardware.

The actual GPS parser's delayed replies are tested separately by test_probe.py.
This checks that the rebuilt executable retains the client/config protocol.
"""
import json,os,subprocess,sys,tempfile,time,socket,fcntl,struct
from pathlib import Path
from meshtastic.protobuf import localonly_pb2,deviceonly_pb2,channel_pb2,mesh_pb2
from meshtastic.tcp_interface import TCPInterface
assert os.environ.get('CZ_MESH_ISOLATED_TEST')=='1'
with socket.socket() as check:
 for _,name in socket.if_nameindex():
  flags=struct.unpack_from('H',fcntl.ioctl(check,0x8913,struct.pack('256s',name.encode())),16)[0]
  assert name=='lo' or not flags & 1,'run with --network none (kernel tunnel templates may exist but must be down)'
assert not any(Path('/dev').glob('gpiochip*')) and not any(Path('/dev').glob('spidev*'))
with tempfile.TemporaryDirectory() as temp:
 root=Path(temp);prefs=root/'prefs';prefs.mkdir()
 config=localonly_pb2.LocalConfig(version=24);config.position.gps_mode=2
 for field in ('device','display','lora','position','power','network','bluetooth','security'):getattr(config,field).SetInParent()
 config.lora.region=1;config.lora.tx_enabled=False;config.lora.use_preset=True;config.lora.modem_preset=3;config.lora.hop_limit=3
 (prefs/'config.proto').write_bytes(config.SerializeToString())
 state=deviceonly_pb2.DeviceState(version=24);state.my_node.my_node_num=0x12345678
 (prefs/'device.proto').write_bytes(state.SerializeToString())
 channels=deviceonly_pb2.ChannelFile(version=24)
 ch=channels.channels.add(index=0,role=channel_pb2.Channel.PRIMARY);ch.settings.name='Synthetic';ch.settings.psk=b'R'*32
 for index in range(1,8):channels.channels.add(index=index,role=channel_pb2.Channel.DISABLED)
 (prefs/'channels.proto').write_bytes(channels.SerializeToString())
 (prefs/'module.proto').write_bytes(localonly_pb2.LocalModuleConfig(version=24).SerializeToString())
 (root/'radio.yaml').write_text('Lora:\n  Module: sim\nWebserver:\n  Port: -1\nLogging:\n  LogLevel: error\n  AsciiLogs: true\n')
 with (root/'native.log').open('wb') as output:
  proc=subprocess.Popen([str(Path(sys.argv[1]).resolve()),'--sim','-c',str(root/'radio.yaml'),'-p','47400','--fsdir='+str(root)],stdout=output,stderr=subprocess.STDOUT)
  iface=None
  try:
   deadline=time.monotonic()+20
   while True:
    try:
     probe=socket.create_connection(('127.0.0.1',47400),timeout=.2);probe.close();break
    except OSError:
     assert proc.poll() is None and time.monotonic()<deadline,'simulated API did not start'
     time.sleep(.1)
   time.sleep(.2)
   iface=TCPInterface(hostname='127.0.0.1',portNumber=47400,timeout=5)
   assert iface.isConnected.is_set() and iface.myInfo
   assert iface.localNode.channels[0].settings.name=='Synthetic','saved setup changed'
   assert not iface.localNode.localConfig.lora.tx_enabled,'receive-only setup changed'
   count=len(iface._localChannels)
   iface._sendToRadio(mesh_pb2.ToRadio(want_config_id=69421));time.sleep(.5)
   assert len(iface._localChannels)==count,'node-only refresh appended channels'
   print(json.dumps({'rebuilt_native_sdk_handshake':True,'saved_private_rx_setup':True,'node_only_refresh':True,'radio':'simulated'}))
  finally:
   if iface:iface.close()
   proc.terminate()
   try:proc.wait(timeout=8)
   except subprocess.TimeoutExpired:proc.kill();proc.wait()
