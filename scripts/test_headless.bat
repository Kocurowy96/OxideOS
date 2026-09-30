@echo off
setlocal
rem Windows wrapper for scripts/test_headless.sh - see README.md "Running on Windows".
rem Usage: scripts\test_headless.bat [timeout_seconds]
rem Opens nothing - boots QEMU with -display none inside WSL, prints PASS/FAIL to
rem this console, and exits with the same exit code as the underlying script.
where wsl >nul 2>nul
if errorlevel 1 (
    echo ERROR: WSL not found. Install WSL2 first: https://learn.microsoft.com/windows/wsl/install
    echo Then install the build toolchain inside your WSL distro - see README.md "Running on Windows".
    exit /b 1
)
wsl bash -lc "./scripts/test_headless.sh %*"
exit /b %errorlevel%
