#!/bin/bash
set -e

# Change to project root
cd "$(dirname "$0")/.."

# limine_dir/ jest gitignored (efemeryczny artefakt budowania, patrz .gitignore) - na
# swiezym clone repo w ogole nie istnieje. Budujemy go tutaj z commitowanego zrodla
# limine-12.5.2/ (configure jest juz wygenerowany i zacommitowany, bootstrap/autoconf
# NIE jest potrzebny). Bez tego kroku caly ten skrypt nie mial szans zadzialac na swiezym
# clone - do tej pory `limine_dir`/`iso_root/boot/limine` byly recznie odtwarzane co
# sesje (patrz historia w daily-work-report/), teraz zeskryptowane raz na dobre.
# --enable-bios* + --enable-uefi-x86-64/-cd: configure domyslnie NIE wlacza zadnego portu
# (patrz limine-12.5.2/INSTALL.md) - bez tych flag "make install" nie dalby bootloaderow.
if [ ! -f limine_dir/limine ] && [ ! -f limine_dir/bin/limine ]; then
    echo "Building Limine host tool + bootloader (bootstrap z limine-12.5.2/)..."
    # Limine'owe configure buduje bootloader (CC_FOR_TARGET) wylacznie clangiem - zwykly
    # gcc wystarcza na HOST tool, ale nie na ta czesc - bez tego sprawdzenia blad z configure
    # ("clang invalid, set CC_FOR_TARGET...") jest kompletnie nieczytelny (zaobserwowane
    # realnie na Windows/WSL Ubuntu 2026-10-02, gdzie `clang`/`lld`/`llvm` nie sa domyslnie
    # zainstalowane mimo ze byly obecne w tym srodowisku deweloperskim).
    if ! command -v clang >/dev/null 2>&1; then
        echo "ERROR: 'clang' not found - Limine's build needs the LLVM toolchain (clang," >&2
        echo "lld, llvm - not just gcc) to build its bootloader. Install it, e.g.:" >&2
        echo "  sudo apt install clang lld llvm      (Debian/Ubuntu)" >&2
        echo "  sudo pacman -S clang lld llvm        (Arch)" >&2
        echo "then re-run this script." >&2
        exit 1
    fi
    (
        cd limine-12.5.2
        ./configure --prefix="$(pwd)/../limine_dir" \
            --enable-bios --enable-bios-cd --enable-uefi-x86-64 --enable-uefi-cd
        make
        make install
    )
fi

# Sciezka do zbudowanej binarki limine rozni sie miedzy wersjami/konfiguracjami configure
# (widziane: limine_dir/limine lokalnie, limine_dir/bin/limine na swiezym configure w
# kontenerze agenta w chmurze) - wykrywamy ktora istnieje zamiast zakladac jedna z gory.
if [ -f limine_dir/bin/limine ]; then
    LIMINE_BIN=limine_dir/bin/limine
else
    LIMINE_BIN=limine_dir/limine
fi

# Prepare ISO directory
mkdir -p iso_root/boot
cp build/kernel.elf iso_root/boot/

# iso_root/boot/limine/ (bootloader binaries + config) i iso_root/EFI/BOOT/BOOTX64.EFI sa
# rowniez gitignored efemerycznymi artefaktami - jak wyzej, odtwarzane tutaj zamiast
# zakladac ze ktos je juz recznie zlozyl. "make install" z configure --prefix kladzie
# binaria bootloadera w limine_dir/share/limine/ (layout GNU), nie w limine_dir/bin/ -
# druga sciezka ponizej to fallback dla starszego/recznie zlozonego limine_dir z innym
# ukladem (widziany w tym repo wczesniej, patrz PLAN_scryptow/daily-work-report).
mkdir -p iso_root/boot/limine iso_root/EFI/BOOT
for f in limine-bios-cd.bin limine-bios.sys limine-uefi-cd.bin; do
    src="limine_dir/share/limine/$f"
    [ -f "$src" ] || src="limine_dir/bin/$f"
    [ -f "$src" ] && cp "$src" "iso_root/boot/limine/$f"
done
src="limine_dir/share/limine/BOOTX64.EFI"
[ -f "$src" ] || src="limine_dir/bin/BOOTX64.EFI"
[ -f "$src" ] && cp "$src" iso_root/EFI/BOOT/BOOTX64.EFI

cat > iso_root/boot/limine/limine.conf <<'LIMINECFG'
timeout: 0

/OxideOS
    protocol: limine
    kernel_path: boot():/boot/kernel.elf
LIMINECFG

# Generate disk.img (ext2 - patrz CoworkWithClaude/PLAN_ext2_filesystem.md Faza 3a).
# Zawsze budowany od zera (mke2fs na czystym pliku), bez osobnej sciezki
# "przyrostowej aktualizacji istniejacego obrazu" jak w starym FAT32-owym skrypcie -
# mke2fs na 64MB obrazie jest praktycznie natychmiastowy, wiec nie ma powodu
# komplikowac to obsluga nadpisywania juz-istniejacych plikow (`debugfs write` ma
# inna semantyke niz `mcopy -o` - nie nadpisuje po cichu, tylko blednie gdy sciezka
# juz istnieje), a swiezy obraz przy okazji nie zostawia smieci po kiedys-usunietych
# apkach z poprzednich sesji. -b 4096: ten sam rozmiar bloku co wszystkie obrazy
# testowe uzywane przy weryfikacji sterownika w Fazach 1-2 (utrzymuje
# block_groups_count=1 przy 64MB - zalozenie na ktorym dzis polega sterownik, patrz
# kernel/fs/ext2.cpp).
echo "Generating ext2 disk image..."
rm -f disk.img
dd if=/dev/zero of=disk.img bs=1M count=64 2>/dev/null
mke2fs -q -t ext2 -b 4096 -F disk.img

