cmake_minimum_required(VERSION 3.10)
project(OxideOS LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Limine and bare metal flags
set(KERNEL_CXX_FLAGS "-O2 -ffreestanding -fno-stack-protector -fno-stack-check -fno-lto -fPIE -m64 -march=x86-64 -mno-80387 -mno-mmx -mno-sse -mno-sse2 -mno-red-zone")
# Disable exceptions and rtti for kernel
set(KERNEL_CXX_FLAGS "${KERNEL_CXX_FLAGS} -fno-exceptions -fno-rtti -fno-threadsafe-statics")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${KERNEL_CXX_FLAGS}")

# Linker flags (custom linker script, no stdlib)
set(CMAKE_EXE_LINKER_FLAGS "-nostdlib -static -pie -Wl,--no-dynamic-linker -Wl,-z,text -Wl,-z,max-page-size=0x1000 -T ${CMAKE_SOURCE_DIR}/linker.ld")

enable_language(ASM)
include_directories(kernel)

set(KERNEL_SOURCES
    kernel/main.cpp
    kernel/serial.cpp
    kernel/cpu/gdt.cpp
    kernel/cpu/gdt_asm.S
    kernel/cpu/idt.cpp
    kernel/cpu/interrupts.S
    kernel/cpu/isr.cpp
    kernel/cpu/syscall.cpp
    kernel/cpu/critical.cpp
    kernel/drivers/pic.cpp
    kernel/drivers/rtc.cpp
    kernel/drivers/pit.cpp
    kernel/drivers/ps2_kbd.cpp
    kernel/drivers/ps2_mouse.cpp
    kernel/drivers/ata.cpp
    kernel/drivers/pci.cpp
    kernel/drivers/ac97.cpp
    kernel/fs/vfs.cpp
    kernel/fs/fat32.cpp
    kernel/fs/elf.cpp
    kernel/mem/pmm.cpp
    kernel/mem/vmm.cpp
    kernel/gui/fb.cpp
    kernel/gui/bmp.cpp
    kernel/gui/window.cpp
    kernel/gui/apps.cpp
    kernel/gui/compositor.cpp
    kernel/gui/osod.cpp
    kernel/fonts/font8x8.cpp
    kernel/proc/sched.cpp
    kernel/media/wav.cpp
)

add_executable(kernel.elf ${KERNEL_SOURCES})
