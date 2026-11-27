# Setup

WiredPad is a desktop-session app. Run it as your normal user, not as root.

## Arch Linux

Install `evdi-wiredpad-dkms` from `packaging/arch/evdi-wiredpad-dkms` before
building WiredPad, or install both prebuilt packages together with `pacman -U`.
It is a required WiredPad dependency and replaces other EVDI kernel packages.
The module sources are pinned to upstream commit
`8acd0b28230d271cc338bc3b2f201fb696dbd286` (1.15.1, with kernel API detection),
the same source used in the original hardware test; no temporary `.ko` is needed.
The user-space `libevdi` library is still supplied by WiredPad.

Install headers matching **each kernel you use**: `linux-headers`,
`linux-zen-headers`, `linux-lts-headers`, or the equivalent for your custom kernel.
The `LINUX-HEADERS` dependency enforces a header provider; it cannot ensure that
the provider matches every installed kernel. For linux-zen:

```sh
sudo pacman -Syu --needed linux-zen-headers
```

DKMS's pacman hooks build and install the module for kernels with headers.
Inspect the transaction output for failures. The driver package supplies
`/usr/lib/modules-load.d/evdi-wiredpad.conf` and
`/usr/lib/modprobe.d/evdi-wiredpad.conf`: EVDI loads at boot with
`initial_device_count=1`. Do not unload a module in use by your desktop.

After a kernel update, reboot into the updated kernel. To load EVDI immediately
for your running kernel (if its matching headers are installed):

```sh
wiredpad-setup-evdi
```

This helper checks the installed module and runs modprobe; it does not clone,
compile or load an uninstalled driver. An old manually loaded module is replaced
on reboot, rather than being unloaded from a running desktop.

Start the USB service, connect and unlock the iPad, and accept the trust prompt:

```sh
sudo systemctl start usbmuxd.service
idevicepair pair
idevicepair validate
```

Open WiredPad. Use **Load display driver** if EVDI is installed but not loaded;
the desktop will ask for administrator authentication. Or load it manually:

```sh
sudo modprobe evdi initial_device_count=1
```

The Arch package installs a uinput rule for the active local desktop session and
loads uinput at boot. If input permissions have not updated, log out and back in.
For video only, uncheck **Enable tablet input**.

## Desktop display and input

Open Wired XDisplay on the iPad and press **Start display** in WiredPad. Enable
the new display in your desktop's display settings, initially without rotation
or fractional scaling. Move a window onto it.

On Plasma Wayland, WiredPad maps **WiredPad Touch** to the tablet output.
If a touch lands on another monitor, select the tablet connector under **Input display**.
Clients that send press, move and release commands provide one touch contact.
Clients that report individual contact IDs can provide up to ten simultaneous touches.

On X11, install xinput and a suitable EVDI/Xorg configuration. Load EVDI before
starting Xorg. The app maps input with xinput; you may need to link GPU providers:

```sh
xrandr --listproviders
xrandr --setprovideroutputsource EVDI_PROVIDER_ID GPU_PROVIDER_ID
xrandr --auto
```

Use the actual IDs from your session. On other Wayland desktops, configure input
mapping in the compositor. Only Plasma Wayland has been tested on hardware.

## Command line and optional service

```sh
wiredpad-host                       # detect a single EVDI card
wiredpad-host --evdi-card 0 --fps 30 # select a card explicitly
wiredpad-host --probe               # USB handshake only
wiredpad-host --test-pattern        # diagnostic video, no input injection
wiredpad-host --no-input            # desktop video only
wiredpad-map-input DVI-I-1          # manual Plasma input mapping
```

Run only one host per user. Closing the app stops the host it started. For an
advanced session without the window, the package also ships a user unit:

```sh
systemctl --user daemon-reload
systemctl --user start wiredpad.service
systemctl --user stop wiredpad.service
```

It is not enabled automatically. Do not run the unit and the app's host together.
The Arch driver package configures persistent loading automatically.

## Troubleshooting

- **No EVDI display after reboot:** run `dkms status`, `modinfo evdi`, and
  `journalctl -b -u systemd-modules-load`. If the module is missing for `uname -r`,
  install that kernel's matching headers and run `sudo dkms autoinstall`.
  Build errors are logged in `/var/lib/dkms/evdi/1.15.1.wiredpad1/`.
  DKMS also needs your usual module-signing setup if Secure Boot enforces it.
  Check `/etc/modprobe.d/` for old options overriding `initial_device_count=1`.
- **No iPad:** check usbmuxd, USB trust, pairing and that the client app is open.
- **Connected, no output:** enable the virtual display in desktop settings.
- **Wrong input screen:** select the tablet output in WiredPad; check mapping.
- **More than one EVDI card:** use the host CLI to select a dedicated card.
- **Disconnected:** keep the app running; the host retries the USB connection.

Expand **Diagnostics** in the app for host messages. Never attach pairing records
from /var/lib/lockdown to a bug report. If protocol behavior differs, see
[protocol notes](../PROTOCOL_IOS.md) and [Windows capture guide](windows-capture.md).
