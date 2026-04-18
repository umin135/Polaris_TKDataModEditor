@echo off
cd /d "%~dp0"
echo === FbsdataExtract ===
python extract_bins.py %*
if errorlevel 1 (
    echo.
    echo [FAILED] Python returned an error.
    pause
) else (
    echo.
    pause
)
