; ============================================
; SuYan Input Method - NSIS Installer Script
; ============================================
; Requirements: 11.1, 11.2, 11.3

!include "MUI2.nsh"
!include "FileFunc.nsh"
!include "x64.nsh"

; ============================================
; Product Information
; ============================================
!define PRODUCT_NAME "SuYan"
!define PRODUCT_NAME_CN "素言输入法"
!define PRODUCT_VERSION "1.0.0"
!define PRODUCT_PUBLISHER "zhangjh"
!define PRODUCT_WEB_SITE "https://github.com/zhangjh"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!define PRODUCT_UNINST_ROOT_KEY "HKLM"

; ============================================
; Installer Attributes
; ============================================
Name "${PRODUCT_NAME_CN} ${PRODUCT_VERSION}"
OutFile "SuYan-${PRODUCT_VERSION}-Setup.exe"
InstallDir "$PROGRAMFILES64\${PRODUCT_NAME}"
InstallDirRegKey HKLM "${PRODUCT_UNINST_KEY}" "InstallLocation"
RequestExecutionLevel admin
ShowInstDetails show
ShowUnInstDetails show
Unicode True

; ============================================
; Version Information
; ============================================
VIProductVersion "${PRODUCT_VERSION}.0"
VIAddVersionKey "ProductName" "${PRODUCT_NAME_CN}"
VIAddVersionKey "CompanyName" "${PRODUCT_PUBLISHER}"
VIAddVersionKey "LegalCopyright" "Copyright © 2026 ${PRODUCT_PUBLISHER}"
VIAddVersionKey "FileDescription" "${PRODUCT_NAME_CN} Installer"
VIAddVersionKey "FileVersion" "${PRODUCT_VERSION}"
VIAddVersionKey "ProductVersion" "${PRODUCT_VERSION}"

; ============================================
; MUI Settings
; ============================================
!define MUI_ABORTWARNING
!define MUI_ICON "..\resources\icons\app-icon.ico"
!define MUI_UNICON "..\resources\icons\app-icon.ico"
!define MUI_WELCOMEFINISHPAGE_BITMAP "${NSISDIR}\Contrib\Graphics\Wizard\win.bmp"

; Welcome page
!insertmacro MUI_PAGE_WELCOME
; License page (optional - uncomment if you have a license file)
; !insertmacro MUI_PAGE_LICENSE "LICENSE.txt"
; Directory page
!insertmacro MUI_PAGE_DIRECTORY
; Install files page
!insertmacro MUI_PAGE_INSTFILES
; Finish page
!define MUI_FINISHPAGE_NOAUTOCLOSE
!define MUI_FINISHPAGE_TEXT "$(FinishPageText)"
!insertmacro MUI_PAGE_FINISH

; Uninstaller pages
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

; ============================================
; Languages
; ============================================
!insertmacro MUI_LANGUAGE "SimpChinese"
!insertmacro MUI_LANGUAGE "English"

; Language strings
LangString FinishPageText ${LANG_SIMPCHINESE} "${PRODUCT_NAME_CN} 安装完成。$\r$\n$\r$\n请在 Windows 设置中添加素言输入法：$\r$\n设置 → 时间和语言 → 语言和区域 → 中文 → 语言选项 → 添加键盘"
LangString FinishPageText ${LANG_ENGLISH} "${PRODUCT_NAME} installation complete.$\r$\n$\r$\nPlease add SuYan input method in Windows Settings:$\r$\nSettings → Time & Language → Language & Region → Chinese → Language Options → Add a keyboard"

LangString UninstConfirm ${LANG_SIMPCHINESE} "确定要卸载 ${PRODUCT_NAME_CN} 吗？"
LangString UninstConfirm ${LANG_ENGLISH} "Are you sure you want to uninstall ${PRODUCT_NAME}?"

LangString CleanUserData ${LANG_SIMPCHINESE} "是否同时删除用户数据（词库、配置等）？"
LangString CleanUserData ${LANG_ENGLISH} "Do you also want to delete user data (dictionaries, settings, etc.)?"

