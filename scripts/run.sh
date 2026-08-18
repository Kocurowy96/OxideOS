#!/bin/bash
set -e

# Change to project root
cd "$(dirname "$0")/.."

# Build the kernel
cmake -B build
cmake --build build

# Build limine host tool if not exists
if [ ! -f limine_dir/limine ]; then
    echo "Building Limine host tool..."
    make -C limine_dir
fi

# Prepare ISO directory
mkdir -p iso_root/boot
cp build/kernel.elf iso_root/boot/

# Compile Apps
echo "Compiling apps..."
gcc -c apps/hello/main.c -o apps/hello/main.o -ffreestanding -O2 -Wall -Wextra -fno-pie -fno-stack-protector
ld -nostdlib -Ttext 0x400000 apps/hello/main.o -o apps/hello/hello.elf -no-pie

mkdir -p apps/settings
gcc -c apps/libgui/gui.c -o apps/libgui/gui.o -ffreestanding -O2 -Wall -Wextra -fno-pie -fno-stack-protector
ar rcs apps/libgui/libgui.a apps/libgui/gui.o
gcc -c apps/settings/main.c -o apps/settings/main.o -ffreestanding -O2 -Wall -Wextra -fno-pie -fno-stack-protector -Iapps/libgui
ld -nostdlib -Ttext 0x400000 apps/settings/main.o apps/libgui/libgui.a -o apps/settings/settings.elf -no-pie

# Generate disk.img (FAT32)
if [ ! -f disk.img ]; then
    echo "Generating FAT32 disk image..."
    dd if=/dev/zero of=disk.img bs=1M count=32 status=none
    mkfs.fat -F 32 disk.img > /dev/null
    
    # Tworzenie struktury katalogów
    mmd -i disk.img ::/DOCS
    mmd -i disk.img ::/PICS
fi

mcopy -o -i disk.img apps/hello/hello.elf ::/HELLO.ELF
mcopy -o -i disk.img apps/settings/settings.elf ::/SETTINGS.ELF

# Kopiowanie dodatkowych assetów (tła, ikony, dźwięki) na dysk FAT32
if [ -d assets ]; then
    for file in assets/*; do
        if [ -f "$file" ]; then
            mcopy -o -i disk.img "$file" "::/$(basename "$file")"
        fi
    done
fi

if [ -f iso_root/bg.bmp ]; then
    mcopy -o -i disk.img iso_root/bg.bmp ::/bg.bmp
fi

# Generate 16x16 icon.bmp
python3 -c "
import struct
width, height = 16, 16
with open('icon.bmp', 'wb') as f:
    # BMP Header
    f.write(b'BM')
    f.write(struct.pack('<I', 54 + width * height * 3)) # size
    f.write(b'\x00\x00\x00\x00')
    f.write(struct.pack('<I', 54)) # offset
    # DIB Header
    f.write(struct.pack('<I', 40)) # DIB size
    f.write(struct.pack('<I', width))
    f.write(struct.pack('<I', height))
    f.write(struct.pack('<H', 1)) # planes
    f.write(struct.pack('<H', 24)) # bpp
    f.write(b'\x00' * 24)
    # Pixels (red square with a white border)
    for y in range(height):
        for x in range(width):
            if x == 0 or x == 15 or y == 0 or y == 15:
                f.write(b'\xff\xff\xff') # white
            else:
                f.write(b'\x00\x00\xff') # red (BGR)
"
mcopy -o -i disk.img icon.bmp ::/icon.bmp
mcopy -o -i disk.img iso_root/bg.bmp ::/bg1.bmp

# Generate bg2.bmp (zielone tło)
python3 -c "
with open('bg2.bmp', 'wb') as f:
    f.write(b'BM')
    f.write((1024*768*3 + 54).to_bytes(4, 'little'))
    f.write((0).to_bytes(4, 'little'))
    f.write((54).to_bytes(4, 'little'))
    f.write((40).to_bytes(4, 'little'))
    f.write((1024).to_bytes(4, 'little'))
    f.write((768).to_bytes(4, 'little'))
    f.write((1).to_bytes(2, 'little'))
    f.write((24).to_bytes(2, 'little'))
    f.write((0).to_bytes(4, 'little'))
    f.write((1024*768*3).to_bytes(4, 'little'))
    f.write((0).to_bytes(4, 'little'))
    f.write((0).to_bytes(4, 'little'))
    f.write((0).to_bytes(4, 'little'))
    f.write((0).to_bytes(4, 'little'))
    for _ in range(1024*768):
        f.write(b'\x00\x80\x00') # green (BGR)
"
mcopy -o -i disk.img bg2.bmp ::/bg2.bmp

# Generate ISO using xorriso
xorriso -as mkisofs -b boot/limine/limine-bios-cd.bin \
        -no-emul-boot -boot-load-size 4 -boot-info-table \
        --efi-boot boot/limine/limine-uefi-cd.bin \
        -efi-boot-part --efi-boot-image --protective-msdos-label \
        iso_root -o oxideos.iso > /dev/null 2>&1

# Install limine to ISO for BIOS boot
./limine_dir/limine bios-install oxideos.iso > /dev/null 2>&1

echo "OxideOS ISO generated at oxideos.iso"

# Run QEMU
echo "Starting QEMU..."
qemu-system-x86_64 -enable-kvm -m 512M -cdrom oxideos.iso -hda disk.img -boot d -serial stdio -audiodev pa,id=snd0 -device AC97,audiodev=snd0
