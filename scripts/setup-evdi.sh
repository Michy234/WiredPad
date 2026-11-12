#!/usr/bin/env bash
# setup-evdi.sh: Ensure EVDI kernel module and user-space library are configured
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd -- "$SCRIPT_DIR/.." && pwd)"

echo "==> Setting up EVDI for WiredPad..."

# 1. User-space libevdi
LOCAL_LIB="$HOME/.local/lib"
mkdir -p "$LOCAL_LIB"

if [[ -f "$ROOT_DIR/build/libevdi.so.1" ]]; then
    echo "==> Copying compiled libevdi from build directory to $LOCAL_LIB..."
    cp -a "$ROOT_DIR/build/libevdi.so"* "$LOCAL_LIB/"
elif [[ -f "$ROOT_DIR/third_party/libevdi/evdi_lib.c" ]]; then
    echo "==> Compiling bundled libevdi..."
    DRM_CFLAGS=$(pkg-config --cflags libdrm 2>/dev/null || echo "-I/usr/include/libdrm")
    cc -I"$ROOT_DIR/third_party/libevdi" $DRM_CFLAGS -std=gnu99 -fPIC -D_FILE_OFFSET_BITS=64 \
        -c -o /tmp/evdi_lib.o "$ROOT_DIR/third_party/libevdi/evdi_lib.c"
    cc -I"$ROOT_DIR/third_party/libevdi" $DRM_CFLAGS -std=gnu99 -fPIC -D_FILE_OFFSET_BITS=64 \
        -c -o /tmp/evdi_procfs.o "$ROOT_DIR/third_party/libevdi/evdi_procfs.c"
    cc /tmp/evdi_lib.o /tmp/evdi_procfs.o -shared -Wl,-soname,libevdi.so.1 -o "$LOCAL_LIB/libevdi.so.1.15.1" -lc
    ln -sf libevdi.so.1.15.1 "$LOCAL_LIB/libevdi.so.1"
    ln -sf libevdi.so.1.15.1 "$LOCAL_LIB/libevdi.so"
    rm -f /tmp/evdi_lib.o /tmp/evdi_procfs.o
    echo "==> Installed libevdi.so.1 to $LOCAL_LIB"
fi

# 2. Kernel module
if lsmod | grep -q "^evdi "; then
    echo "==> EVDI kernel module is already loaded."
else
    echo "==> EVDI kernel module is not loaded."
    if modprobe -n evdi >/dev/null 2>&1; then
        echo "==> Module available in system, loading with initial_device_count=1..."
        sudo modprobe evdi initial_device_count=1 || echo "Warning: Could not modprobe evdi (check permissions)"
    else
        echo "==> Module not found in kernel tree. Building latest DisplayLink EVDI (supports modern kernels)..."
        TEMP_DIR=$(mktemp -d /tmp/evdi-build-XXXXXX)
        git clone --depth 1 https://github.com/DisplayLink/evdi.git "$TEMP_DIR/evdi"
        cd "$TEMP_DIR/evdi/module"
        make
        echo "==> Loading built module into kernel..."
        sudo insmod evdi.ko initial_device_count=1 || sudo modprobe evdi initial_device_count=1 || true
        rm -rf "$TEMP_DIR"
    fi
fi

# 3. Verify EVDI DRM card
CARDS=()
for p in /sys/class/drm/card*/device/driver; do
    if [[ -e "$p" && $(basename "$(readlink -f "$p")") == "evdi" ]]; then
        CARDS+=("$(basename "$(dirname "$(dirname "$p")")")")
    fi
done

if [[ ${#CARDS[@]} -gt 0 ]]; then
    echo "==> SUCCESS: EVDI device detected: ${CARDS[*]}"
    echo "==> WiredPad is ready to use!"
else
    echo "==> Note: No EVDI DRM card detected yet. Load the module with: sudo modprobe evdi initial_device_count=1"
fi
