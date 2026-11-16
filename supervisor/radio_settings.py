# SPDX-License-Identifier: GPL-3.0-or-later
"""Radio-owned identities, safe initial preferences and local mesh profiles."""
from __future__ import annotations

import base64
import hashlib
import hmac
import json
import os
import re
import secrets
import stat
import tempfile
import uuid
from pathlib import Path

try:
    from meshtastic.protobuf import channel_pb2, deviceonly_pb2, localonly_pb2
except ImportError:
    channel_pb2 = deviceonly_pb2 = localonly_pb2 = None


def require_client():
    if localonly_pb2 is None:
        raise RuntimeError("Install meshtastic==2.7.11 in the configured radio venv before setup")


SCHEMA = 24


def atomic_write(path: Path, payload: bytes):
    path.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
    fd, temp = tempfile.mkstemp(prefix=".mesh-", dir=path.parent)
    try:
        with os.fdopen(fd, "wb") as out:
            out.write(payload)
            out.flush()
            os.fsync(out.fileno())
        os.replace(temp, path)
        sync_directory(path.parent)
    finally:
        if os.path.exists(temp): os.unlink(temp)


def sync_directory(path):
    fd = os.open(path, os.O_RDONLY | os.O_DIRECTORY)
    try: os.fsync(fd)
    finally: os.close(fd)


