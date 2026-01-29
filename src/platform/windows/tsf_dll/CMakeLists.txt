# SuYan TSF DLL
# 纯 Win32 实现，无 Qt/RIME 依赖
# 同时支持 64 位和 32 位构建（共享源码）

if(NOT WIN32)
    return()
endif()

add_compile_definitions(
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
    _WIN32_WINNT=0x0A00
    WINVER=0x0A00
)

if(MSVC)
    add_compile_options(/utf-8 /W4 /WX- /MP)
endif()

set(TSF_DLL_SOURCES
    dll_main.cpp
    tsf_text_service.cpp
    ipc_client.cpp
    language_bar_button.cpp
)

set(TSF_DLL_HEADERS
    tsf_text_service.h
    ipc_client.h
    language_bar_button.h
    ${CMAKE_SOURCE_DIR}/src/shared/ipc_protocol.h
)

# 生成带绝对路径的资源文件（确保资源编译器能找到图标）
set(TSF_RC_FILE "${CMAKE_CURRENT_BINARY_DIR}/tsf_dll_generated.rc")
set(ICON_PATH "${CMAKE_SOURCE_DIR}/resources/icons/app-icon.ico")
file(TO_NATIVE_PATH "${ICON_PATH}" ICON_PATH_NATIVE)
# 将反斜杠转义为双反斜杠
string(REPLACE "\\" "\\\\" ICON_PATH_ESCAPED "${ICON_PATH_NATIVE}")

file(WRITE "${TSF_RC_FILE}"
    "// Auto-generated resource file\n"
    "#include <windows.h>\n"
    "\n"
    "// Icon resource (ID 101)\n"
    "101 ICON \"${ICON_PATH_ESCAPED}\"\n"
    "\n"
    "// Version info\n"
    "VS_VERSION_INFO VERSIONINFO\n"
    "FILEVERSION     1,0,0,0\n"
    "PRODUCTVERSION  1,0,0,0\n"
    "FILEFLAGSMASK   VS_FFI_FILEFLAGSMASK\n"
    "FILEFLAGS       0\n"
    "FILEOS          VOS_NT_WINDOWS32\n"
    "FILETYPE        VFT_DLL\n"
    "FILESUBTYPE     VFT2_UNKNOWN\n"
    "BEGIN\n"
    "    BLOCK \"StringFileInfo\"\n"
    "    BEGIN\n"
    "        BLOCK \"040904b0\"\n"
    "        BEGIN\n"
    "            VALUE \"CompanyName\",      \"SuYan\\\\0\"\n"
    "            VALUE \"FileDescription\",  \"SuYan Input Method TSF Module\\\\0\"\n"
    "            VALUE \"FileVersion\",      \"1.0.0.0\\\\0\"\n"
    "            VALUE \"InternalName\",     \"SuYan\\\\0\"\n"
    "            VALUE \"LegalCopyright\",   \"Copyright (C) 2024\\\\0\"\n"
    "            VALUE \"OriginalFilename\", \"SuYan.dll\\\\0\"\n"
    "            VALUE \"ProductName\",      \"SuYan Input Method\\\\0\"\n"
    "            VALUE \"ProductVersion\",   \"1.0.0.0\\\\0\"\n"
    "        END\n"
    "    END\n"
    "    BLOCK \"VarFileInfo\"\n"
    "    BEGIN\n"
    "        VALUE \"Translation\", 0x0409, 1200\n"
    "    END\n"
    "END\n"
)

set(TSF_DLL_RESOURCES
    ${TSF_RC_FILE}
)

# 根据架构设置输出名称
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(TSF_DLL_OUTPUT_NAME "SuYan")
else()
    set(TSF_DLL_OUTPUT_NAME "SuYan32")
endif()

# 生成架构特定的 .def 文件
set(TSF_DEF_FILE "${CMAKE_CURRENT_BINARY_DIR}/${TSF_DLL_OUTPUT_NAME}.def")
file(WRITE "${TSF_DEF_FILE}"
    "LIBRARY ${TSF_DLL_OUTPUT_NAME}\n"
    "EXPORTS\n"
    "    DllGetClassObject   PRIVATE\n"
    "    DllCanUnloadNow     PRIVATE\n"
    "    DllRegisterServer   PRIVATE\n"
    "    DllUnregisterServer PRIVATE\n"
)

add_library(SuYanTSF SHARED
    ${TSF_DLL_SOURCES}
    ${TSF_DLL_HEADERS}
    ${TSF_DLL_RESOURCES}
    ${TSF_DEF_FILE}
)

target_include_directories(SuYanTSF PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

set_target_properties(SuYanTSF PROPERTIES
    OUTPUT_NAME "${TSF_DLL_OUTPUT_NAME}"
    PREFIX ""
    SUFFIX ".dll"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

target_link_libraries(SuYanTSF PRIVATE
    ole32
    oleaut32
    uuid
    advapi32
    user32
    shell32
    imm32
)

# 仅在 64 位构建时生成注册脚本（避免重复）
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/register_admin.bat"
        "@echo off\n"
        "echo SuYan Input Method Registration\n"
        "net session >nul 2>&1\n"
        "if %errorlevel% neq 0 (\n"
        "    echo ERROR: Run as Administrator!\n"
        "    pause\n"
        "    exit /b 1\n"
        ")\n"
        "regsvr32 /s \"%~dp0SuYan.dll\"\n"
        "if exist \"%~dp0SuYan32.dll\" regsvr32 /s \"%~dp0SuYan32.dll\"\n"
        "echo Done.\n"
        "pause\n"
    )

    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/unregister_admin.bat"
        "@echo off\n"
        "echo SuYan Input Method Unregistration\n"
        "net session >nul 2>&1\n"
        "if %errorlevel% neq 0 (\n"
        "    echo ERROR: Run as Administrator!\n"
        "    pause\n"
        "    exit /b 1\n"
        ")\n"
        "if exist \"%~dp0SuYan32.dll\" regsvr32 /u /s \"%~dp0SuYan32.dll\"\n"
        "regsvr32 /u /s \"%~dp0SuYan.dll\"\n"
        "echo Done.\n"
        "pause\n"
    )

    add_custom_command(TARGET SuYanTSF POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_BINARY_DIR}/register_admin.bat"
            "${CMAKE_CURRENT_BINARY_DIR}/unregister_admin.bat"
            "$<TARGET_FILE_DIR:SuYanTSF>/"
        COMMENT "Copying registration scripts..."
    )
endif()
