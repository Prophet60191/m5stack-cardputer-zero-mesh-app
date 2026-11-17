# SPDX-License-Identifier: GPL-3.0-or-later
"""Bounded, cancellable camera capture followed by an explicit channel preview.

Only confirmation runs on the serialized radio worker. Preview URLs/keys stay in
memory, are bound to the requesting connection, and are never returned to the UI.
"""
import base64
import hashlib
import re
import threading
import time


class ChannelImport:
    def _init_imports(self):
        self._import_lock = threading.RLock()
        self._import_previews = {}
        self._setup_plans = {}
        self._setup_latest = {}
        self._import_latest = {}
        self._scan_job = None

    def _import_plan(self, url, node=None):
        from meshtastic.protobuf import apponly_pb2
        if not isinstance(url, str) or len(url) > 4096:
            raise ValueError("channel share URL must be at most 4096 characters")
        match = re.fullmatch(r"https://meshtastic\.org/(?:e|d|v)/(?:\?add=true)?#([A-Za-z0-9_-]+={0,2})", url)
        if not match: raise ValueError("not a Meshtastic channel share URL")
        encoded = match.group(1).rstrip("=")
        try:
            channels = apponly_pb2.ChannelSet.FromString(base64.urlsafe_b64decode(encoded + "=" * (-len(encoded) % 4)))
        except Exception:
            raise ValueError("invalid channel share data") from None
        if not channels.settings or len(channels.settings) > 8:
            raise ValueError("URL must contain 1..8 channels")
        for settings in channels.settings:
            if (not self._valid_channel_key(settings.psk) or len(settings.name.encode("utf-8")) > 11 or
                    (settings.name and not settings.name.isprintable())):
                raise ValueError("invalid channel key or name (maximum 11 UTF-8 bytes)")
        node = node if node is not None else self._iface.localNode
        identities = {(ch.settings.name, bytes(ch.settings.psk), ch.settings.id) for ch in node.channels if ch.role}
        free = [ch for ch in node.channels if not self._channel_configured(ch) and ch.index != 0]
        pending, summaries, seen = [], [], set()
        for settings in channels.settings:
            identity = (settings.name, bytes(settings.psk), settings.id)
            if identity in seen: continue
            seen.add(identity)
            existing = identity in identities
            if not existing:
                identities.add(identity)
                pending.append(settings)
            # Classification uses the same key policy as live channels.
            from types import SimpleNamespace
            summaries.append({"name": settings.name or "Default", "private": self._private_channel(SimpleNamespace(settings=settings)), "existing": existing})
        if len(pending) > len(free): raise ValueError("not enough free channel slots")
        return pending, free, summaries

    def _import_fingerprint(self, node=None):
        node = node if node is not None else self._iface.localNode
        data = (node.localConfig.lora.SerializeToString(), tuple(self._channel_signature(ch) for ch in node.channels))
        return hashlib.sha256(repr(data).encode()).hexdigest()

    def _preview_import(self, client, req, url, cancelled=None):
        import secrets
        request = req.get("id")
        if not isinstance(request, str) or not request: raise ValueError("preview requires a request ID")
        with self._life_lock:
            with self._import_lock:
                if self._preview_cancelled(client, request): return None
            setup = req.get("setup", False)
            if type(setup) is not bool: raise ValueError("setup must be boolean")
            if setup: node = self._setup_node(self._setup_snapshot())
            else:
                self._require_online()
                node = self._iface.localNode
            pending, free, summaries = self._import_plan(url, node)
            fingerprint = self._import_fingerprint(node)
            lora = node.localConfig.lora
            tuning = {"region": lora.RegionCode.Name(lora.region),
                      "preset": lora.ModemPreset.Name(lora.modem_preset), "slot": lora.channel_num}
            with self._import_lock:
                if self._preview_cancelled(client, request) or (cancelled is not None and cancelled.is_set()): return None
                self._import_previews = {c: item for c, item in self._import_previews.items() if item["expires"] > time.monotonic()}
                if client not in self._import_previews and len(self._import_previews) >= 8:
                    raise ValueError("too many pending channel previews")
                token = secrets.token_hex(16) if pending else ""
                if pending:
                    self._import_previews[client] = {"request": request, "token": token, "url": url,
                        "fingerprint": fingerprint, "expires": time.monotonic() + (600 if setup else 120)}
                else:
                    self._import_previews.pop(client, None)
                    self._import_latest.pop(client, None)
        result = {"event": "channel.preview", "token": token, "new": len(pending), "available": len(free),
                  "channels": summaries, **tuning}
        if setup:
            from meshtastic.protobuf import apponly_pb2
            encoded = url.split("#", 1)[1].rstrip("=")
            data = apponly_pb2.ChannelSet.FromString(base64.urlsafe_b64decode(encoded + "=" * (-len(encoded) % 4)))
            if data.HasField("lora_config") and data.lora_config.use_preset and not data.lora_config.override_frequency:
                imported = data.lora_config
                result["suggested_tuning"] = {"region": imported.RegionCode.Name(imported.region) if imported.region else tuning["region"],
                    "preset": imported.ModemPreset.Name(imported.modem_preset), "use_preset": True,
                    "slot": imported.channel_num, "hop": imported.hop_limit or lora.hop_limit, "override_frequency": 0}
            elif data.HasField("lora_config") and data.lora_config.override_frequency:
                result["tuning_warning"] = "Link uses a fixed frequency; it is not imported. Verify the current frequency matches the sender."
        return result

    def _confirm_import(self, client, req):
        if req.get("confirmed") is not True: raise ValueError("confirm the channel preview before importing")
        with self._import_lock:
            preview = self._import_previews.get(client)
            if not preview or req.get("token") != preview["token"]:
                raise ValueError("channel preview is missing; scan or paste again")
            self._import_previews.pop(client)
            self._import_latest.pop(client, None)
        if preview["expires"] <= time.monotonic(): raise ValueError("channel preview expired; scan or paste again")
        if preview["fingerprint"] != self._import_fingerprint():
            raise ValueError("radio or channels changed; preview again before importing")
        return self._add_channel(preview["url"])

    def _start_scan(self, client, req):
        request = req.get("id")
        if not isinstance(request, str) or not request: raise ValueError("scan requires a request ID")
        setup = req.get("setup", False)
        if type(setup) is not bool: raise ValueError("setup must be boolean")
        with self._lock:
            if not setup and (self._iface is None or self._radio != "online"): raise ValueError("turn the radio on before scanning channels")
        with self._import_lock:
            if self._scan_job is not None: raise ValueError("camera is already scanning; cancel or wait, then retry")
            self._begin_import(client, request)
            job = {"client": client, "request": request, "setup": setup, "cancelled": threading.Event()}
            job["thread"] = threading.Thread(target=self._capture_preview, args=(job,), daemon=True)
            self._scan_job = job
            try: job["thread"].start()
            except RuntimeError:
                self._scan_job = None
                self._cancel_import(client, request)
                raise RuntimeError("camera worker could not start; retry after closing other apps") from None

    def _capture_preview(self, job):
        client, request, cancelled = job["client"], job["request"], job["cancelled"]
        try:
            url = self._scan_qr(cancelled)
            if cancelled.is_set() or self._stop.is_set(): return
            if not url: raise ValueError("No QR code found; aim the camera and retry")
            result = self._preview_import(client, {"id": request, "setup": job["setup"]}, url, cancelled)
            with self._import_lock:
                if result is not None and not cancelled.is_set(): self._send(client, {**result, "id": request})
        except Exception as exc:
            with self._import_lock:
                if not cancelled.is_set() and not self._stop.is_set():
                    self._cancel_import(client, request)
                    self._send(client, {"event": "error", "intent": "channel.scan", "id": request, "text": str(exc)[:256]})
        finally:
            with self._import_lock:
                if self._scan_job is job: self._scan_job = None

    def _cancel_import(self, client, request=None):
        self._setup_cancel(client, request)
        with self._import_lock:
            if request is None or self._import_latest.get(client) == request:
                self._import_latest.pop(client, None)
            preview = self._import_previews.get(client)
            if preview and (request is None or request == preview["request"]): self._import_previews.pop(client)
            job = self._scan_job
            if job and job["client"] is client and (request is None or request == job["request"]):
                job["cancelled"].set()

    def _preview_cancelled(self, client, request):
        return (self._stop.is_set() or not getattr(client, "alive", True) or
                self._import_latest.get(client) != request)

    def _begin_import(self, client, request):
        with self._import_lock:
            self._cancel_import(client)
            self._import_latest[client] = request

    def _stop_imports(self):
        with self._import_lock:
            self._import_previews.clear()
            self._setup_plans.clear()
            self._setup_latest.clear()
            self._import_latest.clear()
            job = self._scan_job
            if job: job["cancelled"].set()
        if job: job["thread"].join(timeout=2)
