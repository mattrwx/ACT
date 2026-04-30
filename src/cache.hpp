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

    inline void init()
    {
        process::id = GetCurrentProcessId();
    }
}
