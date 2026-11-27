<p align="center"><img src="assets/icon.svg" width="88" height="88" alt="WiredPad"></p>
<h1 align="center">WiredPad</h1>
<p align="center">Use your tablet as an extra display on Linux, over USB.</p>

WiredPad is a small Linux desktop app for connecting a compatible iPad as an
extra screen. Start or stop sharing from one window, choose a frame rate, and
use the tablet as a touchscreen. The host runs locally over USB.

The current backend works with the **Splashtop Wired XDisplay iPad client**,
which is installed separately. This is an independent project.

## Get started

The x86_64 Flatpak is available as a bundle in [GitHub Releases](https://github.com/Michy234/WiredPad/releases).
Download `WiredPad-x86_64.flatpak` and install it:

```sh
flatpak install --user ./WiredPad-x86_64.flatpak
```

The KDE runtime comes from Flathub. On the host, install and load the EVDI
driver, make `/dev/uinput` writable by your desktop session for touch, and
start `usbmuxd` before opening WiredPad. Restart WiredPad if the USB service is
restarted. The bundle uses direct access to devices for EVDI and touch, and
talks to KWin to map the touchscreen to the tablet display.

On Arch Linux, clone the project and build the package:

```sh
git clone https://github.com/Michy234/WiredPad.git
cd WiredPad
# Use linux-headers instead for the stock linux kernel, or linux-lts-headers for LTS.
sudo pacman -Syu --needed linux-zen-headers
(cd packaging/arch/evdi-wiredpad-dkms && makepkg -si)
(cd packaging/arch && makepkg -si)
```

WiredPad depends on `evdi-wiredpad-dkms`, the pinned EVDI driver used by this
project. DKMS rebuilds it when kernels are installed or updated, and the package
loads one virtual display at boot. Keep headers installed for each kernel you
use. WiredPad builds its user-space `libevdi` library automatically.

For prebuilt downloads, install both packages in the same pacman transaction:

```sh
sudo pacman -U ./evdi-wiredpad-dkms-*.pkg.tar.zst ./wiredpad-0.1.0-3-*.pkg.tar.zst
```

For local files pacman needs both packages; it cannot download the companion
package unless it is available in a configured package repository. Load EVDI
immediately with `sudo modprobe evdi`, or reboot after installation.

Then:

1. Start `usbmuxd.service`, connect and unlock the iPad, and trust the computer.
2. Open Wired XDisplay on the tablet and **WiredPad** from your application menu.
3. Press **Start display**. Enable the new output in your desktop's display settings.

On Plasma Wayland, input is automatically associated with the tablet display.
Use the **Input display** selector if you need to choose it manually.

See [setup and troubleshooting](docs/setup.md) for pairing, driver loading,
permissions, X11 configuration and command-line use. Prebuilt Arch packages are
available in [Releases](https://github.com/Michy234/WiredPad/releases).

## What works today

- USB device detection, pairing validation and automatic reconnection.
- A virtual EVDI display at the resolution reported by the tablet.
- JPEG video encoding with FFmpeg, and direct touchscreen input through uinput.
- Video updates, taps and window dragging tested on **Plasma Wayland**.
- Native Qt app with system tray minimization, desktop launcher and host daemon.

Hardware testing currently covers an iPad mini with iOS 9.3.6 and Wired XDisplay
1.2.3. Other iOS versions and X11 need more testing. Android is not supported.
The tested client's two-finger gesture sends a separate command that is not yet
handled; raw multitouch parsing exists but has not been validated on hardware.

## Build on other Linux distributions

Install a C++20 compiler, CMake, pkg-config, Qt 6 Widgets/DBus/SVG, libusbmuxd,
libimobiledevice, libplist and FFmpeg development packages:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
sudo cmake --install build
```

The desktop session needs access to its EVDI device and `/dev/uinput`.
The Arch package supplies a udev rule for the active local session; other
distributions may need equivalent configuration. EVDI must support your kernel.

For a host without the desktop app, set `-DWIREDPAD_BUILD_APP=OFF`.

## Using the host directly

```sh
wiredpad-host                 # detect a single EVDI card
wiredpad-host --probe         # check the USB handshake
wiredpad-host --test-pattern  # diagnostic video without input injection
wiredpad-host --help
```

Only one host runs per user. The app stops the host it owns when you close it.
The optional `wiredpad.service` is a **user** unit and is not enabled on install.

## Project notes

The implementation is based on static analysis with REA and a working Windows
capture, followed by tests on real hardware. The active client uses complete
JPEG frames; the original H.264 assumption was corrected by the capture.

- [Protocol notes](PROTOCOL_IOS.md)
- [Hardware tests and known limits](docs/testing.md)
- [Capturing a reference session](docs/windows-capture.md)
- [Contributing](CONTRIBUTING.md)

No proprietary installer, client app, pairing records, packet captures or
captured desktop images are included in this repository.

## License

GPL-3.0-or-later. The EVDI API header is LGPL-2.1-or-later; its license and
attribution are included in [third_party](third_party).