; ============================================
; Installer Sections
; ============================================
Section "MainSection" SEC01
    SetOutPath "$INSTDIR"
    SetOverwrite on

    ; ========================================
    ; Core DLL and Dependencies
    ; ========================================
    File "..\build-win\bin\Release\SuYan.dll"
    
    ; 32-bit TSF client for 32-bit applications
    File /nonfatal "..\build-win\bin\Release\SuYan32.dll"
    
    ; Background server process
    File "..\build-win\bin\Release\SuYanServer.exe"
    
    File "..\build-win\bin\Release\rime.dll"
    File "..\build-win\bin\Release\sqlite3.dll"
    File "..\build-win\bin\Release\yaml-cpp.dll"
    
    ; Qt Core DLLs
    File "..\build-win\bin\Release\Qt6Core.dll"
    File "..\build-win\bin\Release\Qt6Gui.dll"
    File "..\build-win\bin\Release\Qt6Widgets.dll"
    File "..\build-win\bin\Release\Qt6Svg.dll"
    File "..\build-win\bin\Release\Qt6Network.dll"
    
    ; DirectX dependencies (optional, for software rendering fallback)
    File /nonfatal "..\build-win\bin\Release\D3Dcompiler_47.dll"
    File /nonfatal "..\build-win\bin\Release\opengl32sw.dll"
    
    ; ========================================
    ; Qt Plugins - platforms
    ; ========================================
    SetOutPath "$INSTDIR\platforms"
    File "..\build-win\bin\Release\platforms\qwindows.dll"
    
    ; ========================================
    ; Qt Plugins - styles
    ; ========================================
    SetOutPath "$INSTDIR\styles"
    File /nonfatal "..\build-win\bin\Release\styles\*.dll"
    
    ; ========================================
    ; Qt Plugins - imageformats
    ; ========================================
    SetOutPath "$INSTDIR\imageformats"
    File "..\build-win\bin\Release\imageformats\qgif.dll"
    File "..\build-win\bin\Release\imageformats\qico.dll"
    File "..\build-win\bin\Release\imageformats\qjpeg.dll"
    File "..\build-win\bin\Release\imageformats\qsvg.dll"
    
    ; ========================================
    ; Qt Plugins - iconengines
    ; ========================================
    SetOutPath "$INSTDIR\iconengines"
    File "..\build-win\bin\Release\iconengines\qsvgicon.dll"
    
    ; ========================================
    ; Themes
    ; ========================================
    SetOutPath "$INSTDIR\themes"
    File "..\resources\themes\dark.yaml"
    File "..\resources\themes\light.yaml"
    
    ; ========================================
    ; Icons
    ; ========================================
    SetOutPath "$INSTDIR\icons"
    File "..\resources\icons\app-icon.ico"
    File "..\resources\icons\generated\status\chinese.svg"
    File "..\resources\icons\generated\status\english.svg"

    ; ========================================
    ; RIME Data Files
    ; ========================================
    SetOutPath "$INSTDIR\rime"
    File "..\data\rime\custom_phrase.txt"
    File "..\data\rime\default.yaml"
    File "..\data\rime\melt_eng.dict.yaml"
    File "..\data\rime\melt_eng.schema.yaml"
    File "..\data\rime\rime_ice.dict.yaml"
    File "..\data\rime\rime_ice.schema.yaml"
    File "..\data\rime\symbols_caps_v.yaml"
    File "..\data\rime\symbols_v.yaml"
    
    ; RIME Chinese dictionaries
    SetOutPath "$INSTDIR\rime\cn_dicts"
    File "..\data\rime\cn_dicts\*.yaml"
    
    ; RIME English dictionaries
    SetOutPath "$INSTDIR\rime\en_dicts"
    File "..\data\rime\en_dicts\*.yaml"
    File "..\data\rime\en_dicts\*.txt"
    
    ; RIME Lua scripts
    SetOutPath "$INSTDIR\rime\lua"
    File "..\data\rime\lua\*.lua"
    
    ; RIME Lua cold_word_drop module
    SetOutPath "$INSTDIR\rime\lua\cold_word_drop"
    File "..\data\rime\lua\cold_word_drop\*.lua"
    
    ; RIME OpenCC data
    SetOutPath "$INSTDIR\rime\opencc"
    File "..\data\rime\opencc\*.json"
    File "..\data\rime\opencc\*.txt"
    
    ; ========================================
    ; Register COM Component (TSF)
    ; ========================================
    SetOutPath "$INSTDIR"
    
    ; Register the 64-bit DLL
    ExecWait 'regsvr32.exe /s "$INSTDIR\SuYan.dll"'
    
    ; Register the 32-bit DLL (if exists)
    IfFileExists "$INSTDIR\SuYan32.dll" 0 +2
        ExecWait 'regsvr32.exe /s "$INSTDIR\SuYan32.dll"'
    
    ; ========================================
    ; Create Start Menu Shortcuts
    ; ========================================
    CreateDirectory "$SMPROGRAMS\${PRODUCT_NAME}"
    CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\卸载 ${PRODUCT_NAME_CN}.lnk" "$INSTDIR\uninst.exe"
    
    ; ========================================
    ; Write Uninstaller
    ; ========================================
    WriteUninstaller "$INSTDIR\uninst.exe"
    
    ; ========================================
    ; Write Registry Keys for Add/Remove Programs
    ; ========================================
    WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayName" "${PRODUCT_NAME_CN}"
    WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "UninstallString" "$INSTDIR\uninst.exe"
    WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayIcon" "$INSTDIR\icons\app-icon.ico"
    WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
    WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"
    WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "InstallLocation" "$INSTDIR"
    
    ; Calculate installed size
    ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
    IntFmt $0 "0x%08X" $0
    WriteRegDWORD ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "EstimatedSize" "$0"
