@echo off
setlocal
rem Windows wrapper for scripts/gdb_inspect.sh - see README.md "Running on Windows".
rem Usage: scripts\gdb_inspect.bat <gdb_commands_file> [wait_s]
rem Pass paths as seen from inside WSL (e.g. plain relative paths from the repo
rem root, or /mnt/c/... for a Windows path) - this wrapper forwards arguments
rem as-is to WSL, it does not translate Windows paths.
where wsl >nul 2>nul
if errorlevel 1 (
    echo ERROR: WSL not found. Install WSL2 first: https://learn.microsoft.com/windows/wsl/install
    echo Then install the build toolchain inside your WSL distro - see README.md "Running on Windows".
    exit /b 1
)
rem cd to the repo root first - wsl maps its working directory from the CALLER's current
rem Windows directory, not from where this .bat lives, so running it from inside scripts\
rem (very easy to do, that's where the .bat files are) would otherwise look for
rem scripts/scripts/gdb_inspect.sh inside WSL and fail with "No such file or directory".
pushd "%~dp0.."
wsl bash -lc "./scripts/gdb_inspect.sh %*"
set "RC=%errorlevel%"
popd
exit /b %RC%
