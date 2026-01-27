# SuYan TSF Client (32位轻量级DLL)
# 不依赖Qt和RIME，只做TSF桥接和IPC通信

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
    SUYAN_TSF_CLIENT
)

if(MSVC)
    add_compile_options(/utf-8 /W4 /WX- /MP)
endif()

set(TSF_CLIENT_SOURCES
    tsf_client.cpp
    ipc_client.cpp
)

set(TSF_CLIENT_HEADERS
    tsf_client.h
    ipc_client.h
)

add_library(SuYanTSFClient SHARED
    ${TSF_CLIENT_SOURCES}
    ${TSF_CLIENT_HEADERS}
    tsf_client.def
    tsf_client.rc
)

set_target_properties(SuYanTSFClient PROPERTIES
    OUTPUT_NAME "SuYan32"
    PREFIX ""
    SUFFIX ".dll"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
)

target_link_libraries(SuYanTSFClient PRIVATE
    ole32
    oleaut32
    uuid
    advapi32
    user32
    shell32
)
