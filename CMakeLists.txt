cmake_minimum_required(VERSION 3.20)
project(ACT)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON) 

add_library(act SHARED
    src/main.cpp,
    src/exceptions.cpp,
    src/loop.cpp,
    src/signatures.cpp,
    src/threads.cpp,
    src/validate_modules.cpp,
    src/vtable.cpp
)

target_compile_definitions(act PRIVATE ACT_EXPORTS)

target_compile_options(act PRIVATE -Wall -Wextra -Wpedantic)

set_target_properties(act PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)