#pragma once
#include <Windows.h>
#include <bcrypt.h>
#include <filesystem>
#include <fstream>
#include <print>
#include <string>
#include <unordered_map>
#include <vector>
#include <winternl.h>


#include "../flags.hpp"
#include "cache.hpp"
#include "utils.hpp"


struct section_t
{
    std::string name;
    std::string hash;
    DWORD protection;
};

struct module_t
{
    std::string name;
    uintptr_t base;
    size_t size;
    IMAGE_DOS_HEADER* dos;
    IMAGE_NT_HEADERS* nt;
    std::vector<section_t> sections;
};

namespace modules
{
    inline std::unordered_map<HMODULE, module_t> module_map;

    void init();
    void validate();
    void rehash_containing_section(void* address);
}
