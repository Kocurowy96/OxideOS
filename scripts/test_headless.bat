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
rem cd to the repo root first - wsl maps its working directory from the CALLER's current
rem Windows directory, not from where this .bat lives, so running it from inside scripts\
rem (very easy to do, that's where the .bat files are) would otherwise look for
rem scripts/scripts/test_headless.sh inside WSL and fail with "No such file or directory".
pushd "%~dp0.."
wsl bash -lc "./scripts/test_headless.sh %*"
set "RC=%errorlevel%"
popd
exit /b %RC%
