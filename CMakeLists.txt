cmake_minimum_required(VERSION 3.20)
project(ACT)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(act SHARED
    src/main.cpp
    src/internal/exceptions.cpp
    src/internal/loop.cpp
    src/internal/signatures.cpp
    src/internal/threads.cpp
    src/internal/validate_modules.cpp
    src/internal/vtable.cpp
    src/utils.cpp
)

target_include_directories(act PRIVATE src)

target_compile_definitions(act PRIVATE
    ACT_EXPORTS
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

target_compile_options(act PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    --target=x86_64-pc-windows-gnu
    -static
)

target_link_options(act PRIVATE
    --target=x86_64-pc-windows-gnu
    -static
    -static-libgcc
    -static-libstdc++
)

set_target_properties(act PROPERTIES
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

target_link_libraries(act PRIVATE ntdll)