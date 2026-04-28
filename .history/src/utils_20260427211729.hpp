#pragma once
#include <Windows.h>
#include <iostream>
#include <print>
#include "cache.hpp"

namespace utils
{
    void populate_pe(HMODULE h_module, IMAGE_DOS_HEADER*& dos_header, IMAGE_NT_HEADERS*& nt_headers);

    bool IsValidCodeRegion(void* address);
}