SectionEnd


; ============================================
; Uninstaller Section
; ============================================
Section "Uninstall"
    SetShellVarContext all
    
    ; ========================================
    ; Unregister COM Component (TSF)
    ; ========================================
    ; Unregister 32-bit DLL first (if exists)
    IfFileExists "$INSTDIR\SuYan32.dll" 0 +2
        ExecWait 'regsvr32.exe /u /s "$INSTDIR\SuYan32.dll"'
    
    ; Unregister 64-bit DLL
    ExecWait 'regsvr32.exe /u /s "$INSTDIR\SuYan.dll"'
    
    ; ========================================
    ; Remove Start Menu Shortcuts
    ; ========================================
    Delete "$SMPROGRAMS\${PRODUCT_NAME}\卸载 ${PRODUCT_NAME_CN}.lnk"
    RMDir "$SMPROGRAMS\${PRODUCT_NAME}"
    
    ; ========================================
    ; Remove Installation Files
    ; ========================================
    ; Core DLLs
    Delete "$INSTDIR\SuYan.dll"
    Delete "$INSTDIR\SuYan32.dll"
    Delete "$INSTDIR\SuYanServer.exe"
    Delete "$INSTDIR\rime.dll"
    Delete "$INSTDIR\sqlite3.dll"
    Delete "$INSTDIR\yaml-cpp.dll"
    Delete "$INSTDIR\Qt6Core.dll"
    Delete "$INSTDIR\Qt6Gui.dll"
    Delete "$INSTDIR\Qt6Widgets.dll"
    Delete "$INSTDIR\Qt6Svg.dll"
    Delete "$INSTDIR\Qt6Network.dll"
    Delete "$INSTDIR\D3Dcompiler_47.dll"
    Delete "$INSTDIR\opengl32sw.dll"
    
    ; Qt plugins
    Delete "$INSTDIR\platforms\qwindows.dll"
    RMDir "$INSTDIR\platforms"
    
    RMDir /r "$INSTDIR\styles"
    RMDir /r "$INSTDIR\imageformats"
    RMDir /r "$INSTDIR\iconengines"
    
    ; Themes and icons
    RMDir /r "$INSTDIR\themes"
    RMDir /r "$INSTDIR\icons"
    
    ; RIME data
    RMDir /r "$INSTDIR\rime"
    
    ; Uninstaller
    Delete "$INSTDIR\uninst.exe"
    
    ; Remove installation directory
    RMDir "$INSTDIR"
    
    ; ========================================
    ; Remove Registry Keys
    ; ========================================
    DeleteRegKey ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}"
    
    ; ========================================
    ; Optional: Clean User Data
    ; In silent mode, skip user data cleanup (preserve by default)
    ; Use /CLEANDATA parameter for silent uninstall with data cleanup
    ; ========================================
    IfSilent skip_userdata_prompt
        MessageBox MB_YESNO|MB_ICONQUESTION "$(CleanUserData)" IDNO skip_userdata
            RMDir /r "$APPDATA\${PRODUCT_NAME}"
            Goto skip_userdata
    skip_userdata_prompt:
        ; Check for /CLEANDATA parameter in silent mode
        ${GetParameters} $R0
        ${GetOptions} $R0 "/CLEANDATA" $R1
        IfErrors skip_userdata
            RMDir /r "$APPDATA\${PRODUCT_NAME}"
    skip_userdata:
SectionEnd

; ============================================
; Installer Functions
; ============================================
Function .onInit
    ; Check for 64-bit Windows
    ${IfNot} ${RunningX64}
        IfSilent +2
            MessageBox MB_OK|MB_ICONSTOP "This application requires 64-bit Windows."
        Abort
    ${EndIf}
    
    ; Check for admin rights
    UserInfo::GetAccountType
    Pop $0
    ${If} $0 != "admin"
        IfSilent +2
            MessageBox MB_OK|MB_ICONSTOP "Administrator privileges required for installation."
        Abort
    ${EndIf}
FunctionEnd

Function un.onInit
    IfSilent +3
        MessageBox MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2 "$(UninstConfirm)" IDYES +2
        Abort
FunctionEnd

; ============================================
; Silent Installation Support
; ============================================
; The installer supports the following command-line parameters:
;
; Installation:
;   /S              - Silent installation (no UI)
;   /D=<path>       - Custom installation directory (must be last parameter)
;
; Examples:
;   SuYan-1.0.0-Setup.exe /S
;   SuYan-1.0.0-Setup.exe /S /D=D:\Programs\SuYan
;
; Uninstallation:
;   /S              - Silent uninstallation
;   /CLEANDATA      - Also remove user data (only in silent mode)
;
; Examples:
;   uninst.exe /S
;   uninst.exe /S /CLEANDATA
