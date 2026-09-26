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

# Faza 3b (patrz CoworkWithClaude/PLAN_ext2_filesystem.md): VFS:: wola teraz Ext2::,
# nie FAT32:: - kryterium PASS zaktualizowane zeby sprawdzac wlasciwy log (FAT32::Init()
# juz nigdy sie nie wywoluje, wiec stare kryterium nigdy by tu nie trafilo).
if grep -q "Ext2: Initialized successfully" "$LOG" && ! grep -qi "kernel panic" "$LOG"; then
    echo "PASS: boot dotarl do Ext2 init, brak panic"
    exit 0
else
    echo "FAIL: albo brak Ext2 init, albo wykryto panic"
    exit 1
fi
