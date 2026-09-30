@echo off
setlocal
rem Windows wrapper for scripts/run.sh - see README.md "Running on Windows".
rem Builds OxideOS inside WSL, then boots it in QEMU. QEMU's own window opens
rem normally on the Windows desktop (WSLg/WSL2 forwards the GUI) - nothing extra
rem to configure beyond having qemu-system-x86_64 installed inside WSL.
where wsl >nul 2>nul
if errorlevel 1 (
    echo ERROR: WSL not found. Install WSL2 first: https://learn.microsoft.com/windows/wsl/install
    echo Then install the build toolchain inside your WSL distro - see README.md "Running on Windows".
    exit /b 1
)
wsl bash -lc "./scripts/run.sh"
exit /b %errorlevel%
