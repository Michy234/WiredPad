#!/usr/bin/env bash
# The driver must be installed persistently; never insmod a temporary build.
set -euo pipefail

if ! modinfo evdi >/dev/null 2>&1; then
    printf '%s\n' \
        "EVDI is not installed for kernel $(uname -r)." \
        'Install evdi-wiredpad-dkms and the matching kernel headers (linux-zen-headers for linux-zen).' \
        'Check the pacman DKMS build output, then run: sudo dkms autoinstall' >&2
    exit 1
fi

if (( EUID == 0 )); then
    modprobe evdi initial_device_count=1
else
    sudo modprobe evdi initial_device_count=1
fi
printf '%s\n' 'EVDI loaded. Open WiredPad and enable the tablet output in your display settings.'
