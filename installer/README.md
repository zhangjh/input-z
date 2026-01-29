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

The installer deploys a centralized server architecture:

### TSF DLLs (System Directories)
- `C:\Windows\System32\SuYan.dll` - 64-bit TSF DLL for 64-bit applications
- `C:\Windows\SysWOW64\SuYan32.dll` - 32-bit TSF DLL for 32-bit applications (e.g., 企业微信)

### Server and Dependencies (Program Directory)
- `SuYanServer.exe` - Background server hosting RIME engine and candidate window
- Qt6 runtime DLLs (Qt6Core, Qt6Gui, Qt6Widgets, etc.)
- `rime.dll` - RIME input method engine
- RIME data files, themes, icons

### Architecture
```
64-bit App          32-bit App
    │                   │
    ▼                   ▼
SuYan.dll          SuYan32.dll
(System32)         (SysWOW64)
    │                   │
    └───────┬───────────┘
            │ Named Pipe IPC
            ▼
      SuYanServer.exe
      (Program Files)
```

The DLLs are pure Win32 IPC clients with no Qt/RIME dependencies. All heavy lifting happens in SuYanServer.

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

## Installation Locations

- **System32**: `C:\Windows\System32\SuYan.dll` (64-bit TSF DLL)
- **SysWOW64**: `C:\Windows\SysWOW64\SuYan32.dll` (32-bit TSF DLL)
- **Program Files**: `C:\Program Files\SuYan\` (Server, Qt, RIME, data)
- **User Data**: `%APPDATA%\SuYan\` (created on first use)
- **Start Menu**: `Start Menu\Programs\SuYan\`

## After Installation

The input method is registered with Windows TSF. To use it:

1. Open Windows Settings
2. Go to Time & Language > Language & Region
3. Click on Chinese language options
4. Add SuYan as a keyboard
