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
    pause
    exit /b 1
)
rem cd to the repo root first - wsl maps its working directory from the CALLER's current
rem Windows directory, not from where this .bat lives, so running it from inside scripts\
rem (very easy to do, that's where the .bat files are) would otherwise look for
rem scripts/scripts/run.sh inside WSL and fail with "No such file or directory".
pushd "%~dp0.."
wsl bash -lc "./scripts/run.sh"
set "RC=%errorlevel%"
popd
rem Pause only on failure - if this window was double-clicked rather than opened from an
rem existing cmd, it would otherwise close instantly and the error (e.g. missing KVM
rem permissions, a build failure) would never be seen.
if not "%RC%"=="0" (
    echo.
    echo Exited with an error - see the output above.
    pause
)
exit /b %RC%
