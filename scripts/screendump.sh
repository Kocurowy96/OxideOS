#!/bin/bash
# Port debugowy - etap 1: zrzut ekranu emulatora bez GUI, przez QMP `screendump`.
# Boot headless, czekaj WAIT sekund na stan ktory ma nas interesowac, zrzuc ekran, zabij QEMU.
set -e
cd "$(dirname "$0")/.."

OUT="${1:-screenshot.png}"
WAIT="${2:-8}"

./scripts/build.sh
./scripts/make_disk.sh

QMP_SOCK=$(mktemp -u /tmp/oxideos-qmp-XXXXXX.sock)
SERIAL_LOG=$(mktemp)
PPM=$(mktemp /tmp/oxideos-shot-XXXXXX.ppm)

QEMU_FLAGS="-m 512M -cdrom oxideos.iso -hda disk.img -boot d -serial file:$SERIAL_LOG -display none -qmp unix:$QMP_SOCK,server,nowait"
if [ -e /dev/kvm ]; then
    QEMU_FLAGS="-enable-kvm $QEMU_FLAGS"
fi

qemu-system-x86_64 $QEMU_FLAGS &
QEMU_PID=$!

cleanup() {
    kill "$QEMU_PID" 2>/dev/null || true
    wait "$QEMU_PID" 2>/dev/null || true
    rm -f "$QMP_SOCK" "$SERIAL_LOG" "$PPM"
}
trap cleanup EXIT

for _ in $(seq 1 50); do
    [ -S "$QMP_SOCK" ] && break
    sleep 0.1
done
if [ ! -S "$QMP_SOCK" ]; then
    echo "FAIL: gniazdo QMP nie powstalo w 5s"
    exit 1
fi

sleep "$WAIT"

python3 "$(dirname "$0")/qmp_screendump.py" "$QMP_SOCK" "$PPM"

if command -v magick >/dev/null 2>&1; then
    magick "$PPM" "$OUT"
else
    convert "$PPM" "$OUT"
fi

echo "OK: $OUT"
echo "--- SERIAL LOG ($SERIAL_LOG) ---"
cat "$SERIAL_LOG"
