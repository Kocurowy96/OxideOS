@echo off
setlocal
rem Windows wrapper for scripts/make_disk.sh - see README.md "Running on Windows".
rem Disk/ISO generation (mke2fs, debugfs, xorriso) is Linux-only tooling, so this
rem just runs the real script inside WSL rather than reimplementing it.
where wsl >nul 2>nul
if errorlevel 1 (
    echo ERROR: WSL not found. Install WSL2 first: https://learn.microsoft.com/windows/wsl/install
    echo Then install the build toolchain inside your WSL distro - see README.md "Running on Windows".
    exit /b 1
)
wsl bash -lc "./scripts/make_disk.sh"
exit /b %errorlevel%
