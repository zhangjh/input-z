@echo off
REM ============================================
REM SuYan Installer Build Script
REM ============================================
REM Prerequisites:
REM   - NSIS installed and in PATH
REM   - Release build completed (run scripts\build-windows-all.bat first)
REM
REM Usage:
REM   build_installer.bat
REM ============================================

setlocal

REM Check if NSIS is available
where makensis >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo ERROR: NSIS not found in PATH
    echo Please install NSIS from https://nsis.sourceforge.io/
    exit /b 1
)

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
makensis suyan.nsi

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