class RadioSettings:
    def __init__(self, directory):
        self.directory = Path(directory)
        self.prefs = self.directory / "prefs"
        self.profiles = self.directory / "profiles"
        secret = self.directory / "identity-secret"
        try:
            info = secret.lstat()
            if not stat.S_ISREG(info.st_mode) or info.st_uid != os.getuid() or info.st_mode & 0o077:
                raise PermissionError("identity secret must be an owned private regular file")
            self._secret = secret.read_bytes()
        except FileNotFoundError:
            self._secret = secrets.token_bytes(32)
            atomic_write(secret, self._secret)
        if len(self._secret) != 32: raise ValueError("invalid local identity secret")
        self.recover()

    def identity(self, channel, lora):
        key = bytes(channel.settings.psk)
        if len(key) == 1:
            n = key[0]
            if n == 0: key = b""
            else:
                default = bytearray.fromhex("d4f1bb3a20290759f0bcffabcf4e6901")
                default[-1] = (default[-1] + n - 1) & 255
                key = bytes(default)
        # Default channel names are derived from the modem. Index and role are
        # deliberately excluded, so moving a group to a new slot preserves it.
        name = channel.settings.name
        effective = name if name else "default-preset:" + str(lora.modem_preset)
        value = effective.encode() + b"\0" + key
        if channel.settings.id:
            value += b"\0id" + channel.settings.id.to_bytes(4, "big")
        return hmac.new(self._secret, value, hashlib.sha256).hexdigest()[:32]

    def load(self):
        require_client()
        config = localonly_pb2.LocalConfig.FromString((self.prefs / "config.proto").read_bytes())
        channels = deviceonly_pb2.ChannelFile.FromString((self.prefs / "channels.proto").read_bytes())
        if config.version != SCHEMA or channels.version != SCHEMA or not config.HasField("lora"):
            raise ValueError("unsupported preference schema")
        return config, channels

    def recover(self):
        marker = self.directory / "settings-change.pending"
        if not marker.exists(): return
        require_client()
        payload = marker.read_bytes()
        if len(payload) > 128 * 1024: raise ValueError("oversized radio recovery transaction")
        data = json.loads(payload)
        config = localonly_pb2.LocalConfig.FromString(base64.b64decode(data["config"], validate=True))
        channels = deviceonly_pb2.ChannelFile.FromString(base64.b64decode(data["channels"], validate=True))
        if config.version != SCHEMA or channels.version != SCHEMA:
            raise ValueError("invalid radio recovery transaction")
        modules = None
        if "modules" in data:
            modules = localonly_pb2.LocalModuleConfig.FromString(base64.b64decode(data["modules"], validate=True))
            if modules.version != SCHEMA: raise ValueError("invalid module recovery transaction")
        atomic_write(self.prefs / "config.proto", config.SerializeToString())
        atomic_write(self.prefs / "channels.proto", channels.SerializeToString())
        if modules is not None:
            atomic_write(self.prefs / "module.proto", modules.SerializeToString())
        marker.unlink()
        sync_directory(self.directory)

    def apply(self, config, channels, modules=None):
        payload = {"config": base64.b64encode(config.SerializeToString()).decode(),
                   "channels": base64.b64encode(channels.SerializeToString()).decode()}
        if modules is not None: payload["modules"] = base64.b64encode(modules.SerializeToString()).decode()
        atomic_write(self.directory / "settings-change.pending", json.dumps(payload).encode())
        self.recover()  # idempotent roll-forward if power fails between the files

    def initialize(self, region, private=False, name=""):
        require_client()
        if (self.prefs / "config.proto").exists() or (self.prefs / "channels.proto").exists():
            raise ValueError("a radio profile already exists; setup will not overwrite it")
        config = localonly_pb2.LocalConfig(version=SCHEMA)
        for field in ("device", "display", "power", "network", "position", "bluetooth", "security"):
            if field in config.DESCRIPTOR.fields_by_name: getattr(config, field).SetInParent()
        lora = config.lora
        lora.region = lora.RegionCode.Value(region)
        if not lora.region: raise ValueError("choose a region before initializing")
        lora.use_preset = True
        lora.modem_preset = lora.ModemPreset.Value("LONG_FAST")
        lora.hop_limit = 3
        lora.tx_enabled = False
        config.position.gps_mode = config.position.GpsMode.Value("ENABLED")
        # Portduino uses this for its local console/API and complete startup
        # diagnostics. An empty security submessage disables it implicitly.
        config.security.serial_enabled = True
        channels = deviceonly_pb2.ChannelFile(version=SCHEMA)
        if private and (not name or not name.isprintable() or len(name.encode()) > 11):
            raise ValueError("private channel name must contain 1..11 printable UTF-8 bytes")
        for i in range(8):
            ch = channels.channels.add(index=i)
            if i == 0:
                ch.role = channel_pb2.Channel.PRIMARY
                ch.settings.name = name if private else ""
                ch.settings.psk = secrets.token_bytes(32) if private else b"\x01"
                ch.settings.module_settings.position_precision = 0
        modules = localonly_pb2.LocalModuleConfig(version=SCHEMA)
        modules.mqtt.enabled = False
        self.apply(config, channels, modules)

    @staticmethod
    def _profile_id(value):
        if not isinstance(value, str) or not re.fullmatch(r"[a-f0-9]{32}", value):
            raise ValueError("invalid profile identity")
        return value

    def save_profile(self, name, config, channels):
        if not isinstance(name, str) or not name.strip() or not name.isprintable() or len(name.encode()) > 48:
            raise ValueError("profile name must contain 1..48 printable UTF-8 bytes")
        self.profiles.mkdir(mode=0o700, exist_ok=True)
        if len(list(self.profiles.glob("*.json"))) >= 16: raise ValueError("profile limit reached")
        ident = uuid.uuid4().hex
        data = {"id": ident, "name": name, "schema": SCHEMA,
                "config": base64.b64encode(config.SerializeToString()).decode(),
                "channels": base64.b64encode(channels.SerializeToString()).decode()}
        atomic_write(self.profiles / (ident + ".json"), json.dumps(data).encode())
        return ident

    def profile(self, ident):
        require_client()
        ident = self._profile_id(ident)
        path = self.profiles / (ident + ".json")
        raw = path.read_bytes()
        if len(raw) > 128 * 1024: raise ValueError("oversized mesh profile")
        data = json.loads(raw)
        if data.get("schema") != SCHEMA: raise ValueError("unsupported mesh profile")
        config = localonly_pb2.LocalConfig.FromString(base64.b64decode(data["config"], validate=True))
        channels = deviceonly_pb2.ChannelFile.FromString(base64.b64decode(data["channels"], validate=True))
        return data, config, channels

    def list_profiles(self):
        if not self.profiles.exists(): return []
        profiles = []
        for path in sorted(self.profiles.glob("*.json"))[:16]:
            try:
                data, config, channels = self.profile(path.stem)
                profiles.append({"id": path.stem, "name": data["name"],
                                 "region": config.lora.RegionCode.Name(config.lora.region),
                                 "preset": config.lora.ModemPreset.Name(config.lora.modem_preset),
                                 "slot": config.lora.channel_num,
                                 "channels": sum(bool(ch.role) for ch in channels.channels)})
            except (ValueError, KeyError, OSError): continue
        return profiles

    def delete_profile(self, ident):
        (self.profiles / (self._profile_id(ident) + ".json")).unlink()
        sync_directory(self.profiles)
