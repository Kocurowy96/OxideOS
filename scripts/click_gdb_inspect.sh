#!/bin/bash
# Port debugowy - rozszerzenie etapu 2/3: laczy klikanie przez QMP (jak
# headless_interact.sh) z podgladem GDB (jak gdb_inspect.sh) w JEDNYM uruchomieniu QEMU.
# Potrzebne tam, gdzie trzeba odtworzyc konkretny scenariusz klikania RECZNIE (np. ciag
# otwiera apek z Menu Start prowadzacy do crasha) i zlapac stan "na gorąco" (rejestry/
# pamiec/bt) w momencie gdy jadro sie zatrzymuje (np. po "cli; hlt" w panic handlerze),
# zamiast czekac stala liczbe sekund jak w gdb_inspect.sh (ktory nie steruje mysza).
#
# Uzycie: scripts/click_gdb_inspect.sh <scenariusz_klikania.txt> <komendy_gdb.txt>
#                                       [boot_wait_s] [halt_timeout_s] [live_break_symbol]
#                                       [live_timeout_s]
#
# Scenariusz klikania - format identyczny jak headless_interact.sh (move/click/scroll/
# key/type/wait, bez "shot" - tu nie robimy zrzutow ekranu).
# Komendy GDB - format identyczny jak gdb_inspect.sh (jedna komenda na linie).
#
# Domyslnie (live_break_symbol nie podany): skrypt czeka az scenariusz sie wykona, POTEM
# obserwuje log serialowy w poszukiwaniu "Halting." (panic handler jadra zawsze to loguje
# tuz przed "cli; hlt") do halt_timeout_s sekund - dopiero wtedy dolacza GDB, zeby zlapac
# faktycznie zatrzymany, a nie wciaz dzialajacy, procesor. Jesli "Halting." nie pojawi sie
# (bug sie nie odtworzyl), GDB i tak sie dolacza po uplynieciu limitu, zeby zobaczyc co
# sie dzieje. To zawsze obserwuje stan PO fakcie (CPU juz zahaltowany).
#
# live_break_symbol (opcjonalnie): wlacza tryb "live" - zamiast dolaczac GDB po
# "Halting.", QEMU startuje z "-S" (zatrzymany od resetu), GDB dolacza sie NATYCHMIAST,
# stawia hardware breakpoint na podanym symbolu (np. "isr_stub_14" - wejscie do
# konkretnego wektora ISR, PRZED jakimkolwiek kodem C++/push rejestrow, patrz
# kernel/cpu/interrupts.S) i robi "continue" - gosc bootuje normalnie od tego momentu.
# Scenariusz klikania wykonuje sie jak zwykle (po BOOT_WAIT), a GDB w tle czeka na
# wlasnie ten breakpoint (nie na "Halting." - zlapie SAM MOMENT faultu, zanim
# jakikolwiek kod go obsluzy, przydatne do zbadania czy CR2 jest juz "zle" w chwili
# sprzetowego dostarczenia wyjatku czy psuje sie pozniej). Po trafieniu breakpointu GDB
# wykonuje komendy z GDB_CMDS (np. "p/x $cr2", "stepi" x N) tak jak w trybie domyslnym.
# live_timeout_s (domyslnie 30) ogranicza jak dlugo caly skrypt czeka na GDB w tym trybie
# (osobno od BOOT_WAIT/HALT_TIMEOUT, ktore w tym trybie sa nieuzywane do synchronizacji
# z GDB - HALT_TIMEOUT wciaz obowiazuje jako normalny czas na wykonanie klikania).
set -e
cd "$(dirname "$0")/.."

CLICK_SCENARIO="$1"
GDB_CMDS="$2"
BOOT_WAIT="${3:-8}"
HALT_TIMEOUT="${4:-10}"
LIVE_BREAK_SYMBOL="$5"
LIVE_TIMEOUT="${6:-30}"

if [ -z "$CLICK_SCENARIO" ] || [ ! -f "$CLICK_SCENARIO" ] || [ -z "$GDB_CMDS" ] || [ ! -f "$GDB_CMDS" ]; then
    echo "Uzycie: scripts/click_gdb_inspect.sh <scenariusz_klikania.txt> <komendy_gdb.txt> [boot_wait_s] [halt_timeout_s] [live_break_symbol] [live_timeout_s]"
    exit 1
fi

./scripts/build.sh
./scripts/make_disk.sh

QMP_SOCK=$(mktemp -u /tmp/oxideos-qmp-XXXXXX.sock)
SERIAL_LOG=$(mktemp)
GDB_LIVE_LOG=$(mktemp)

