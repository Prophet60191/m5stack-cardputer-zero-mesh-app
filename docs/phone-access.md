# Phone access

Mesh 0.1.16 adds optional messaging and viewing through the **official Meshtastic
phone app**. It does not install a replacement phone application. The Cardputer
and phone remain usable together: the supervisor retains the sole connection to
meshtasticd and owns all radio/channel/privacy changes.

## Connection and security design

The daemon API stays on firewall-confined TCP 47400. A separate, unprivileged
PhoneAPI gateway listens on **127.0.0.1:47401 only**. Tailscale Serve forwards
**tailnet TCP 4403** to that gateway and supplies PROXY protocol v2 identity.
The gateway accepts only the device selected in Phone access. Its stable VPN
node identity and current source address are checked before configuration is
released; a device name or a guessed source header on the network is insufficient.
Do not configure a public port forward, Funnel, a LAN listener or a raw forward
to the daemon. The local Unix intent socket is never exposed.

Tailscale provides transport authentication and encryption, subject to your
account and tailnet access policy. The gateway adds a selected-device check;
it does not broaden ACLs or give the app administrator/operator privileges.
Root and other local programs remain outside this isolation boundary: as with
the existing loopback daemon, this is a single-owner handheld design, not a
sandbox against other local users. Revoke compromised VPN devices/account access.

Enabling access intentionally lets the selected phone receive channel keys,
mesh messages, node identities and positions. They are sensitive on the phone
as well as the Cardputer. Disabled channel-slot keys, host Wi-Fi/MQTT credentials, private signing/admin
keys, unqualified future protobuf fields, raw logs and local supervisor controls are excluded. This feature does
not transfer the Cardputer's saved conversation/draft database to the phone;
the official phone app maintains its own history and receives live traffic.

## One-time platform setup

Tailscale is an optional, separately installed dependency. Follow its
[official Debian instructions](https://pkgs.tailscale.com/stable/) for your
actual image, sign the Cardputer into the same tailnet as your phone, and keep
Tailscale connected on both. Do not put authentication keys in the app source.
For a device with working ordinary DNS, `tailscale up --accept-dns=false` avoids
changing the image's DNS settings; inspect your own network needs separately.
The Mesh package never installs Tailscale or signs into an account automatically.

An administrator configures this one private forwarder:

```bash
sudo tailscale serve --bg --tcp=4403 --proxy-protocol=2 tcp://127.0.0.1:47401
tailscale serve status --json
```

The `TCP` entry for `4403` must name `127.0.0.1:47401` and `ProxyProtocol: 2`.
Public Funnel must be off. Mesh checks these conditions before enabling access.
Do not reset unrelated Serve configurations. To remove just this forwarder:

```bash
sudo tailscale serve --tcp=4403 off
```

Tailscale Serve retains a background forwarder across restarts. When access or
the radio is Off, the application gateway is closed, so this forwarder cannot
serve Mesh data. A changed/missing VPN setup does not power off a working radio;
Phone access reports that setup is required and retries an absent gateway every
15 seconds while Radio is Online. An established listener does not poll VPN
status. Keep the ordinary daemon firewall.

## Connect the phone

1. Open **Settings → Phone access → Select phone** on the Cardputer. Choose the
   correct device from your Tailscale devices. Selecting a different device
   turns access Off until you deliberately enable it again.
2. Set **Access: On**. This does not start the radio, enable transmission, change
   a channel or enable location/Internet sharing. Turn Radio on separately with
   its antenna attached. Transmission follows the current radio mode.
3. Open **Connection details**. On the iPhone, turn Tailscale on and open the
   official Meshtastic app. Add a manual **TCP** connection using the displayed
   address and port **4403**. Use an optional friendly name of your choice.
4. The phone should load the node/channels and remain connected. Refresh Phone
   access to see its connection status. Tap Access to turn it Off when finished.

The address is learned from the current VPN, never hardcoded into this project.
Use its IPv4 address when following this recipe. Selection/access preferences
are stored privately. Access is device-wide and follows the active mesh setup;
turn it Off before using a setup that should not be shared with that phone. After restarting, a previously enabled gateway resumes
only when the radio is Online and the selected VPN device/forwarder still check
out. A failed preference write is reported; runtime access closes, but an old
saved value may remain. Retry the change before relying on its reboot behavior.

## Supported operations and honest results

- Live messages, node/position information, channels and a sanitized configuration
  snapshot; channel messages and DMs through the same target/TX/identity checks as
  the Cardputer. Text is limited to 200 UTF-8 bytes. No message is auto-replayed.
- Phone packet IDs remain intact for real radio queue/routing results. A mesh ACK,
  recipient receipt and a person reading a message remain different outcomes.
- Radio/channel/profile changes, phone-position uploads, remote administration,
  MQTT proxying, firmware updates and arbitrary encrypted/application packets
  are rejected. Use the Cardputer for configuration. Apple's automatic clock
  hint is denied without a recurring notification; neither clock is changed.
- A changed channel/tuning context invalidates the phone session before another
  send can use the old slot. Reconnect to load the current setup.
- An authorized new connection replaces only the old phone connection; it never
  takes the supervisor's daemon connection. Offline/slow/malformed clients have
  bounded buffers, deadlines, request rates and queues. Overflow disconnects the
  phone explicitly rather than silently dropping messages or stopping the radio.
- Frames received during the configuration handshake are buffered and sent after
  its snapshot. Idle heartbeats go to the actual daemon and use its actual reply;
  a local invented heartbeat response cannot conceal a failed radio API.

If a send is rejected or its result is unknown, inspect its status on the
Cardputer before resending. Repeated pending/completed phone IDs are not sent again. The iPhone app's explicit
Retry reuses its ID; a matching failed message may be retried after five seconds
with the same text/recipient/channel identity. Changing the payload or setup
requires a new message. The recent-ID window is bounded to 15 minutes in RAM; retained outgoing
history also protects those IDs after reconnect/restart. Opting out of history,
clearing it or expiring the RAM window limits that protection. This is not an
exactly-once guarantee across arbitrary failures.

## Troubleshooting and qualification

- **No endpoint / setup required:** verify both Tailscale sign-ins, Serve's exact
  target/PROXY version and your tailnet policy. Keep Funnel off.
- **Phone cannot connect:** check selected device, Access On, Radio Online,
  phone Tailscale On and the displayed IPv4 address/4403. A second phone is not
  implicitly authorized. Do not open the raw daemon port to work around this.
- **Phone connects but cannot send:** check receive-only mode, channel identity,
  message length and delivery results. Do not switch transmission on automatically.
- **Settings rejected on phone:** expected; configure the radio on the Cardputer.

[The qualification record](phone-access-validation.md) separates offline source/
protocol checks from installed-device, actual-iPhone and RF checks. A Python
client connection proves protocol interoperability, not the iPhone UI experience.

## Official references

- [Firmware's single-connection server](https://github.com/meshtastic/firmware/blob/master/src/mesh/api/ServerAPI.cpp).
- [Meshtastic protobuf protocol](https://github.com/meshtastic/protobufs/blob/master/meshtastic/mesh.proto).
- [Official Apple client](https://github.com/meshtastic/Meshtastic-Apple), especially
  its TCP transport, two-phase config/database nonces and heartbeat watchdog.
- [Tailscale Serve and PROXY protocol](https://tailscale.com/docs/reference/tailscale-cli/serve).

[Security](security.md) · [Supervisor contract](supervisor-protocol.md)
