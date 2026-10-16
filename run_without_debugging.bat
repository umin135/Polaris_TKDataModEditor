@echo off
setlocal EnableExtensions

rem Always run from the folder that contains this script (project root).
cd /d "%~dp0"

set "EXE=%~dp0x64\Debug\PolarisTKDataEditor.exe"

rem Incremental build first (same as VS Ctrl+F5), then launch.
call "%~dp0debug_build.bat"
if errorlevel 1 (
    echo.
    echo Not starting the app because the build failed.
    exit /b 1
)

if not exist "%EXE%" (
    echo ERROR: Executable not found after build:
    echo   %EXE%
    exit /b 1
)

echo.
echo Starting ^(no debugger^): %EXE%
start "" "%EXE%"
exit /b 0
