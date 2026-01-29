@echo off
REM ============================================
REM SuYan 输入法 Windows 构建脚本
REM 构建64位完整版本和32位轻量DLL
REM ============================================

setlocal

set ROOT_DIR=%~dp0..
set BUILD_X64=%ROOT_DIR%\build-win
set BUILD_X86=%ROOT_DIR%\build-win-x86

REM 检查 Qt 路径
if "%QT_PREFIX%"=="" set QT_PREFIX=C:\Qt\6.7.2\msvc2019_64
if "%VCPKG_PREFIX%"=="" set VCPKG_PREFIX=C:\vcpkg\installed\x64-windows

if not exist "%QT_PREFIX%\bin\Qt6Core.dll" (
    echo Error: Qt6 not found at %QT_PREFIX%
    echo Set QT_PREFIX to your Qt installation path
    exit /b 1
)

REM 构建 64 位完整版本
echo.
echo ========================================
echo Building x64 (full)...
echo ========================================
if not exist "%BUILD_X64%" mkdir "%BUILD_X64%"
cmake -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="%QT_PREFIX%;%VCPKG_PREFIX%" -S "%ROOT_DIR%" -B "%BUILD_X64%"
if errorlevel 1 goto :error
cmake --build "%BUILD_X64%" --config Release
if errorlevel 1 goto :error

REM 构建 32 位轻量DLL（不需要Qt，共享tsf_dll源码）
echo.
echo ========================================
echo Building x86 (lightweight TSF DLL)...
echo ========================================
if not exist "%BUILD_X86%" mkdir "%BUILD_X86%"
cmake -G "Visual Studio 17 2022" -A Win32 -S "%ROOT_DIR%" -B "%BUILD_X86%"
if errorlevel 1 goto :error
cmake --build "%BUILD_X86%" --config Release
if errorlevel 1 goto :error

REM 复制32位DLL到64位输出目录
echo.
echo ========================================
echo Copying 32-bit DLL...
echo ========================================
copy /Y "%BUILD_X86%\src\platform\windows\tsf_dll\Release\SuYan32.dll" "%BUILD_X64%\bin\Release\"

REM 复制Qt依赖到输出目录（windeployqt）
echo.
echo ========================================
echo Deploying Qt dependencies...
echo ========================================
windeployqt --release --no-translations "%BUILD_X64%\bin\Release\SuYanServer.exe"

echo.
echo ========================================
echo Build complete!
echo ========================================
echo 64-bit DLL: %BUILD_X64%\bin\Release\SuYan.dll
echo 32-bit DLL: %BUILD_X64%\bin\Release\SuYan32.dll
echo Server:     %BUILD_X64%\bin\Release\SuYanServer.exe
echo.
echo Next: cd installer ^&^& build_installer.bat
exit /b 0

:error
echo Build failed!
exit /b 1
