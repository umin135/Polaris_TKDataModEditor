@echo off
setlocal EnableExtensions

rem Always run from the folder that contains this script (project root).
cd /d "%~dp0"

set "CONFIG=Debug"
set "PLATFORM=x64"
set "SLN=%~dp0PolarisTKDataEditor.sln"

if not exist "%SLN%" (
    echo ERROR: Solution not found:
    echo   %SLN%
    exit /b 1
)

rem Locate MSBuild via vswhere (VS 2022+), then fall back to a common path.
set "MSBUILD="
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    for /f "usebackq delims=" %%I in (`
        "%VSWHERE%" -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe
    `) do (
        set "MSBUILD=%%I"
        goto :have_msbuild
    )
)

:have_msbuild
if not defined MSBUILD (
    if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" (
        set "MSBUILD=%ProgramFiles%\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
    ) else if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe" (
        set "MSBUILD=%ProgramFiles%\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe"
    ) else if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe" (
        set "MSBUILD=%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
    )
)

if not defined MSBUILD (
    echo ERROR: MSBuild not found. Install Visual Studio 2022 with C++ desktop workload.
    exit /b 1
)

echo Building %CONFIG%^|%PLATFORM% ...
echo MSBuild: %MSBUILD%
echo.

"%MSBUILD%" "%SLN%" /m /nologo /v:minimal /p:Configuration=%CONFIG% /p:Platform=%PLATFORM%
set "ERR=%ERRORLEVEL%"

echo.
if not "%ERR%"=="0" (
    echo BUILD FAILED ^(exit %ERR%^).
    exit /b %ERR%
)

echo BUILD SUCCEEDED.
echo Output: %~dp0x64\%CONFIG%\PolarisTKDataEditor.exe
exit /b 0
