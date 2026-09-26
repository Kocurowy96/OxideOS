#!/bin/bash
set -e

# Change to project root
cd "$(dirname "$0")/.."

./scripts/build.sh
./scripts/make_disk.sh

# Faza 1a sieci (patrz CoworkWithClaude/PLAN_networking.md): RTL8139 + QEMU user-mode
# networking, ruch przechwycony do net_dump.pcap (czytelne przez `tcpdump -r` na hoscie).
NET_FLAGS="-netdev user,id=net0 -device rtl8139,netdev=net0 -object filter-dump,id=f1,netdev=net0,file=net_dump.pcap"

# Run QEMU
echo "Starting QEMU..."
qemu-system-x86_64 -enable-kvm -m 512M -cdrom oxideos.iso -hda disk.img -boot d -serial stdio -audiodev pa,id=snd0 -device AC97,audiodev=snd0 $NET_FLAGS
