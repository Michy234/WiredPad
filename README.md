<p align="center"><img src="assets/icon.svg" width="88" height="88" alt="WiredPad"></p>
<h1 align="center">WiredPad</h1>
<p align="center">Use your tablet as an extra display on Linux, over USB.</p>

WiredPad is a small Linux desktop app for connecting a compatible iPad as an
extra screen. Start or stop sharing from one window, choose a frame rate, and
use the tablet to point, click and drag. The host runs locally over USB.

The current backend works with the **Splashtop Wired XDisplay iPad client**,
which is installed separately. This is an independent project.

## Get started

On Arch Linux, clone the project and build the package:

```sh
git clone https://github.com/Michy234/WiredPad.git
cd WiredPad/packaging/arch
makepkg -si
```

Install a compatible **EVDI kernel module and libevdi**, along with headers for
your running kernel. This driver is required to create the extra display.
WiredPad does not download kernel code or upgrade your kernel automatically.

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
- JPEG video encoding with FFmpeg, and absolute pointer input through uinput.
- Video updates, taps and window dragging tested on **Plasma Wayland**.
- A native Qt app, desktop launcher and optional command-line host/user service.

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
