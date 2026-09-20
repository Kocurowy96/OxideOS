#!/bin/bash
# Port debugowy - etap 3: automatyczna interakcja z headless QEMU (mysz/klawiatura/zrzuty
# ekranu) bez czlowieka przy ekranie. Czyta sekwencje akcji z pliku scenariusza.
#
# Uzycie: scripts/headless_interact.sh <plik_scenariusza> [katalog_wyjsciowy] [boot_wait_s]
#
# Format pliku scenariusza (jedna akcja na linie, puste linie i linie zaczynajace sie od #
# sa ignorowane):
#   move X Y
#   click BUTTON [X Y]     BUTTON = left|right|middle, X/Y opcjonalne (przesuwa przed klikiem)
#   scroll up|down [ILOSC]
#   key QCODE               np. ret, esc, tab, a - patrz QEMU QKeyCode
#   type tekst do konca linii
#   wait SEKUNDY
#   shot nazwa.png           zrzut biezacego ekranu do katalog_wyjsciowy/nazwa.png
set -e
cd "$(dirname "$0")/.."

SCENARIO="$1"
OUT_DIR="${2:-.}"
BOOT_WAIT="${3:-8}"

if [ -z "$SCENARIO" ] || [ ! -f "$SCENARIO" ]; then
    echo "Uzycie: scripts/headless_interact.sh <plik_scenariusza> [katalog_wyjsciowy] [boot_wait_s]"
    exit 1
fi

mkdir -p "$OUT_DIR"

./scripts/build.sh
./scripts/make_disk.sh

QMP_SOCK=$(mktemp -u /tmp/oxideos-qmp-XXXXXX.sock)
SERIAL_LOG=$(mktemp)

QEMU_FLAGS="-m 512M -cdrom oxideos.iso -hda disk.img -boot d -serial file:$SERIAL_LOG -display none -qmp unix:$QMP_SOCK,server,nowait"
if [ -e /dev/kvm ]; then
    QEMU_FLAGS="-enable-kvm $QEMU_FLAGS"
fi

qemu-system-x86_64 $QEMU_FLAGS &
QEMU_PID=$!

cleanup() {
    kill "$QEMU_PID" 2>/dev/null || true
    wait "$QEMU_PID" 2>/dev/null || true
    rm -f "$QMP_SOCK" "$SERIAL_LOG"
}
trap cleanup EXIT

for _ in $(seq 1 50); do
    [ -S "$QMP_SOCK" ] && break
    sleep 0.1
done
if [ ! -S "$QMP_SOCK" ]; then
    echo "FAIL: gniazdo QMP nie powstalo w 5s"
    exit 1
fi

sleep "$BOOT_WAIT"

SCRIPT_DIR="$(dirname "$0")"

while IFS= read -r line || [ -n "$line" ]; do
    [ -z "$line" ] && continue
    case "$line" in \#*) continue ;; esac

    set -f
    set -- $line
    set +f
    action="$1"
    shift || true

    case "$action" in
        move)
            python3 "$SCRIPT_DIR/qmp_input.py" "$QMP_SOCK" move "$1" "$2"
            ;;
        click)
            button="$1"
            if [ -n "$2" ]; then
                python3 "$SCRIPT_DIR/qmp_input.py" "$QMP_SOCK" click --button "$button" --at "$2" "$3"
            else
                python3 "$SCRIPT_DIR/qmp_input.py" "$QMP_SOCK" click --button "$button"
            fi
            ;;
        scroll)
            python3 "$SCRIPT_DIR/qmp_input.py" "$QMP_SOCK" scroll "$1" --amount "${2:-1}"
            ;;
        key)
            python3 "$SCRIPT_DIR/qmp_input.py" "$QMP_SOCK" key "$1"
            ;;
        type)
            text="${line#type }"
            python3 "$SCRIPT_DIR/qmp_input.py" "$QMP_SOCK" type "$text"
            ;;
        wait)
            sleep "$1"
            ;;
        shot)
            PPM=$(mktemp /tmp/oxideos-shot-XXXXXX.ppm)
            python3 "$SCRIPT_DIR/qmp_screendump.py" "$QMP_SOCK" "$PPM"
            if command -v magick >/dev/null 2>&1; then
                magick "$PPM" "$OUT_DIR/$1"
            else
                convert "$PPM" "$OUT_DIR/$1"
            fi
            rm -f "$PPM"
            echo "shot: $OUT_DIR/$1"
            ;;
        *)
            echo "Nieznana akcja w scenariuszu: $action" >&2
            exit 1
            ;;
    esac
done < "$SCENARIO"

echo "--- SERIAL LOG ($SERIAL_LOG) ---"
cat "$SERIAL_LOG"
