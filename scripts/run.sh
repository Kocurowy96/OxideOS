#!/bin/bash
set -e

# Change to project root
cd "$(dirname "$0")/.."

./scripts/build.sh
./scripts/make_disk.sh

# Faza 1a sieci (patrz CoworkWithClaude/PLAN_networking.md): RTL8139 + QEMU user-mode
# networking, ruch przechwycony do net_dump.pcap (czytelne przez `tcpdump -r` na hoscie).
NET_FLAGS="-netdev user,id=net0 -device rtl8139,netdev=net0 -object filter-dump,id=f1,netdev=net0,file=net_dump.pcap"

# /dev/kvm moze istniec ale byc niedostepne dla biezacego uzytkownika (np. brak w grupie
# "kvm" - zaobserwowane realnie na swiezym WSL2, gdzie urzadzenie jest ale QEMU wywala sie
# z "Could not access KVM kernel module: Permission denied") - sprawdzamy wiec faktyczna
# dostepnosc do odczytu/zapisu, nie sam fakt istnienia pliku. Bez KVM QEMU i tak dziala,
# tylko w czystej emulacji (TCG) zamiast akceleracji sprzetowej - wolniej, ale nie pada.
KVM_FLAG=""
if [ -r /dev/kvm ] && [ -w /dev/kvm ]; then
    KVM_FLAG="-enable-kvm"
else
    echo "UWAGA: /dev/kvm niedostepne (brak pliku lub brak uprawnien) - QEMU odpali sie bez akceleracji KVM (wolniej, czysta emulacja)."
fi

# Run QEMU
echo "Starting QEMU..."
qemu-system-x86_64 $KVM_FLAG -m 512M -cdrom oxideos.iso -hda disk.img -boot d -serial stdio -audiodev pa,id=snd0 -device AC97,audiodev=snd0 $NET_FLAGS