# icon.bmp nie jest juz generowany/kopiowany tutaj - od "Redesign assetow"
# (CoworkWithClaude/TASKS.md, Zrobione) generyczna ikonka okna jest wbudowana
# w binarke kernela (kernel/gui/embedded_assets.cpp), kernel przestal ja
# wczytywac z dysku w runtime (patrz Compositor::Init w kernel/gui/compositor.cpp).

# Budujemy plik komend dla `debugfs -w -f` (jeden proces zamiast osobnego wywolania
# na kazdy plik/katalog) - `write <host> <obraz>` to odpowiednik `mcopy`, `mkdir` to
# odpowiednik `mmd`. Konwersja PNG->BMP (`magick`) wykonuje sie na hoscie dokladnie
# jak w starym skrypcie - zmienia sie tylko docelowe polecenie kopiowania (write
# zamiast mcopy).
DEBUGFS_CMDS=$(mktemp)
trap 'rm -f "$DEBUGFS_CMDS"' EXIT

echo -e "mkdir /DOCS\nmkdir /PICS\nmkdir /usr\nmkdir /usr/bin" > "$DEBUGFS_CMDS"

# `debugfs write` (w przeciwienstwie do `mcopy -o` w starym skrypcie) NIE nadpisuje
# po cichu - blednie gdy cel juz istnieje w obrazie. Ten skrypt z natury potrafi
# trafic w ten sam cel dwa razy: assets/wp1.bmp i iso_root/wp1.bmp to ten sam plik
# pod dwiema sciezkami zrodlowymi (podobnie winver.bmp); a pliki .bmp wygenerowane
# w POPRZEDNIM uruchomieniu (zapisywane wprost do assets/, patrz petla konwersji
# PNG->BMP nizej) pasuja przy KOLEJNYM uruchomieniu zarowno do wzorca *.png (przez
# swoj zrodlowy plik) jak i do wzorca *.bmp (przez samego siebie) - bez tego dopisek
# kazdy z tych przypadkow skonczylby sie "Ext2 file already exists" z debugfs.
# Zamiast wyliczac z gory wszystkie takie przypadki, pilnujemy tego ogolnie: kazda
# sciezka docelowa jest zapisywana najwyzej raz w danym przebiegu.
declare -A written_dest
add_write() {
    local src="$1" dest="$2"
    if [ -n "${written_dest[$dest]:-}" ]; then
        return
    fi
    written_dest[$dest]=1
    echo "write $src $dest" >> "$DEBUGFS_CMDS"
}

for elf in HELLO SETTINGS CALC PAINT CALENDAR WINVER CLOCK NOTEPAD TASKMGR; do
    add_write "${elf}.ELF" "/usr/bin/${elf}.ELF"
done

# Kopiowanie dodatkowych assetow (tla, ikony, dzwieki) na dysk
if [ -d assets ]; then
    for file in assets/*; do
        if [ -f "$file" ]; then
            filename=$(basename "$file")
            # Konwersja PNG do 32-bit BMP (ARGB) dla przezroczystosci
            if [[ "$filename" == *.png ]]; then
                bmp_file="assets/${filename%.png}.bmp"
                echo "Konwertowanie $filename do 32-bit BMP..."
                # Niektore srodowiska (widziane w kontenerze agenta w chmurze) maja tylko IM6
                # (`convert`), nie `magick` (IM7) - ten sam fallback co w headless_interact.sh.
                if command -v magick >/dev/null 2>&1; then
                    magick "$file" -define bmp:format=bmp3 -define bmp3:alpha=true "BMP3:$bmp_file"
                else
                    convert "$file" -define bmp:format=bmp3 -define bmp3:alpha=true "BMP3:$bmp_file"
                fi
                add_write "$bmp_file" "/$(basename "$bmp_file")"
            elif [[ "$filename" == *.bmp ]] || [[ "$filename" == *.wav ]]; then
                add_write "$file" "/$filename"
            fi
        fi
    done
fi

if [ -f iso_root/bg.bmp ]; then
    add_write "iso_root/bg.bmp" "/bg.bmp"
fi
if [ -f iso_root/wp1.bmp ]; then
    add_write "iso_root/wp1.bmp" "/wp1.bmp"
fi
if [ -f iso_root/winver.bmp ]; then
    add_write "iso_root/winver.bmp" "/winver.bmp"
fi
if [ -f iso_root/DOCS/CONFIG.DAT ]; then
    add_write "iso_root/DOCS/CONFIG.DAT" "/DOCS/CONFIG.DAT"
fi

# Miniatura tapety #1 do Panelu Sterowania - assets/wp1_thumb.bmp powstaje jako
# efekt uboczny petli konwersji PNG->BMP wyzej (wp1_thumb.png), wiec o tym momencie
# juz istnieje.
if [ -f assets/wp1_thumb.bmp ]; then
    add_write "assets/wp1_thumb.bmp" "/PICS/wp1_thumb.bmp"
fi
if [ -f iso_root/bg.bmp ]; then
    add_write "iso_root/bg.bmp" "/bg1.bmp"
fi

debugfs -w -f "$DEBUGFS_CMDS" disk.img > /dev/null

# Generate ISO using xorriso
xorriso -as mkisofs -b boot/limine/limine-bios-cd.bin \
        -no-emul-boot -boot-load-size 4 -boot-info-table \
        --efi-boot boot/limine/limine-uefi-cd.bin \
        -efi-boot-part --efi-boot-image --protective-msdos-label \
        iso_root -o oxideos.iso > /dev/null 2>&1

# Install limine to ISO for BIOS boot
"./$LIMINE_BIN" bios-install oxideos.iso > /dev/null 2>&1

echo "OxideOS ISO generated at oxideos.iso"
