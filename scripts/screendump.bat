@echo off
setlocal
rem Windows wrapper for scripts/screendump.sh - see README.md "Running on Windows".
rem Usage: scripts\screendump.bat [out.png] [wait_seconds]
rem Opens nothing on screen - boots QEMU headless inside WSL, grabs a screenshot via
rem QMP, and writes it under WSL's filesystem (see the printed path; from Windows
rem that's \\wsl$\<distro>\home\<user>\OxideOS\<out.png> unless out.png is an
rem absolute /mnt/c/... path).
where wsl >nul 2>nul
if errorlevel 1 (
    echo ERROR: WSL not found. Install WSL2 first: https://learn.microsoft.com/windows/wsl/install
    echo Then install the build toolchain inside your WSL distro - see README.md "Running on Windows".
    exit /b 1
)
wsl bash -lc "./scripts/screendump.sh %*"
exit /b %errorlevel%
