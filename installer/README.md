# SuYan Installer

NSIS-based installer for SuYan Input Method on Windows.

## Prerequisites

1. **NSIS** - Download and install from https://nsis.sourceforge.io/
2. **Qt6** - Set Qt6_DIR environment variable

## Building

### Quick Build (Recommended)

```batch
set Qt6_DIR=C:\Qt\6.7.2\msvc2019_64\lib\cmake\Qt6
scripts\build-windows-all.bat
cd installer
build_installer.bat
```

### Manual Build

```batch
# 1. 64-bit version
cmake -G "Visual Studio 17 2022" -A x64 -DQt6_DIR="%Qt6_DIR%" -S . -B build-win
cmake --build build-win --config Release

# 2. 32-bit client
cmake -G "Visual Studio 17 2022" -A Win32 -S . -B build-win-x86
cmake --build build-win-x86 --config Release

# 3. Copy 32-bit DLL
copy build-win-x86\src\platform\windows\tsf_client\Release\SuYan32.dll build-win\bin\Release\

# 4. Build installer
cd installer
build_installer.bat
```

## Output

`SuYan-1.0.0-Setup.exe` in the `installer` directory.

## What Gets Installed

- `SuYan.dll` - 64-bit TSF DLL (full functionality)
- `SuYan32.dll` - 32-bit TSF client (IPC to 64-bit)
- Qt6 runtime, RIME engine, themes, icons

## Installation Options

### Interactive Installation

Simply run the installer and follow the wizard.

### Silent Installation

```batch
REM Install to default location
SuYan-1.0.0-Setup.exe /S

REM Install to custom location
SuYan-1.0.0-Setup.exe /S /D=D:\Programs\SuYan
```

Note: The `/D=` parameter must be the last parameter.

## Uninstallation Options

### Interactive Uninstallation

Use Windows Settings > Apps > SuYan, or run `uninst.exe` from the installation directory.

### Silent Uninstallation

```batch
REM Uninstall, keep user data
uninst.exe /S

REM Uninstall and remove user data
uninst.exe /S /CLEANDATA
```

## What Gets Installed

- `SuYan.dll` - Main TSF input method DLL
- Qt6 runtime DLLs
- RIME engine and data files
- Theme files
- Icons

## Installation Locations

- **Program Files**: `C:\Program Files\SuYan\`
- **User Data**: `%APPDATA%\SuYan\` (created on first use)
- **Start Menu**: `Start Menu\Programs\SuYan\`

## After Installation

The input method is registered with Windows TSF. To use it:

1. Open Windows Settings
2. Go to Time & Language > Language & Region
3. Click on Chinese language options
4. Add SuYan as a keyboard
