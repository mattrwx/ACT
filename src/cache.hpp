#pragma once
#include <Windows.h>
#include <cstdint>






namespace cache
{
    inline uint8_t flags{};
    
    namespace process
    {
        inline bool is_32_bit{};
    }
    
    namespace local_module
    {
        inline HMODULE handle{};
    }

    namespace pointers
    {
        inline void** Wow64PrepareForExecution_pointer{};
        inline void* Wow64PrepareForExecution_original{};
    }

    namespace main_pe
    {
        inline IMAGE_DOS_HEADER* dos_header{};
        inline IMAGE_NT_HEADERS64* nt_headers{};
    }
}