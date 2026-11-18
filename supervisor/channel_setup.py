# SPDX-License-Identifier: GPL-3.0-or-later
"""Connection-bound channel setup: validate privately, then commit once.

No secret is returned to the UI. Tuning is global; a separate-profile save leaves
active preferences/radio untouched. Only an explicit commit can persist changes.
"""
import hashlib
import hmac
import math
import secrets
import time
from types import SimpleNamespace
from radio_settings import SCHEMA, atomic_write, channel_pb2, localonly_pb2, sync_directory


class ChannelSetup:
    def _setup_snapshot(self):
        if self._radio not in ('off', 'error', 'online'):
            raise ValueError('wait for the radio to finish starting or stopping')
        if self._radio in ('off', 'error'): self._require_idle()
        elif self._iface is None: raise ValueError('radio connection is unavailable')
        config, channels = self._capture_preferences()
        if (config.version != SCHEMA or channels.version != SCHEMA or len(channels.channels) != 8 or
                channels.channels[0].role != 1 or not config.lora.region):
            raise ValueError('complete first-run setup before adding a channel')
        for i, ch in enumerate(channels.channels):
            if ch.index != i or ch.role not in (0, 1, 2) or (i and ch.role == 1):
                raise ValueError('invalid saved channel layout')
            if ch.role and not self._valid_channel_key(ch.settings.psk):
                raise ValueError('invalid saved channel key')
        modules = localonly_pb2.LocalModuleConfig(version=SCHEMA)
        path = self._settings.prefs / 'module.proto'
        if path.exists():
            modules.ParseFromString(path.read_bytes())
            if modules.version != SCHEMA: raise ValueError('unsupported module preference schema')
        if self._iface is not None:
            modules.mqtt.CopyFrom(self._iface.localNode.moduleConfig.mqtt)
        modules.version = SCHEMA
        if sum(p.ByteSize() for p in (config, channels, modules)) > 128 * 1024:
            raise ValueError('radio settings exceed the setup size limit')
        return config, channels, modules

    @staticmethod
    def _setup_tuning(config):
        lora = config.lora
        return dict(region=lora.RegionCode.Name(lora.region), preset=lora.ModemPreset.Name(lora.modem_preset),
                    use_preset=lora.use_preset, slot=lora.channel_num, hop=lora.hop_limit,
                    override_frequency=lora.override_frequency, frequency_offset=lora.frequency_offset)

    def _setup_fingerprint(self, snapshot):
        material = b'\0'.join(p.SerializeToString(deterministic=True) for p in snapshot)
        return hmac.new(self._settings._secret, self._radio.encode() + b'\0' + material, hashlib.sha256).hexdigest()

    def _setup_context(self):
        config, channels, modules = self._setup_snapshot()
        return {'event': 'channel.setup.context', 'tuning': self._setup_tuning(config),
                'base': self._setup_fingerprint((config, channels, modules)),
                'radio': self._radio, 'tx': config.lora.tx_enabled,
                'mqtt': modules.mqtt.enabled,
                'available': sum(ch.index > 0 and not self._channel_configured(ch) for ch in channels.channels),
                'profiles_available': 16 - len(list(self._settings.profiles.glob('*.json')))}

    @staticmethod
    def _setup_bool(req, key):
        value = req.get(key)
        if type(value) is not bool: raise ValueError(key + ' must be boolean')
        return value

    def _setup_preview(self, client, req):
        request = req.get('id')
        if not isinstance(request, str) or not request: raise ValueError('setup preview requires a request ID')
        with self._import_lock:
            if self._setup_latest.get(client) != request or not getattr(client, 'alive', True):
                raise ValueError('setup preview cancelled')
        original = self._setup_snapshot()
        if req.get('base') != self._setup_fingerprint(original):
            raise ValueError('setup changed; reload current radio settings before saving')
        config, channels, modules = tuple(type(p).FromString(p.SerializeToString()) for p in original)
        if not self._public_position_approved(channels.channels):
            raise ValueError('existing public location sharing is unconfirmed; turn it off before setup')
        source = req.get('source')
        summaries = []
        if source == 'import':
            with self._import_lock:
                imported = self._import_previews.get(client)
                if (not imported or req.get('import_token') != imported['token'] or
                        imported['expires'] <= time.monotonic()):
                    raise ValueError('channel preview expired; scan or paste again')
                if imported['fingerprint'] != self._import_fingerprint(self._setup_node(original)):
                    raise ValueError('radio or channels changed; scan or paste again')
                url = imported['url']
            pending, _, summaries = self._import_plan(url, self._setup_node(original))
            if not pending: raise ValueError('all imported channels are already present')
        elif source in ('public', 'private'):
            name = self._channel_name(req.get('name', ''), required=source == 'private')
            item = channel_pb2.ChannelSettings(name=name, psk=secrets.token_bytes(32) if source == 'private' else b'\x01')
            if any(ch.role and ch.settings.name == name and bytes(ch.settings.psk) == bytes(item.psk)
                   for ch in channels.channels):
                raise ValueError('this public channel is already present')
            pending = [item]
        else: raise ValueError('choose scan, paste, public or private channel')
        tuning = req.get('tuning')
        if not isinstance(tuning, dict): raise ValueError('radio tuning is required')
        lora = config.lora
        region, preset = tuning.get('region'), tuning.get('preset')
        if not isinstance(region, str) or not isinstance(preset, str): raise ValueError('invalid region or preset')
        lora.region = lora.RegionCode.Value(region)
        if not lora.region: raise ValueError('choose a region')
        lora.modem_preset = lora.ModemPreset.Value(preset)
        lora.use_preset = self._setup_bool(tuning, 'use_preset')
        if not lora.use_preset and (original[0].lora.use_preset or lora.modem_preset != original[0].lora.modem_preset):
            raise ValueError('custom modulation can only be preserved unchanged')
        for key, field, low, high in [('slot', 'channel_num', 0, 255), ('hop', 'hop_limit', 1, 7)]:
            value = tuning.get(key)
            if type(value) is not int or not low <= value <= high: raise ValueError(key + ' is out of range')
            setattr(lora, field, value)
        frequency = tuning.get('override_frequency', original[0].lora.override_frequency)
        if (type(frequency) not in (int, float) or not math.isfinite(frequency) or
                frequency not in (0, original[0].lora.override_frequency)):
            raise ValueError('a custom frequency can only be preserved or explicitly cleared')
        if frequency and lora.region != original[0].lora.region:
            raise ValueError('clear the custom frequency before changing region')
        lora.override_frequency = frequency
        tx = self._setup_bool(req, 'tx')
        position = self._setup_bool(req, 'position')
        mqtt = self._setup_bool(req, 'mqtt')
        profile_only = self._setup_bool(req, 'profile_only')
        name = req.get('profile_name', '')
        if not isinstance(name, str): raise ValueError('invalid profile name')
        if name:
            self._settings.validate_profile_name(name)
            if len(list(self._settings.profiles.glob('*.json'))) >= 16: raise ValueError('profile limit reached')
        if profile_only and not name: raise ValueError('name the separate mesh profile')
        if profile_only and (tx or position or mqtt):
            raise ValueError('separate profiles start receive-only, with location and MQTT Off')
        free = [ch for ch in channels.channels if ch.index > 0 and not self._channel_configured(ch)]
        if len(pending) > len(free): raise ValueError('not enough free channel slots')
        added = []
        for settings, ch in zip(pending, free):
            ch.settings.CopyFrom(settings)
            ch.role = channel_pb2.Channel.SECONDARY
            ch.settings.module_settings.position_precision = 0
            ch.settings.uplink_enabled = ch.settings.downlink_enabled = mqtt
            added.append(ch)
        position_target = req.get('position_target', 0)
        if type(position_target) is not int or not 0 <= position_target < len(added):
            raise ValueError('select a valid new channel for automatic location')
        if position:
            for ch in channels.channels: ch.settings.module_settings.position_precision = 0
            target = added[position_target]
            target.settings.module_settings.position_precision = 16 if self._private_channel(target) else 15
            config.position.position_broadcast_smart_enabled = True
            config.position.position_broadcast_secs = 900
        lora.tx_enabled = tx
        if mqtt: modules.mqtt.enabled = True
        if profile_only:
            for ch in channels.channels:
                ch.settings.module_settings.position_precision = 0
                ch.settings.uplink_enabled = ch.settings.downlink_enabled = False
            modules.mqtt.enabled = False
        public = any(not self._private_channel(ch) for ch in added)
        tuning_changed = self._setup_tuning(config) != self._setup_tuning(original[0])
        public_position = position and not self._private_channel(added[position_target])
        warnings = []
        if public: warnings.append('Public channel: messages and metadata may be visible to others.')
        if tuning_changed: warnings.append('Radio tuning changes affect every channel on this radio.')
        if lora.override_frequency: warnings.append('Existing fixed frequency is preserved: ' + str(lora.override_frequency) + ' MHz; slot is overridden.')
        if lora.frequency_offset: warnings.append('Existing radio frequency calibration offset is preserved.')
        if source == 'import' and lora.channel_num == 0: warnings.append("Imported Auto uses this device's primary name; set an explicit slot if the sender has a different primary.")
        if lora.channel_num == 0: warnings.append('Auto frequency uses the unchanged primary channel name.')
        if position: warnings.append('Automatic location sharing moves to ' + (added[position_target].settings.name or 'Default') + '; GPS receiver stays unchanged.')
        if public_position: warnings.append('Public automatic location is limited to roughly 730 m precision.')
        if mqtt: warnings.append('MQTT connects to the configured broker; other forwarding-enabled channels may also forward.')
        if tx: warnings.append('TX/RX permits firmware broadcasts as well as your messages. Check antenna and region.')
        if not profile_only and self._radio == 'online': warnings.append('The radio will restart once; any pending message ACKs are interrupted. No automatic resend.')
        if profile_only: warnings.append('Separate profile only: active setup and radio stay unchanged. Profile starts RX/location/MQTT Off.')
        snapshot = (config, channels, modules)
        token = secrets.token_hex(16)
        plan = dict(request=request, token=token, expires=time.monotonic()+120,
                    fingerprint=self._setup_fingerprint(original), snapshot=snapshot, original=original,
                    profile_only=profile_only, profile_name=name, tuning_changed=tuning_changed,
                    tx_confirmation=tx and (not original[0].lora.tx_enabled or tuning_changed), public_position=public_position,
                    mqtt=mqtt, added=[ch.index for ch in added])
        with self._import_lock:
            if self._setup_latest.get(client) != request or not getattr(client, 'alive', True):
                raise ValueError('setup preview cancelled')
            self._setup_plans = {c: p for c, p in self._setup_plans.items() if p['expires'] > time.monotonic()}
            if client not in self._setup_plans and len(self._setup_plans) >= 8: raise ValueError('too many setup previews')
            self._setup_plans[client] = plan
        return {'event': 'channel.setup.preview', 'token': token, 'tuning': self._setup_tuning(config),
                'channels': [{'index': ch.index, 'name': ch.settings.name or 'Default', 'private': self._private_channel(ch)} for ch in added],
                'tx': tx, 'position': position, 'position_channel': (added[position_target].settings.name or 'Default') if position else '', 'mqtt': mqtt, 'profile_only': profile_only,
                'profile_name': name, 'tuning_changed': tuning_changed,
                'tx_confirmation': plan['tx_confirmation'], 'public_position': public_position, 'warnings': warnings}

    @staticmethod
    def _setup_node(snapshot):
        return SimpleNamespace(localConfig=snapshot[0], channels=snapshot[1].channels)

    def _setup_begin(self, client, request):
        with self._import_lock:
            self._setup_cancel(client)
            self._setup_latest[client] = request

    def _setup_cancel(self, client, request=None):
        with self._import_lock:
            if request is None or self._setup_latest.get(client) == request: self._setup_latest.pop(client, None)
            plan = self._setup_plans.get(client)
            if plan and (request is None or plan['request'] == request): self._setup_plans.pop(client, None)

    def _setup_commit(self, client, req):
        with self._import_lock:
            plan = self._setup_plans.get(client)
            if not plan or req.get('token') != plan['token']: raise ValueError('check the form and save again')
        if plan['expires'] <= time.monotonic(): raise ValueError('setup validation expired; save again')
        if req.get('confirmed') is not True: raise ValueError('confirm saving the setup')
        for needed, field in [(plan['tuning_changed'], 'tuning_confirmed'), (plan['tx_confirmation'], 'antenna_confirmed'),
                              (plan['public_position'], 'public_position_confirmed'), (plan['mqtt'], 'mqtt_confirmed')]:
            if needed and req.get(field) is not True: raise ValueError('confirm ' + field.replace('_', ' '))
        current = self._setup_snapshot()
        if plan['fingerprint'] != self._setup_fingerprint(current): raise ValueError('setup changed; reload current radio settings before saving')
        # Cancellation/disconnect during a slow readback must win before writes.
        with self._import_lock:
            if self._setup_plans.get(client) is not plan or not getattr(client, 'alive', True):
                raise ValueError('setup cancelled before saving')
            self._setup_plans.pop(client)
            self._setup_latest.pop(client, None)
            self._import_previews.pop(client, None)
            self._import_latest.pop(client, None)
        config, channels, modules = plan['snapshot']
        if plan['profile_only']:
            ident = self._settings.save_profile(plan['profile_name'], config, channels)
            return {'event': 'channel.setup.saved', 'profile_only': True, 'profile': ident,
                    'verified': 'stored', 'added': len(plan['added']), 'channels': []}
        online = self._radio == 'online'
        consent = self._state_dir / 'public-position-consent'
        old_consent = consent.read_bytes() if consent.exists() else None
        self._config_confirmed = False
        self.radio_stop()
        try:
            self._settings.apply(config, channels, modules)
            saved = (*self._settings.load(), localonly_pb2.LocalModuleConfig.FromString((self._settings.prefs/'module.proto').read_bytes()))
            if any(a.SerializeToString() != b.SerializeToString() for a, b in zip(plan['snapshot'], saved)):
                raise ValueError('stored setup readback differs')
            atomic_write(consent, self._public_position_digest(channels.channels).encode())
            if online:
                self.radio_start()
                self._require_online()
                actual = self._setup_snapshot()
                # Compare the fields this operation changes; firmware may normalize
                # unrelated protobuf defaults during startup.
                if (self._setup_tuning(actual[0]) != self._setup_tuning(config) or actual[0].lora.tx_enabled != config.lora.tx_enabled or
                        [self._channel_signature(ch) for ch in actual[1].channels] != [self._channel_signature(ch) for ch in channels.channels] or
                        actual[2].mqtt.enabled != modules.mqtt.enabled):
                    raise ValueError('radio setup readback differs')
            else:
                self._set_radio('off', 'Channel setup saved; live verification occurs when Radio is turned on')
        except Exception:
            self.radio_stop()
            try:
                self._settings.apply(*current)
                if old_consent is None:
                    consent.unlink(missing_ok=True)
                    sync_directory(self._state_dir)
                else: atomic_write(consent, old_consent)
                reason = 'Channel setup failed; previous settings restored. Radio is Off.'
            except Exception:
                reason = 'Channel setup failed; recovery could not be confirmed. Radio is Off; inspect Diagnostics.'
            self._set_radio('error', reason)
            self._broadcast({'event': 'config.unknown'})
            raise RuntimeError(reason) from None
        profile = ''; profile_error = ''
        if plan['profile_name']:
            try: profile = self._settings.save_profile(plan['profile_name'], config, channels)
            except Exception: profile_error = 'Channel saved, but the optional profile was not saved. Save it in Mesh profiles.'
        result = {'event': 'channel.setup.saved', 'profile_only': False, 'profile': profile,
                  'profile_error': profile_error, 'verified': 'radio' if online else 'stored',
                  'added': len(plan['added']), 'channels': [{'index': i, 'identity': self._settings.identity(channels.channels[i], config.lora)} for i in plan['added']]}
        try: self._publish_history()
        except Exception:
            result['warning'] = 'Channel saved; conversation refresh failed. Reopen Messages and check storage before further changes.'
        return result
