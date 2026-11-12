# Setup

WiredPad is a desktop-session app. Run it as your normal user, not as root.

## Arch Linux

WiredPad includes and builds the user-space `libevdi` library automatically, so
a separate library package is not required. You only need the EVDI kernel module.

If you are running a recent kernel (such as Linux 6.12+ or 7.x / linux-zen),
use the automated setup script to build and configure the matching driver:

```sh
./scripts/setup-evdi.sh
# or after package installation:
wiredpad-setup-evdi
```

Alternatively, install `evdi-dkms` and headers for your running kernel (`uname -r`).

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

On Plasma Wayland, WiredPad automatically maps its pointer and touch devices to
the EVDI output. The **Input display** selector overrides the automatic choice.
The app uses a mapped absolute pen for legacy mouse commands, so dragging works
without moving input to the main screen. HID multitouch uses a separate device.

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
Configure persistent driver loading through your distribution's EVDI package.

## Troubleshooting

- **No EVDI display:** check module/library versions, kernel headers and `lsmod`.
- **No iPad:** check usbmuxd, USB trust, pairing and that the client app is open.
- **Connected, no output:** enable the virtual display in desktop settings.
- **Wrong input screen:** select the tablet output in WiredPad; check mapping.
- **More than one EVDI card:** use the host CLI to select a dedicated card.
- **Disconnected:** keep the app running; the host retries the USB connection.

Expand **Diagnostics** in the app for host messages. Never attach pairing records
from /var/lib/lockdown to a bug report. If protocol behavior differs, see
[protocol notes](../PROTOCOL_IOS.md) and [Windows capture guide](windows-capture.md).
