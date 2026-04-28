#pragma once
#include <Windows.h>
#include <iostream>
#include <print>
#include <winternl.h>
#include <wintrust.h>
#include <softpub.h>
#include "cache.hpp"


namespace utils
{
    void populate_pe(HMODULE h_module, IMAGE_DOS_HEADER*& dos_header, IMAGE_NT_HEADERS*& nt_headers);

    bool is_valid_code_region(void* address);

    bool verify_trust(const wchar_t* path);
}