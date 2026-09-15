#!/bin/bash
set -e

# Change to project root
cd "$(dirname "$0")/.."

./scripts/build.sh
./scripts/make_disk.sh

# Run QEMU
echo "Starting QEMU..."
qemu-system-x86_64 -enable-kvm -m 512M -cdrom oxideos.iso -hda disk.img -boot d -serial stdio -audiodev pa,id=snd0 -device AC97,audiodev=snd0
