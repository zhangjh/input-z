# SuYan Server - 后台服务进程
# 为32位和64位客户端提供IPC服务

if(NOT WIN32)
    return()
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

set(SERVER_SOURCES
    main.cpp
    server_app.cpp
    server_app.h
)

add_executable(SuYanServer WIN32
    ${SERVER_SOURCES}
    server.rc
)

target_link_libraries(SuYanServer PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    suyan_core
    suyan_ui
    librime
    ole32
    oleaut32
    uuid
    advapi32
    user32
    shell32
)

target_include_directories(SuYanServer PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/ui
)

set_target_properties(SuYanServer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
)
