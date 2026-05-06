cmake_minimum_required(VERSION 3.20)
project(ACT)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(ACT_SOURCES
    src/main.cpp
    src/utils.cpp
    src/flags.cpp
    src/gui/gui.cpp
    src/internal/exceptions.cpp
    src/internal/threads.cpp
    src/internal/validate_modules.cpp
    src/internal/page_walk.cpp
    src/internal/vtable.cpp
    src/external/overlay.cpp
)
add_library(act SHARED ${ACT_SOURCES})

target_include_directories(act PRIVATE src)

target_compile_definitions(act PRIVATE
    ACT_EXPORTS
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _CRT_SECURE_NO_WARNINGS
)

target_compile_options(act PRIVATE
    "$<$<CXX_COMPILER_ID:MSVC>:/DThreadQuerySetWin32StartAddress=((THREADINFOCLASS)9)>"
)

set_target_properties(act PROPERTIES
    PREFIX ""
    OUTPUT_NAME "ACT"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/third_party/imgui)

add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
)

target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

target_compile_definitions(imgui PUBLIC
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

target_link_libraries(imgui PUBLIC
    d3d11
    dxgi
    d3dcompiler
    dwmapi
)

target_link_libraries(act PRIVATE
    imgui
    ntdll
    bcrypt
    wintrust
    crypt32
    user32
    d3d9
    d3d10
    d3d12
)