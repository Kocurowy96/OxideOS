@echo off
setlocal
rem Windows wrapper for scripts/build.sh - see README.md "Running on Windows".
rem OxideOS's build toolchain (freestanding ELF via raw gcc/ld, CMake) is Linux-only,
rem so this just runs the real script inside WSL rather than reimplementing it.
where wsl >nul 2>nul
if errorlevel 1 (
    echo ERROR: WSL not found. Install WSL2 first: https://learn.microsoft.com/windows/wsl/install
    echo Then install the build toolchain inside your WSL distro - see README.md "Running on Windows".
    exit /b 1
)
wsl bash -lc "./scripts/build.sh"
exit /b %errorlevel%
