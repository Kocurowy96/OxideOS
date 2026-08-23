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
mkdir -p apps/settings
gcc -c apps/libgui/gui.c -o apps/libgui/gui.o -ffreestanding -O2 -Wall -Wextra -fno-pie -fno-stack-protector -mno-sse -mno-sse2 -mno-mmx -msoft-float
ar rcs apps/libgui/libgui.a apps/libgui/gui.o

gcc -c apps/hello/main.c -o apps/hello/main.o -ffreestanding -O2 -Wall -Wextra -fno-pie -fno-stack-protector -mno-sse -mno-sse2 -mno-mmx -msoft-float -I apps/libgui
ld -nostdlib -Ttext 0x400000 apps/hello/main.o apps/libgui/gui.o -o HELLO.ELF -no-pie

gcc -c apps/settings/main.c -o apps/settings/main.o -ffreestanding -O2 -Wall -Wextra -fno-pie -fno-stack-protector -mno-sse -mno-sse2 -mno-mmx -msoft-float -I apps/libgui
ld -nostdlib -Ttext 0x500000 apps/settings/main.o apps/libgui/gui.o -o SETTINGS.ELF -no-pie

gcc -c apps/calculator/main.c -o apps/calculator/main.o -ffreestanding -O2 -Wall -Wextra -fno-pie -fno-stack-protector -mno-sse -mno-sse2 -mno-mmx -msoft-float -I apps/libgui
ld -nostdlib -Ttext 0x600000 apps/calculator/main.o apps/libgui/gui.o -o CALC.ELF -no-pie

gcc -c apps/paint/main.c -o apps/paint/main.o -ffreestanding -O2 -Wall -Wextra -fno-pie -fno-stack-protector -mno-sse -mno-sse2 -mno-mmx -msoft-float -I apps/libgui
ld -nostdlib -Ttext 0x700000 apps/paint/main.o apps/libgui/gui.o -o PAINT.ELF -no-pie

gcc -c apps/calendar/main.c -o apps/calendar/main.o -ffreestanding -O2 -Wall -Wextra -fno-pie -fno-stack-protector -mno-sse -mno-sse2 -mno-mmx -msoft-float -I apps/libgui
ld -nostdlib -Ttext 0x800000 apps/calendar/main.o apps/libgui/gui.o -o CALENDAR.ELF -no-pie

gcc -c apps/winver/main.c -o apps/winver/main.o -ffreestanding -O2 -Wall -Wextra -fno-pie -fno-stack-protector -mno-sse -mno-sse2 -mno-mmx -msoft-float -I apps/libgui
ld -nostdlib -Ttext 0x900000 apps/winver/main.o apps/libgui/gui.o -o WINVER.ELF -no-pie

gcc -c apps/clock/main.c -o apps/clock/main.o -ffreestanding -O2 -Wall -Wextra -fno-pie -fno-stack-protector -mno-sse -mno-sse2 -mno-mmx -msoft-float -I apps/libgui
ld -nostdlib -Ttext 0xA00000 apps/clock/main.o apps/libgui/gui.o -o CLOCK.ELF -no-pie

gcc -c apps/notepad/main.c -o apps/notepad/main.o -ffreestanding -O2 -Wall -Wextra -fno-pie -fno-stack-protector -mno-sse -mno-sse2 -mno-mmx -msoft-float -I apps/libgui
ld -nostdlib -Ttext 0xB00000 apps/notepad/main.o apps/libgui/gui.o -o NOTEPAD.ELF -no-pie

gcc -c apps/taskmgr/main.c -o apps/taskmgr/main.o -ffreestanding -O2 -Wall -Wextra -fno-pie -fno-stack-protector -mno-sse -mno-sse2 -mno-mmx -msoft-float -I apps/libgui
ld -nostdlib -Ttext 0xC00000 apps/taskmgr/main.o apps/libgui/gui.o -o TASKMGR.ELF -no-pie
# Generate disk.img (FAT32)
if [ ! -f disk.img ]; then
    echo "Generating FAT32 disk image..."
    dd if=/dev/zero of=disk.img bs=1M count=64
    mformat -i disk.img -F

    # Tworzenie struktury katalogów
    mmd -i disk.img ::/DOCS
    mmd -i disk.img ::/PICS
    mmd -i disk.img ::/usr
    mmd -i disk.img ::/usr/bin
    
    mcopy -i disk.img SETTINGS.ELF ::/usr/bin/SETTINGS.ELF
    mcopy -i disk.img CALC.ELF ::/usr/bin/CALC.ELF
    mcopy -i disk.img PAINT.ELF ::/usr/bin/PAINT.ELF
    mcopy -i disk.img CALENDAR.ELF ::/usr/bin/CALENDAR.ELF
    mcopy -i disk.img WINVER.ELF ::/usr/bin/WINVER.ELF
    mcopy -i disk.img CLOCK.ELF ::/usr/bin/CLOCK.ELF
    mcopy -i disk.img NOTEPAD.ELF ::/usr/bin/NOTEPAD.ELF
    mcopy -i disk.img TASKMGR.ELF ::/usr/bin/TASKMGR.ELF
fi

mcopy -o -i disk.img HELLO.ELF ::/usr/bin/HELLO.ELF
mcopy -o -i disk.img SETTINGS.ELF ::/usr/bin/SETTINGS.ELF
mcopy -o -i disk.img CALC.ELF ::/usr/bin/CALC.ELF
mcopy -o -i disk.img PAINT.ELF ::/usr/bin/PAINT.ELF
mcopy -o -i disk.img CALENDAR.ELF ::/usr/bin/CALENDAR.ELF
mcopy -o -i disk.img WINVER.ELF ::/usr/bin/WINVER.ELF
mcopy -o -i disk.img CLOCK.ELF ::/usr/bin/CLOCK.ELF
mcopy -o -i disk.img NOTEPAD.ELF ::/usr/bin/NOTEPAD.ELF
mcopy -o -i disk.img TASKMGR.ELF ::/usr/bin/TASKMGR.ELF
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
if [ -f iso_root/wp1.bmp ]; then
    mcopy -o -i disk.img iso_root/wp1.bmp ::/wp1.bmp
fi
if [ -f iso_root/winver.bmp ]; then
    mcopy -o -i disk.img iso_root/winver.bmp ::/winver.bmp
fi
if [ -f iso_root/DOCS/CONFIG.DAT ]; then
    mcopy -o -i disk.img iso_root/DOCS/CONFIG.DAT ::/DOCS/CONFIG.DAT
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
