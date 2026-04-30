cmake_minimum_required(VERSION 3.20)
project(ACT)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(ACT_SOURCES
    src/main.cpp
    src/utils.cpp
    src/internal/exceptions.cpp
    src/internal/threads.cpp
    src/internal/validate_modules.cpp
    src/internal/page_walk.cpp
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/internal/vmt_hook_detection.cpp"
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/internal/vmt_hook_detection.hpp")
    list(APPEND ACT_SOURCES src/internal/vmt_hook_detection.cpp)
endif()

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

target_link_libraries(act PRIVATE
    ntdll
    bcrypt
    wintrust
    crypt32
    user32
    d3d9
    d3d10
    d3d10_1
    d3d11
    d3d12
    dxgi
)