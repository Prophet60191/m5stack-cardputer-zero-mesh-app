# Installing, updating and recovering Mesh

These instructions cover the **0.1.19-and-later bundled-runtime implementation** on the
official **Debian 13 arm64 CardputerZero image**, with its normal systemd user
session and hardware permissions. Mesh is a Linux app, not a firmware flasher.
Use the Cap LoRa-1262 and attach its antenna **before Radio On**.

## 1. Get the correct installer

At this documentation update, 0.1.22 is a **source/package candidate**, not a public
release or Store listing. Older published candidates do not include this new
dependency arrangement. Check [current status](current-status.md) and
[current UI qualification](ui-checkup.md) before choosing a file.

The source-correlated candidate build is [CI run 45500604820](https://github.com/Prophet60191/m5stack-cardputer-zero-mesh-app/actions/runs/45500604820),
artifact **cardputerzero-mesh-debian13-arm64**. Require a successful run before
using its output. CI artifacts and the exact locally built, installed package
have separate hashes; see [the device record](ui-checkup.md#final-package-and-device-result)
for the latter. The older [0.1.19 record](distribution-validation-20261123.md)
remains evidence for its separately qualified RF behavior. Signed-in GitHub users can download
that artifact in a browser. With the optional GitHub CLI on your computer:

```sh
gh run download 45500604820 \
  --repo Prophet60191/m5stack-cardputer-zero-mesh-app \
  --name cardputerzero-mesh-debian13-arm64 --dir mesh-0.1.22
cd mesh-0.1.22
sha256sum -c SHA256SUMS
```

On macOS, use `shasum -a 256 -c SHA256SUMS`. The complete CI artifact includes
the large corresponding-source archive. Once published, individual assets will
be available on the [Releases page](https://github.com/Prophet60191/m5stack-cardputer-zero-mesh-app/releases).
Ordinary users need the `.deb`; developers/redistributors also need its paired
source and license material. Actions artifacts expire; they are not permanent
release hosting. A private draft requires maintainer access.

The qualified installer is `cardputerzero-mesh_0.1.22-rev1_arm64.deb`:

```text
SHA-256: feb0579ef8517b3629d10c8ac6e098751e9c85c6c02727e0bccbdfd190057a61
Source: 8ada52add1b9fb19524002daba5adf1d054ade97
```

If you downloaded only that installer, run `sha256sum` (or macOS `shasum -a 256`)
on it and compare the complete value above. Other same-version development
builds can have different bytes; version alone is not verification. Release
`SHA256SUMS` covers installer/source assets; `GUIDE_SHA256SUMS` separately covers
the PDF manuals. A checksum set can only verify files you actually downloaded.

## 2. Prepare the device

For an upgrade, turn **Radio Off**, close Mesh, and use the backup procedure below
before replacing files. Existing chats, drafts, channels, saved mesh setups and
preferences are retained. New installs do not contain someone else's keys or
radio profile. Do not erase/reimage a working card to install this app.

If you previously installed an independent global `meshtasticd` package, stop
and mask **that standalone service** before using Mesh:

```sh
sudo systemctl mask --now meshtasticd.service
```

This applies to an existing separate daemon, not the Mesh supervisor or firewall.
A fresh installation does not need that global package and should not install
it. Do not unmask it for Mesh: two owners must not operate the same Cap.

Copy the `.deb` to the device using your usual download/transfer method. For SSH,
replace `YOUR_USER` and `YOUR_DEVICE` with your own working login and host:

```sh
scp cardputerzero-mesh_0.1.22-rev1_arm64.deb YOUR_USER@YOUR_DEVICE:~/
```

An SSH server, VPN or USB/ADB connection is not required by Mesh itself. The
installer requires administrator privileges; the running app does not.

### Platform interfaces and permissions

The package uses the image's existing driver/permission setup; a wheel bundle
cannot supply a missing kernel overlay or grant hardware access. The normal user
needs framebuffer/input access for the UI, read/write access to `/dev/spidev0.1`,
`/dev/gpiochip0`, `/dev/i2c-1`, the fitted GPS serial interface `/dev/serial0`, and
the Cap switches under `/sys/class/leds/ext_5v_out/brightness` and
`/sys/class/leds/ext_usb_gpio_fun/brightness`. Camera access is optional.

For a permission/missing-interface error, inspect without changing hardware:

```sh
id
ls -l /dev/fb0 /dev/spidev0.1 /dev/gpiochip0 /dev/i2c-1 /dev/serial0
ls -l /sys/class/leds/ext_5v_out/brightness \
  /sys/class/leds/ext_usb_gpio_fun/brightness
```

Use the image's supported groups/udev setup for those actual owners. Where the
nodes belong to `spi`, `gpio` and `i2c`, follow that group-membership guidance and
log in again; do not assume every image uses identical ownership. Do not make
hardware globally writable, grant blanket sudo or run the app as root. Preserve
unrelated drivers and services. GPIO already claimed by another app is an
ownership conflict, not a reason to force-unbind it. After NFC use, follow the
[user guide's full power-cycle advice](user-guide/01-Introduction-and-Getting-Started.md#before-you-start-hardware-care)
before LoRa. Fresh-image hardware permissions are part of the explicitly deferred
physical qualification, not something the minimal Debian container proves.

## 3. Install

In a terminal on the Cardputer, from the directory containing the installer:

```sh
sudo apt install ./cardputerzero-mesh_0.1.22-rev1_arm64.deb
```

If this version is already installed, use `sudo apt install --reinstall ./cardputerzero-mesh_0.1.22-rev1_arm64.deb` to replace it with these verified bytes or repair package files. Normal install may otherwise leave a same-version package alone.

Keep `./` so APT uses the local file. APT installs missing declared Debian
packages; refresh its indexes with `sudo apt update` if the configured image's
indexes are stale. Standard OS dependencies may require Internet access or your
own prepared Debian package cache. This is not an offline OS installer.

The package includes the private GPS-corrected radio executable and 15 pinned
Python wheels. **Do not install meshtasticd separately or run pip manually.**
Python 3.13 is a Pre-Depends requirement because the pre-install stop helper uses
it. Python venv support, Pillow, zbar, fonts and native shared libraries are
resolved through Debian dependencies. Root scripts stop only this app's user
supervisor before replacing its files and configure the limited firewall monitor.
They do not start the radio, sign into accounts or prepare a user environment as root.

## 4. First launch and radio setup

1. Attach the LoRa antenna, then open **Mesh** from the normal launcher as your
   regular device user. Do not use `sudo` to launch the UI or supervisor.
2. On its first launch, Mesh checks the installed wheels and builds a private
   managed Python environment offline. Give it time to finish; progress/errors
   are available in the user-service journal. Later launches reuse the cache.
3. A new user's first-run setup asks for the permitted region and a public or
   newly generated private primary channel. It starts in **receive-only** mode.
   An existing setup keeps its saved transmission and privacy preferences.
4. Match region, modem preset, frequency and channel/key to your intended mesh.
   Use **Settings → Add a channel** for the grouped configuration form or
   **Saved mesh setups** for an existing profile. The
   [Settings guide](user-guide/05-Settings-Menu.md) explains the fields and Save.
5. Enable **Transmit mode** deliberately when ready, with the antenna attached.
   Automatic location sharing and Internet/MQTT forwarding are separate choices;
   installation does not opt you into them.

Closing the UI leaves an enabled radio receiving in the background. Before
swapping/removing the Cap, turn Radio Off. Auto-start follows your saved setting;
opening a second UI must not restart an already active supervisor.

Check **Settings → Diagnostics** for app 0.1.19, client 2.7.11, the qualified
2.7.26 native build, connected API and healthy firewall. The header shows the
actual radio/mode and battery percentage without a battery icon. Try one
harmless message with an intended peer; distinguish a mesh acknowledgment from
a recipient confirmation. The [user guide](user-guide/README.md) covers operation.

## Optional camera and phone features

| Feature | Included | Separate platform/account requirement |
|---|---|---|
| QR decode | pyzbar wheel; APT dependencies Pillow and zbar | CSI capture program `rpicam-still`, supplied by the image's `rpicam-apps-core`; USB/V4L2 fallback additionally uses `python3-opencv` |
| Fitted Cap GPS | Native identification-timeout correction and app position refresh | Supported fitted receiver, GPS enabled, clear sky for a fix; USB GPS is not automatically selected/supported |
| Official phone app | Messaging/viewing gateway in the supervisor | Tailscale installed/signed in on both devices, private Serve configuration, selected phone and official Meshtastic app |

Pasting a channel link works without a camera. Do not run another GPS/radio
application against the same hardware while Mesh owns it. For phone setup,
follow [Phone access](phone-access.md); do not expose the daemon or intent socket
on the LAN/Internet. No device SIM is needed for its existing Wi-Fi/VPN path.

## Service controls and logs

Run these as the **logged-in device user**, without sudo, in its normal terminal:

```sh
python3 /usr/lib/cardputerzero-mesh/user_service.py status
python3 /usr/lib/cardputerzero-mesh/user_service.py ensure
journalctl --user -u cardputerzero-mesh-supervisor.service -n 80 --no-pager
python3 /usr/lib/cardputerzero-mesh/firewall.py health
```

`ensure` starts an absent service, but leaves an active one alone. `stop` ends
reception and powers down through the guardian. `restart` explicitly stops and
starts the owner; it can interrupt pending work and applies the saved startup
policy, so check Radio in the UI afterward. It is not a radio-On command.
The service is transient, so an inactive/unknown unit before first launch is
normal. The system firewall service is separate:

```sh
systemctl status cardputerzero-mesh-firewall.service --no-pager
```

A remote shell without the logged-in user's systemd session/runtime bus cannot
manage that user unit reliably. Use the normal device session; do not work around
this by running Mesh as root. Journal/raw radio logs and backups can contain
private information: prefer the app's sanitized Diagnostics export for reports.

## Backup before upgrade or rollback

Turn Radio Off, close the UI, and stop the user supervisor first:

```sh
python3 /usr/lib/cardputerzero-mesh/user_service.py stop
```

With the writer stopped, archive these directories to a private location:

| Path | Contents |
|---|---|
| `~/.local/state/cardputerzero-radio/` | Radio preferences/identity, channel keys, profiles, phone preference, SQLite conversations/drafts/pins and runtime records |
| `${XDG_CONFIG_HOME:-$HOME/.config}/cardputerzero-mesh/` | App INI preferences, including theme, quick messages and startup choices |
| `/etc/cardputerzero-mesh.conf` | Package/platform configuration; preserve any administrator customization separately |

For the default XDG config location, if both user directories already exist:

```sh
umask 077
mkdir -p "$HOME/mesh-backups"
tar -C "$HOME" -czf "$HOME/mesh-backups/mesh-$(date +%Y%m%d-%H%M%S).tar.gz" \
  .local/state/cardputerzero-radio .config/cardputerzero-mesh
```

If you use a custom XDG config root, archive that actual directory instead. If a
first-run directory does not exist yet, omit it; inspect tar's exit status and
list/test the archive before relying on it. Backups contain secrets and personal
history: keep them private, never attach them to issues or commit them to Git.
Do not copy a live SQLite database file by itself; stop its writer as above or
use SQLite's consistent backup facility.

The managed Python cache is rebuildable and does not need to be part of a data
backup. Retain a legacy `venv/` if you need rollback to a version that used it.

For rollback, retain the previous verified installer **and its prerequisites**.
Stop Radio/UI/supervisor, install that specific local package through APT, and
check the live version/configuration afterward. A pre-0.1.19 version needs its
old client environment and qualified global daemon, still masked as a system
service. A bundled-only fresh system does not automatically satisfy that older
arrangement. Do not restore an entire old data archive over new legitimate chats
or settings merely to make hashes match. Restore only deliberately, with the
writer stopped and correct user ownership/private permissions; keep a backup of
the current state and review compatibility first.

### Deliberate data restoration (default config paths)

Use only your own trusted archive and a compatible app/schema. Restoring replaces
history, identity, profiles and preferences with that snapshot; it can restore
old transmit/location/auto-start choices too. Stop the UI/owner first, back up the
current state, inspect `tar -tzf /path/to/your-backup.tar.gz`, then extract privately:

```sh
umask 077
mesh_restore_stage="$(mktemp -d)"
tar -xzf /path/to/your-backup.tar.gz -C "$mesh_restore_stage"
```

Inspect the staging directories before replacing anything. For the two default
user directories, when both staged and current directories exist:

```sh
mesh_restore_stamp="$(date +%Y%m%d-%H%M%S)"
mkdir -p "$HOME/mesh-backups"
mv "$HOME/.local/state/cardputerzero-radio" \
  "$HOME/mesh-backups/state-before-restore-$mesh_restore_stamp"
mv "$HOME/.config/cardputerzero-mesh" \
  "$HOME/mesh-backups/config-before-restore-$mesh_restore_stamp"
cp -a "$mesh_restore_stage/.local/state/cardputerzero-radio" "$HOME/.local/state/"
cp -a "$mesh_restore_stage/.config/cardputerzero-mesh" "$HOME/.config/"
chmod 700 "$HOME/.local/state/cardputerzero-radio"
```

Skip a move only if that current directory is absent; do not copy a missing or
unreviewed staged directory. Adapt both paths for custom XDG config. Run as the
normal user so restored files belong to that user; do not merge into a live state
tree or use sudo. Retain the displaced directories until verification succeeds.
With antenna attached and the intended restored settings understood, launch Mesh
and check chats, profile, region, mode, location/MQTT and phone selection. Keep
Radio Off if the backup is incompatible or its settings are uncertain.

## Recovery

| Symptom | Action |
|---|---|
| First launch is still preparing | Allow the bounded setup to complete; inspect the user journal. Do not start a second manual pip install. |
| Incomplete cache or detected import/version/ready-marker failure | Restart through the packaged service. It rebuilds its fixed cache from bundled wheels offline; it does not delete history or the legacy venv. |
| Managed directory/lock ownership or permissions unsafe | Stop the service and review the private cache paths below. Unsafe paths fail closed; reinstalling root-owned package files does not repair user-directory ownership. |
| Bundle checksum, unsafe permissions, missing wheel or executable | Reinstall the verified app `.deb`; do not edit manifests, chmod broad paths, substitute a PATH binary or install newer client versions. |
| Package installation incomplete/update marker present | Complete package configuration with `sudo dpkg --configure -a`; if APT reports missing dependencies, review/run `sudo apt --fix-broken install`. Those commands can finish other pending package work too. Never manually remove the update marker to bypass the block. |
| Python version/ABI or missing Debian dependency | Confirm supported Debian 13 arm64/Python 3.13 and use APT with the image's supported repositories. Do not force a wrong-architecture install. |
| User bus unavailable / root refused | Run lifecycle commands in the normal logged-in device user's session; launch from the grid. |
| Radio fails firewall or hardware initialization | Keep Radio Off, inspect Diagnostics and the sanitized health result, check antenna/Cap/ownership. Do not bypass the firewall, unmask a competing daemon or change unrelated service permissions. |
| Camera unavailable | Check the image's capture provider/permissions; close competing camera apps or paste the link. Installing a Python QR wheel manually is unnecessary. |
| Phone needs reconnect after a restart | Reconnect its saved TCP entry if necessary; follow the phone guide. Restart recovery is not a guarantee of automatic handset reconnection. |

### Repair package files or reset only the generated cache

For missing/corrupt **package-owned** files, after Radio Off/close/backup:

```sh
sudo apt install --reinstall ./cardputerzero-mesh_0.1.22-rev1_arm64.deb
```

For a **user managed-cache** problem, first stop its service. If the app's data
parent is the correct private/owned location and the problem is limited to this
generated cache, move the whole managed cache aside instead of deleting data:

```sh
python3 /usr/lib/cardputerzero-mesh/user_service.py stop
umask 077
mesh_cache_parent="${XDG_DATA_HOME:-$HOME/.local/share}/cardputerzero-mesh"
ls -ld "$mesh_cache_parent" "$mesh_cache_parent/managed-runtimes"
mv "$mesh_cache_parent/managed-runtimes" \
  "$mesh_cache_parent/managed-runtimes.saved-$(date +%Y%m%d-%H%M%S)"
python3 /usr/lib/cardputerzero-mesh/user_service.py ensure
```

Review the `ls` result before the move. The parent must belong to your normal
user and not be group/world writable; the managed directory should be private
(mode 0700). If paths/ownership are unexpected or the move is denied, resolve
that account/image problem rather than applying broad sudo/chown/chmod commands.
A missing cache needs no move. Reopen from the normal launcher after preparation,
with the antenna attached; check Radio and diagnostics. Keep the moved cache for
inspection until satisfied. Do not move/delete the sibling legacy `venv/` or
`~/.local/state/cardputerzero-radio/` as a cache remedy. This reset is optional
recovery, not part of a normal installation or update.

## Remove the app

Turn Radio Off, close the UI and stop its user supervisor. Then:

```sh
sudo apt remove cardputerzero-mesh
# Optional: also remove package-owned conffiles
sudo apt purge cardputerzero-mesh
```

Removal/purge stops this package's services and removes its installed runtime;
it intentionally retains per-user history, keys, profiles, managed caches and a
legacy venv. It does not uninstall shared system libraries, Tailscale or a
separately installed global radio package. Do not run autoremove blindly on a
shared device. If you want personal data erased, back it up/review it separately;
package purge is not a privacy wipe. Disable only this feature's Serve forwarder
using the phone guide if you no longer want it; preserve unrelated Serve entries.

## Optional USB/ADB transfer

First follow the backup and Radio Off/close steps above. Use a data cable in the computer/data port and the selector in **Slave** mode,
with Developer → ADB enabled intentionally. Copy the verified package:

```sh
adb devices
adb push cardputerzero-mesh_0.1.22-rev1_arm64.deb /tmp/cardputerzero-mesh.deb
adb shell sha256sum /tmp/cardputerzero-mesh.deb
adb shell apt install /tmp/cardputerzero-mesh.deb
```

Compare the copied hash before installing. On this image ADB is a root shell:
use it for package administration, then launch Mesh through the normal device
launcher. Do not start its UI or user supervisor from the root ADB session.
Manage ADB according to your own access policy; these instructions do not toggle
it automatically.

## Qualification limits

The exact existing-device upgrade, minimal clean Debian container and offline
runtime checks pass as [recorded](distribution-validation-20261123.md). No spare
microSD was available: fresh-image **hardware** qualification remains deferred.
The owner video/publication steps are separate. USB GPS/SIM work and the earlier
phone reliability/public-peer/IPv6/full keyboard limits remain explicit.
