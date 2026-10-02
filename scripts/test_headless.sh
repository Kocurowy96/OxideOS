#!/bin/bash
set -e

# Change to project root
cd "$(dirname "$0")/.."

TIMEOUT="${1:-20}"

./scripts/build.sh
./scripts/make_disk.sh

# Faza 1a sieci (patrz CoworkWithClaude/PLAN_networking.md): RTL8139 + QEMU user-mode
# networking (SLIRP, brak roota) - gosc dostaje 10.0.2.15, brama 10.0.2.2. filter-dump
# przechwytuje caly ruch do net_dump.pcap, czytelne potem przez `tcpdump -r` na hoscie.
NET_FLAGS="-netdev user,id=net0 -device rtl8139,netdev=net0 -object filter-dump,id=f1,netdev=net0,file=net_dump.pcap"

QEMU_FLAGS="-m 512M -cdrom oxideos.iso -hda disk.img -boot d -serial stdio -display none $NET_FLAGS"
# /dev/kvm moze istniec ale byc niedostepne dla biezacego uzytkownika (np. brak w grupie
# "kvm" - zaobserwowane realnie na WSL2, "-e" samo w sobie nie wystarczylo i QEMU padal z
# "Permission denied") - sprawdzamy faktyczna dostepnosc do odczytu/zapisu, nie sam fakt
# istnienia pliku.
if [ -r /dev/kvm ] && [ -w /dev/kvm ]; then
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
