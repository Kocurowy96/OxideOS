@echo off
setlocal
rem Windows wrapper for scripts/build.sh - see README.md "Running on Windows".
rem OxideOS's build toolchain (freestanding ELF via raw gcc/ld, CMake) is Linux-only,
rem so this just runs the real script inside WSL rather than reimplementing it.
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
rem scripts/scripts/build.sh inside WSL and fail with "No such file or directory".
pushd "%~dp0.."
wsl bash -lc "./scripts/build.sh"
set "RC=%errorlevel%"
popd
rem Pause only on failure - a double-clicked window would otherwise close instantly and
rem the error would never be seen.
if not "%RC%"=="0" (
    echo.
    echo Exited with an error - see the output above.
    pause
)
exit /b %RC%
