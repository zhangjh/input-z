@echo off
REM ============================================
REM SuYan Installer Build Script
REM ============================================
REM Prerequisites:
REM   - NSIS installed (auto-detected or in PATH)
REM   - Release build completed (run scripts\build-windows-all.bat first)
REM
REM Usage:
REM   build_installer.bat
REM ============================================

setlocal

REM Try to find NSIS
set MAKENSIS=
where makensis >nul 2>&1
if %ERRORLEVEL% equ 0 (
    set MAKENSIS=makensis
    goto :nsis_found
)

REM Check common installation paths
if exist "C:\Program Files (x86)\NSIS\makensis.exe" (
    set "MAKENSIS=C:\Program Files (x86)\NSIS\makensis.exe"
    goto :nsis_found
)
if exist "C:\Program Files\NSIS\makensis.exe" (
    set "MAKENSIS=C:\Program Files\NSIS\makensis.exe"
    goto :nsis_found
)

echo ERROR: NSIS not found
echo Please install NSIS from https://nsis.sourceforge.io/
echo Or add NSIS to PATH
exit /b 1

:nsis_found
echo Found NSIS: %MAKENSIS%

REM Check if 64-bit Release build exists
if not exist "..\build-win\bin\Release\SuYan.dll" (
    echo ERROR: 64-bit Release build not found
    echo Please run: scripts\build-windows-all.bat
    exit /b 1
)

REM Check if 32-bit Release build exists
if not exist "..\build-win\bin\Release\SuYan32.dll" (
    echo WARNING: 32-bit DLL not found in Release directory
    echo 32-bit applications will not be supported
    echo.
)

echo Building SuYan installer...
"%MAKENSIS%" suyan.nsi

if %ERRORLEVEL% equ 0 (
    echo.
    echo Installer built successfully!
    echo Output: SuYan-1.0.0-Setup.exe
) else (
    echo.
    echo ERROR: Installer build failed
    exit /b 1
)

endlocal
