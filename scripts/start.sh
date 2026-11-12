#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
if [[ ! -S /var/run/usbmuxd ]]; then
    echo 'Socket usbmuxd assente. Avvia: sudo systemctl start usbmuxd.service' >&2
    exit 1
fi
if [[ $# == 0 ]]; then
    cards=()
    for path in /sys/class/drm/card[0-9]*/device/driver; do
        [[ -e "$path" ]] || continue
        if [[ $(basename "$(readlink -f "$path")") == evdi ]]; then
            name=$(basename "$(dirname "$(dirname "$path")")")
            cards+=("${name#card}")
        fi
    done
    if [[ ${#cards[@]} != 1 ]]; then
        echo 'Carica evdi e specifica la card: scripts/start.sh --evdi-card N' >&2
        exit 1
    fi
    set -- --evdi-card "${cards[0]}"
fi
# Reuse the locally compiled EVDI library during development if present.
if [[ -f "$root/.research/evdi/library/libevdi.so.1" ]]; then
    export LD_LIBRARY_PATH="$root/.research/evdi/library${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
exec "$root/build/wiredpad-host" "$@"
