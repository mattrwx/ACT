#pragma once
#include <Windows.h>
#include <unordered_map>
#include <utility>
#include <vector>
#include <winternl.h>
#include "../flags.hpp"


namespace rtti
{
    std::vector<std::pair<void**, size_t>> get_vtables();
}
