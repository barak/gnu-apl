@echo off
rem Installs the GNU APL TrueType font for the current Windows user.
rem No administrator privileges required.

setlocal

set "SCRIPT_DIR=%~dp0"
set "FONT_FILE=%SCRIPT_DIR%GNU_APL.ttf"
set "DEST_DIR=%LOCALAPPDATA%\Microsoft\Windows\Fonts"
set "DEST_FILE=%DEST_DIR%\GNU_APL.ttf"
set "REG_KEY=HKCU\Software\Microsoft\Windows NT\CurrentVersion\Fonts"
set "REG_VALUE=GNU_APL (TrueType)"

if not exist "%FONT_FILE%" (
    echo Font file not found: %FONT_FILE%
    exit /b 1
)

if not exist "%DEST_DIR%" mkdir "%DEST_DIR%"

copy /y "%FONT_FILE%" "%DEST_FILE%" >nul
if errorlevel 1 (
    echo Failed to copy font file to %DEST_DIR%
    exit /b 1
)

reg add "%REG_KEY%" /v "%REG_VALUE%" /t REG_SZ /d "GNU_APL.ttf" /f >nul
if errorlevel 1 (
    echo Failed to register font in %REG_KEY%
    exit /b 1
)

echo.
echo GNU APL font installed for the current user.
echo Close and reopen any Command Prompt windows to see "GNU_APL" in the Font tab.
echo.
pause

endlocal
