#!/bin/bash
set -e

# Change to project root
cd "$(dirname "$0")/.."

# Build the kernel
cmake -B build
cmake --build build

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
