#!/bin/bash
set -e

# Change to project root
cd "$(dirname "$0")/.."

TIMEOUT="${1:-20}"

./scripts/build.sh
./scripts/make_disk.sh

QEMU_FLAGS="-m 512M -cdrom oxideos.iso -hda disk.img -boot d -serial stdio -display none"
if [ -e /dev/kvm ]; then
    QEMU_FLAGS="-enable-kvm $QEMU_FLAGS"
fi

LOG=$(mktemp)
timeout "$TIMEOUT" qemu-system-x86_64 $QEMU_FLAGS > "$LOG" 2>&1 || true

echo "--- LOG ($LOG) ---"
cat "$LOG"

if grep -q "FAT32: Initialized successfully" "$LOG" && ! grep -qi "kernel panic" "$LOG"; then
    echo "PASS: boot dotarl do FAT32 init, brak panic"
    exit 0
else
    echo "FAIL: albo brak FAT32 init, albo wykryto panic"
    exit 1
fi
