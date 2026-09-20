#!/bin/bash
# Port debugowy - etap 2: podglad rejestrow/pamieci/aktualnie wykonywanej instrukcji przez
# wbudowany w QEMU GDB stub (-s = gniazdo TCP :1234), bez zadnych zmian w kernelu OxideOS.
#
# Uzycie: scripts/gdb_inspect.sh <plik_komend_gdb> [wait_s]
#
# Plik komend - zwykle polecenia GDB, jedno na linie, puste linie i linie od # ignorowane,
# np.:
#   info registers
#   bt
#   x/16i $pc
#
# Kernel.elf ma symbole (mangled C++, GDB demanguje automatycznie), wiec `bt`/`x/i $pc`
# pokazuja nazwy funkcji nawet bez pelnego DWARF (kompilujemy bez -g).
#
# CELOWO bez -enable-kvm - GDB stub pod czystym TCG jest przewidywalny (pelna emulacja
# instrukcja-po-instrukcji), pod KVM bywa ograniczony w zaleznosci od wersji QEMU/kernela.
# Bez -S: gosc bootuje normalnie, GDB dolacza sie po WAIT sekundach i przerywa wykonanie
# w tym momencie (nie od resetu) - do inspekcji stanu "w locie", np. przy buggach schedulera.
set -e
cd "$(dirname "$0")/.."

CMDS_FILE="$1"
WAIT="${2:-8}"

if [ -z "$CMDS_FILE" ] || [ ! -f "$CMDS_FILE" ]; then
    echo "Uzycie: scripts/gdb_inspect.sh <plik_komend_gdb> [wait_s]"
    exit 1
fi

./scripts/build.sh
./scripts/make_disk.sh

SERIAL_LOG=$(mktemp)

qemu-system-x86_64 -m 512M -cdrom oxideos.iso -hda disk.img -boot d -serial file:"$SERIAL_LOG" -display none -s &
QEMU_PID=$!

cleanup() {
    kill "$QEMU_PID" 2>/dev/null || true
    wait "$QEMU_PID" 2>/dev/null || true
    rm -f "$SERIAL_LOG"
}
trap cleanup EXIT

sleep "$WAIT"

GDB_ARGS=(-nx -q -batch -ex "target remote :1234")
while IFS= read -r line || [ -n "$line" ]; do
    [ -z "$line" ] && continue
    case "$line" in \#*) continue ;; esac
    GDB_ARGS+=(-ex "$line")
done < "$CMDS_FILE"
GDB_ARGS+=(-ex "detach")

gdb "${GDB_ARGS[@]}" build/kernel.elf

echo "--- SERIAL LOG ($SERIAL_LOG) ---"
cat "$SERIAL_LOG"
