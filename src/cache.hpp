#pragma once
#include <Windows.h>
#include <cstdint>

namespace cache
{
    namespace process
    {
        inline DWORD id;
    }

    namespace local_module
    {
        inline HMODULE handle{};
    }

    namespace pointers
    {
        inline void** Wow64PrepareForExecution_pointer{};
    }

    namespace main_pe
    {
        inline IMAGE_DOS_HEADER* dos_header{};
        inline IMAGE_NT_HEADERS64* nt_headers{};
    }

    inline HWND game_hwnd{};
    inline void* game_d3d8_device{};
    inline void* game_d3d9_device{};
    inline void* game_d3d9_swap{};
    inline void* game_d3d10_swap{};
    inline void* game_d3d10_swap1{};
    inline void* game_d3d11_swap{};
    inline void* game_d3d11_swap1{};
    inline void* game_d3d12_swap{};
    inline void* game_d3d12_swap1{};
    inline void* game_d3d12_queue{};

    inline void init()
    {
        process::id = GetCurrentProcessId();
    }
}
