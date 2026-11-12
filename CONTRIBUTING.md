# Contributing

Build with CMake and run `ctest --test-dir build --output-on-failure` before
submitting changes. Keep the desktop app optional and the host usable on its own.
Use the existing C++20, Qt and FFmpeg dependencies rather than adding a framework.

For a bug report, include your distribution, desktop session, kernel, tablet and
client-app version, plus relevant text from the app's Diagnostics panel.
Do not post pairing records, credentials or unreviewed desktop captures publicly.

Protocol changes need evidence from the compatible client. Distinguish static
inferences, synthetic tests and observations from hardware. Small regression tests
should cover parser changes, input state and encoder compatibility.

A two-finger command has been observed, but its semantics and cross-version
behavior still need validation. Do not label gesture support or raw multitouch
as tested until it has been exercised on the device.