# -s = GDB stub na :1234. Bez trybu live: bez -S (gosc bootuje normalnie, nie od
# resetu) - tak samo jak gdb_inspect.sh. W trybie live: z -S (zatrzymany od resetu, GDB
# musi sam go wystartowac przez "continue" po postawieniu breakpointu). Dodatkowo -qmp
# (jak headless_interact.sh) do sterowania mysza/klawiatura.
QEMU_FLAGS="-m 512M -cdrom oxideos.iso -hda disk.img -boot d -serial file:$SERIAL_LOG -display none -qmp unix:$QMP_SOCK,server,nowait -s"
if [ -n "$LIVE_BREAK_SYMBOL" ]; then
    QEMU_FLAGS="$QEMU_FLAGS -S"
fi
if [ -r /dev/kvm ] && [ -w /dev/kvm ]; then
    QEMU_FLAGS="-enable-kvm $QEMU_FLAGS"
fi

qemu-system-x86_64 $QEMU_FLAGS &
QEMU_PID=$!
GDB_LIVE_PID=""

cleanup() {
    kill "$GDB_LIVE_PID" 2>/dev/null || true
    kill "$QEMU_PID" 2>/dev/null || true
    wait "$QEMU_PID" 2>/dev/null || true
    rm -f "$QMP_SOCK" "$SERIAL_LOG" "$GDB_LIVE_LOG"
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

if [ -n "$LIVE_BREAK_SYMBOL" ]; then
    # Tryb live: GDB musi dolaczyc sie i wystartowac goscia (-S) ZANIM zaczniemy
    # klikac, inaczej zdarzenia QMP wyslane do wciaz-zatrzymanego CPU ulegna
    # wszystkie na raz w momencie "continue" (zepsuty timing scenariusza).
    GDB_LIVE_ARGS=(-nx -q -batch -ex "target remote :1234" -ex "hbreak $LIVE_BREAK_SYMBOL" -ex "continue")
    while IFS= read -r line || [ -n "$line" ]; do
        [ -z "$line" ] && continue
        case "$line" in \#*) continue ;; esac
        GDB_LIVE_ARGS+=(-ex "$line")
    done < "$GDB_CMDS"
    GDB_LIVE_ARGS+=(-ex "detach")

    timeout "$LIVE_TIMEOUT" gdb "${GDB_LIVE_ARGS[@]}" build/kernel.elf > "$GDB_LIVE_LOG" 2>&1 &
    GDB_LIVE_PID=$!

    # Krotkie, stale oczekiwanie - "target remote"+"hbreak"+"continue" w GDB batch
    # wykonuja sie praktycznie natychmiast (zadne z nich nie blokuje), wiec to
    # wystarcza by miec pewnosc ze goscia faktycznie juz dziala, zanim zaczniemy
    # odmierzac BOOT_WAIT/wysylac klikniecia.
    sleep 2
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
        *)
            echo "Nieznana akcja w scenariuszu: $action" >&2
            exit 1
            ;;
    esac
done < "$CLICK_SCENARIO"

if [ -n "$LIVE_BREAK_SYMBOL" ]; then
    # Tryb live: GDB juz dziala w tle (postawiony breakpoint + "continue" przed
    # klikaniem) i czeka na $LIVE_BREAK_SYMBOL. Scenariusz klikania sie wykonal -
    # czekamy az GDB w tle faktycznie zlapie breakpoint, wykona GDB_CMDS i sam
    # sie zakonczy (do LIVE_TIMEOUT sekund, liczonych od startu GDB, nie od teraz).
    wait "$GDB_LIVE_PID" 2>/dev/null || true
    GDB_LIVE_PID=""

    echo "--- GDB LIVE LOG (breakpoint: $LIVE_BREAK_SYMBOL) ---"
    cat "$GDB_LIVE_LOG"
else
    # Scenariusz klikania sie wykonal - teraz czekamy az jadro faktycznie sie zatrzyma
    # ("Halting." w logu serialowym = panic handler tuz przed "cli; hlt"), zamiast lapac
    # GDB-m proces ktory jeszcze dziala (niedeterministyczny stan).
    for _ in $(seq 1 "$HALT_TIMEOUT"); do
        grep -q "Halting\." "$SERIAL_LOG" 2>/dev/null && break
        sleep 1
    done

    GDB_ARGS=(-nx -q -batch -ex "target remote :1234")
    while IFS= read -r line || [ -n "$line" ]; do
        [ -z "$line" ] && continue
        case "$line" in \#*) continue ;; esac
        GDB_ARGS+=(-ex "$line")
    done < "$GDB_CMDS"
    GDB_ARGS+=(-ex "detach")

    gdb "${GDB_ARGS[@]}" build/kernel.elf
fi

echo "--- SERIAL LOG ($SERIAL_LOG) ---"
cat "$SERIAL_LOG"
