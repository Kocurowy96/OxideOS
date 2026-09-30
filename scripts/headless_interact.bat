@echo off
setlocal
rem Windows wrapper for scripts/headless_interact.sh - see README.md "Running on Windows".
rem Usage: scripts\headless_interact.bat <scenario_file> [out_dir] [boot_wait_s]
rem Pass paths as seen from inside WSL (e.g. plain relative paths from the repo
rem root, or /mnt/c/... for a Windows path) - this wrapper forwards arguments
rem as-is to WSL, it does not translate Windows paths.
where wsl >nul 2>nul
if errorlevel 1 (
    echo ERROR: WSL not found. Install WSL2 first: https://learn.microsoft.com/windows/wsl/install
    echo Then install the build toolchain inside your WSL distro - see README.md "Running on Windows".
    exit /b 1
)
wsl bash -lc "./scripts/headless_interact.sh %*"
exit /b %errorlevel%
