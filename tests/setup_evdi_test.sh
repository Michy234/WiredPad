#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "$0")/.." && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
export PATH="$tmp:$PATH" EVDI_TEST_LOG="$tmp/modprobe.log"
cat > "$tmp/modinfo" <<'EOF'
#!/usr/bin/env bash
exit "${EVDI_TEST_MODINFO_STATUS:-0}"
EOF
cat > "$tmp/modprobe" <<'EOF'
#!/usr/bin/env bash
printf '%s\n' "$*" >> "$EVDI_TEST_LOG"
exit "${EVDI_TEST_MODPROBE_STATUS:-0}"
EOF
cat > "$tmp/sudo" <<'EOF'
#!/usr/bin/env bash
exec "$@"
EOF
chmod +x "$tmp/modinfo" "$tmp/modprobe" "$tmp/sudo"

if EVDI_TEST_MODINFO_STATUS=1 bash "$root/scripts/setup-evdi.sh" > "$tmp/output" 2>&1; then
    echo 'Missing module must fail' >&2
    exit 1
fi
[[ ! -e "$EVDI_TEST_LOG" ]]
grep -q 'matching kernel headers' "$tmp/output"

bash "$root/scripts/setup-evdi.sh" > "$tmp/output"
[[ $(cat "$EVDI_TEST_LOG") == 'evdi initial_device_count=1' ]]
grep -q 'EVDI loaded' "$tmp/output"

if EVDI_TEST_MODPROBE_STATUS=3 bash "$root/scripts/setup-evdi.sh" > "$tmp/output" 2>&1; then
    echo 'modprobe failure must propagate' >&2
    exit 1
else
    [[ $? == 3 ]]
fi
if grep -q 'EVDI loaded' "$tmp/output"; then
    echo 'Failed loading must not report success' >&2
    exit 1
fi
echo 'EVDI setup checks passed'
