#!/usr/bin/env bash
set -euo pipefail
output=${1:?Uso: wiredpad-map-input DVI-I-1}
command -v qdbus6 >/dev/null || { echo 'Serve qdbus6 (pacchetto qt6-tools).' >&2; exit 1; }
base=/org/kde/KWin/InputDevice
interface=org.kde.KWin.InputDevice
found=0
while IFS= read -r device; do
    [[ -n "$device" ]] || continue
    name=$(qdbus6 org.kde.KWin "$base/$device" org.freedesktop.DBus.Properties.Get "$interface" name)
    case "$name" in
        'WiredPad Touch'|'WiredPad Pointer')
            qdbus6 org.kde.KWin "$base/$device" org.freedesktop.DBus.Properties.Set "$interface" outputName "$output"
            actual=$(qdbus6 org.kde.KWin "$base/$device" org.freedesktop.DBus.Properties.Get "$interface" outputName)
            [[ "$actual" == "$output" ]] || { echo "Mapping non applicato: $name" >&2; exit 1; }
            printf '%s -> %s\n' "$name" "$actual"
            found=$((found+1))
            ;;
    esac
done < <(qdbus6 org.kde.KWin "$base" org.kde.KWin.InputDeviceManager.devicesSysNames)
[[ $found -gt 0 ]] || { echo 'Avvia prima il demone con input abilitato.' >&2; exit 1; }
