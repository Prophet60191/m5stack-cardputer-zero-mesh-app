# SPDX-License-Identifier: GPL-3.0-or-later
"""Validated application operations; all methods run in the radio owner process."""
from __future__ import annotations

import importlib.metadata
import importlib.util
import json
import math
import os
import platform
import secrets
import shutil
import subprocess
import time
from pathlib import Path

try:
    from google.protobuf.message import DecodeError
except ImportError:
    class DecodeError(ValueError): pass
from radio_settings import SCHEMA, atomic_write, channel_pb2, deviceonly_pb2, localonly_pb2


class ApplicationServices:
    def _require_idle(self):
        if self._iface is not None or self._daemon is not None or self._radio not in ("off", "error"):
            raise ValueError("stop the radio before changing stored setup or applying a profile")

    def _daemon_version(self):
        try:
            result = subprocess.run(["/usr/bin/dpkg-query", "-W", "-f=${Version}", "meshtasticd"],
                                    capture_output=True, text=True, timeout=3)
            return result.stdout.strip()[:80] if result.returncode == 0 else "not installed"
        except (OSError, subprocess.TimeoutExpired): return "unavailable"

    def _check_daemon_compatibility(self):
        if self._client_version() != "2.7.11":
            raise ValueError("install the qualified meshtastic==2.7.11 client in the configured radio venv")
        version = self._daemon_version()
        if not version.startswith("2.7.26"):
            raise ValueError("this candidate requires the qualified meshtasticd 2.7.26 build (schema 24); detected " + version)

    @staticmethod
    def _client_version():
        try: return importlib.metadata.version("meshtastic")
        except importlib.metadata.PackageNotFoundError: return "not installed"

    def _setup_payload(self):
        ready = False
        profile_error = "No radio profile; run first-time setup"
        mode = "not configured"
        region = "UNSET"
        try:
            config, channels = self._settings.load()
            ready = bool(config.lora.region and channels.channels and channels.channels[0].role == 1 and
                         self._valid_channel_key(channels.channels[0].settings.psk))
            profile_error = "" if ready else "Invalid primary channel"
            mode = "transmit" if config.lora.tx_enabled else "receive-only"
            region = config.lora.RegionCode.Name(config.lora.region)
        except (OSError, ValueError, DecodeError, RuntimeError): pass
        dependencies = {"daemon": self._daemon_version(), "client": self._client_version()}
        try:
            self._check_firewall()
            firewall = True
        except Exception: firewall = False
        return {"event": "setup", "ready": ready, "profile_error": profile_error,
                "mode": mode, "region": region, "firewall": firewall,
                "dependencies": dependencies, "history_enabled": self._store.preference("history.enabled", True)}

    def _initialize_setup(self, req):
        self._require_idle()
        if req.get("confirmed") is not True: raise ValueError("confirm initializing a receive-only profile")
        kind = req.get("kind")
        if kind not in ("public", "private"): raise ValueError("choose public or private setup")
        self._settings.initialize(str(req.get("region", "UNSET")), kind == "private", str(req.get("name", "")))
        self._set_radio("off", "Setup saved in receive-only mode; transmission requires a separate choice")

    def _set_transmit(self, req):
        value = req.get("enabled")
        if type(value) is not bool: raise ValueError("transmit mode must be boolean")
        if value and req.get("antenna_confirmed") is not True:
            raise ValueError("confirm the antenna and regional settings before enabling transmission")
        if self._iface is not None:
            self._config_confirmed = False
            try:
                self._set_config("lora.tx_enabled", value)
                self._verify_config()
                if self._iface.localNode.localConfig.lora.tx_enabled != value:
                    raise ValueError("transmit mode readback differs from requested mode")
            except Exception:
                self.radio_stop_with_error("Transmit mode was not confirmed; radio stopped")
                self._broadcast({"event": "config.unknown"})
                raise
            self._config_confirmed = True
        else:
            self._require_idle()
            config, channels = self._settings.load()
            if value and not config.lora.region: raise ValueError("choose the region before enabling transmission")
            config.lora.tx_enabled = value
            self._settings.apply(config, channels)

    def _capture_preferences(self):
        if self._iface is None: return self._settings.load()
        self._verify_config()
        node = self._iface.localNode
        config = localonly_pb2.LocalConfig()
        config.CopyFrom(node.localConfig)
        config.version = SCHEMA
        channels = deviceonly_pb2.ChannelFile(version=SCHEMA)
        for ch in node.channels: channels.channels.add().CopyFrom(ch)
        return config, channels

    def _profile_save(self, name):
        config, channels = self._capture_preferences()
        return self._settings.save_profile(name, config, channels)

    def _profile_preview(self, ident):
        data, config, channels = self._settings.profile(ident)
        return {"event": "profile.preview", "profile": ident, "name": data["name"],
                "region": config.lora.RegionCode.Name(config.lora.region),
                "preset": config.lora.ModemPreset.Name(config.lora.modem_preset),
                "slot": config.lora.channel_num, "mode": "receive-only", "position": False, "mqtt": False,
                "channels": [{"index": ch.index, "name": ch.settings.name,
                              "private": self._private_channel(ch)} for ch in channels.channels if ch.role]}

    def _profile_apply(self, req):
        self._require_idle()
        if req.get("confirmed") is not True: raise ValueError("preview and confirm the mesh profile first")
        _, config, channels = self._settings.profile(req["profile"])
        if (config.version != SCHEMA or channels.version != SCHEMA or not channels.channels or
                channels.channels[0].role != 1 or len(channels.channels) > 8):
            raise ValueError("invalid mesh profile")
        for i, ch in enumerate(channels.channels):
            if ch.index != i or ch.role not in (0, 1, 2) or (i and ch.role == 1): raise ValueError("invalid profile channels")
            if ch.role and (not self._valid_channel_key(ch.settings.psk) or
                            (ch.settings.name and not ch.settings.name.isprintable()) or len(ch.settings.name.encode()) > 11):
                raise ValueError("invalid profile channel settings")
            ch.settings.module_settings.position_precision = 0
            ch.settings.uplink_enabled = ch.settings.downlink_enabled = False
        # Applying a profile never starts RF or silently restores location/MQTT.
        config.lora.tx_enabled = False
        module_path = self._settings.prefs / "module.proto"
        modules = None
        if module_path.exists():
            modules = localonly_pb2.LocalModuleConfig.FromString(module_path.read_bytes())
            modules.mqtt.enabled = False
        self._settings.apply(config, channels, modules)
        atomic_write(self._state_dir / "public-position-consent", b"")
        self._set_radio("off", "Mesh profile applied in receive-only mode; position and MQTT Off")

    def _channel_slots(self):
        self._require_online()
        node = self._iface.localNode
        return {"event": "channel.slots", "channels": [
            {"index": ch.index, "name": ch.settings.name, "role": ch.Role.Name(ch.role),
             "private": self._private_channel(ch), "configured": self._channel_configured(ch),
             "identity": self._settings.identity(ch, node.localConfig.lora)} for ch in node.channels]}

    @staticmethod
    def _channel_configured(ch):
        # Firmware serializes an empty settings submessage in disabled slots.
        # Presence alone cannot distinguish those from retained named/keyed groups.
        return bool(ch.role or ch.settings.name or ch.settings.psk or ch.settings.id)

    @staticmethod
    def _channel_signature(ch):
        return (ch.index, ch.role, ch.settings.name, bytes(ch.settings.psk),
                ch.settings.uplink_enabled, ch.settings.downlink_enabled,
                ch.settings.module_settings.position_precision, ch.settings.module_settings.is_muted,
                ch.settings.id, ch.settings.channel_num)

    @staticmethod
    def _channel_name(name, *, required=False):
        if not isinstance(name, str) or (required and not name) or (name and not name.isprintable()) or len(name.encode()) > 11:
            raise ValueError("channel name must contain at most 11 printable UTF-8 bytes")
        return name

    def _channel_manage(self, req):
        self._require_online()
        if req.get("confirmed") is not True: raise ValueError("confirm the channel change")
        node = self._iface.localNode
        action = req.get("action")
        if action == "create":
            kind = req.get("kind")
            if kind not in ("public", "private"): raise ValueError("choose public or private channel")
            name = self._channel_name(req.get("name", ""), required=kind == "private")
            ch = next((ch for ch in node.channels if ch.index > 0 and not self._channel_configured(ch)), None)
            if ch is None: raise ValueError("all channel slots are occupied")
            ch.settings.name = name
            ch.settings.psk = secrets.token_bytes(32) if kind == "private" else b"\x01"
            ch.role = channel_pb2.Channel.SECONDARY
        else:
            index = req.get("channel")
            if type(index) is not int or not 0 <= index < len(node.channels): raise ValueError("invalid channel index")
            ch = node.channels[index]
            if req.get("channel_id") != self._settings.identity(ch, node.localConfig.lora):
                raise ValueError("channel changed; reopen the manager")
            if action == "rename":
                name = self._channel_name(req.get("name", ""))
                if ch.index == 0 and node.localConfig.lora.channel_num == 0:
                    raise ValueError("set an explicit frequency slot before renaming the primary channel")
                ch.settings.name = name
            elif action == "rotate": ch.settings.psk = secrets.token_bytes(32)
            elif action in ("disable", "remove"):
                if ch.index == 0: raise ValueError("the primary channel cannot be disabled or removed")
                ch.role = channel_pb2.Channel.DISABLED
                if action == "remove": ch.Clear(); ch.index = index
            elif action == "enable":
                if not self._channel_configured(ch): raise ValueError("configure this empty slot first")
                ch.role = channel_pb2.Channel.SECONDARY if ch.index else channel_pb2.Channel.PRIMARY
            else: raise ValueError("unknown channel operation")
        # Changed groups do not inherit location/forwarding consent.
        if action != "remove":
            ch.settings.module_settings.position_precision = 0
            ch.settings.uplink_enabled = ch.settings.downlink_enabled = False
        try:
            node.writeChannel(ch.index)
            expected = self._channel_signature(ch)
            self._config_confirmed = False
            self._verify_config()
            if self._channel_signature(node.channels[ch.index]) != expected:
                raise ValueError("channel readback differs from requested settings")
        except Exception:
            self.radio_stop_with_error("channel change readback failed")
            raise ValueError("channel change was not confirmed")
        self._config_confirmed = True
        self._broadcast(self._channels())
        self._broadcast(self._config_payload())
        self._publish_history()

    def _diagnostics(self):
        # Whitelist export fields. Never copy raw daemon logs, owner/node IDs,
        # coordinates, message bodies, keys, URLs or personal filesystem paths.
        try:
            firewall = self._check_firewall()
        except Exception: firewall = {"ok": False}
        try:
            gps = (self._iface.getMyNodeInfo() or {}).get("position", {}) if self._iface else {}
        except Exception: gps = {}
        channels = self._channels()["channels"]
        import qr_scan
        camera_available = qr_scan.available()
        lat, lon = gps.get("latitude"), gps.get("longitude")
        position_valid = (type(lat) in (int, float) and type(lon) in (int, float) and
                          math.isfinite(lat) and math.isfinite(lon) and -90 <= lat <= 90 and -180 <= lon <= 180)
        age = self._position_age(gps)
        connected = getattr(self._iface, "isConnected", None)
        return {"event": "diagnostics", "app": "0.1.5", "python": platform.python_version(),
                "client": self._client_version(), "daemon": self._daemon_version(),
                "radio": self._radio, "api_connected": bool(connected and connected.is_set()),
                "mode": self._status()["mode"], "firewall": firewall,
                "gps_age": self._position_age(gps), "gps_satellites": gps.get("satsInView") if type(gps.get("satsInView")) is int and 0 <= gps["satsInView"] <= 255 else None,
                "position_valid": position_valid, "position_fresh": position_valid and age is not None and age <= 120,
                "last_rx_age": max(0, int(time.time()) - self._last_rx) if self._last_rx else None,
                "camera_available": camera_available,
                "channels": len(channels), "private_channels": sum(ch["private"] for ch in channels),
                "history_count": len(self._messages), "history_enabled": self._store.preference("history.enabled", True)}

    def _export_diagnostics(self):
        result = self._diagnostics()
        path = self._state_dir / "diagnostics-export.json"
        atomic_write(path, json.dumps(result, indent=2, allow_nan=False).encode())
        return result
