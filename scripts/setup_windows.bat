@echo off
setlocal enabledelayedexpansion
rem One-time Windows setup: installs the build toolchain inside WSL2 and clones
rem OxideOS into a Windows-visible folder, so scripts\run.bat etc. work right after.
rem See README.md "Running on Windows" for what this does and why WSL2 is needed at all.

where wsl >nul 2>nul
if errorlevel 1 (
    echo ERROR: WSL not found. Install WSL2 first: https://learn.microsoft.com/windows/wsl/install
    echo Then re-run this script.
    exit /b 1
)

wsl -l -q >nul 2>nul
if errorlevel 1 (
    echo ERROR: WSL has no installed Linux distro yet.
    echo Run "wsl --install -d Ubuntu" first, finish its first-time setup ^(username/password^),
    echo then re-run this script.
    exit /b 1
)

rem Not every WSL distro is Debian/Ubuntu - if your DEFAULT distro happens to be e.g.
rem Arch, "apt" doesn't exist there and a hardcoded apt-get call just fails. Detect the
rem package manager actually available inside WSL and use the matching install command
rem instead of assuming apt. An unrecognized package manager is a warning, not a hard
rem stop - cloning below doesn't depend on it, and the packages might already be there.
echo.
echo === Installing build toolchain inside WSL ===
echo You will be asked for your WSL/Linux sudo password below - this is normal.
echo.
wsl bash -lc "if command -v apt-get >/dev/null 2>&1; then sudo apt-get update && sudo apt-get install -y git build-essential cmake xorriso qemu-system-x86 imagemagick e2fsprogs nasm mtools gdb python3; elif command -v pacman >/dev/null 2>&1; then sudo pacman -Syu --needed --noconfirm git base-devel cmake xorriso qemu-system-x86 imagemagick e2fsprogs nasm mtools gdb python; else echo 'WARNING: no supported package manager found (looked for apt-get, pacman) - skipping automatic install.' >&2; echo 'Install these manually for your distro: git, a C/C++ toolchain (gcc/make/binutils), cmake, xorriso, qemu-system-x86_64, ImageMagick, e2fsprogs (mke2fs/debugfs), nasm, mtools, gdb, python3.' >&2; fi"
if errorlevel 1 (
    echo ERROR: package install failed - see the output above ^(wrong sudo password,
    echo network issue, or a package name that doesn't match your distro's repos^).
    exit /b 1
)

set "TARGET=%~dp0..\OxideOS"
set /p "TARGET=Where should OxideOS be cloned? [%TARGET%] "
if "%TARGET%"=="" set "TARGET=%~dp0..\OxideOS"

if exist "%TARGET%\.git" (
    echo.
    echo === %TARGET% already looks like a git checkout - pulling instead of cloning ===
    for /f "delims=" %%i in ('wsl wslpath -a "%TARGET%"') do set "WSLTARGET=%%i"
    wsl bash -lc "git -C '!WSLTARGET!' pull"
    if errorlevel 1 (
        echo ERROR: git pull failed - see output above.
        exit /b 1
    )
    echo.
    echo Done - %TARGET% updated to the latest main.
    exit /b 0
)

if exist "%TARGET%" (
    echo ERROR: "%TARGET%" already exists and is not a git checkout - refusing to clone over it.
    echo Delete it or re-run and pick a different path.
    exit /b 1
)

for /f "delims=" %%i in ('wsl wslpath -a "%TARGET%"') do set "WSLTARGET=%%i"

echo.
echo === Cloning OxideOS into %TARGET% ===
wsl bash -lc "git clone https://github.com/Kocurowy96/OxideOS '!WSLTARGET!'"
if errorlevel 1 (
    echo ERROR: git clone failed - see output above.
    exit /b 1
)

echo.
echo Done. OxideOS is at %TARGET%
echo Next: open a terminal in that folder and run scripts\run.bat
