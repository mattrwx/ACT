#pragma once
#include <Windows.h>
#include <fstream>
#include <iostream>
#include <tlhelp32.h>
#include <print>
#include <bcrypt.h>
#include <vector>
#include <string>
#include <cstdint>
#include <stdexcept>
#include <unordered_map>
#include <filesystem>
#include <winternl.h>
#include "cache.hpp"
#include "utils.hpp"

namespace validate_modules
{
    inline std::unordered_map<HMODULE, std::vector<std::string>> hashes;

    void hash_module_sections();
    void compare_to_disk();
    void compare_module_hashes();
}