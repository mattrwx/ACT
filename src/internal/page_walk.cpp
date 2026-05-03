#include "page_walk.hpp"

void pages::walk()
{
    SYSTEM_INFO system_info;
    GetSystemInfo(&system_info);

    MEMORY_BASIC_INFORMATION mbi{};
    for (auto addr{system_info.lpMinimumApplicationAddress}; addr < system_info.lpMaximumApplicationAddress;
         addr = (void*)((uintptr_t)addr + (mbi.RegionSize ? mbi.RegionSize : system_info.dwPageSize)))
    {
        if (!VirtualQuery(addr, &mbi, sizeof(mbi)))
            continue;

        HMODULE hmod{};
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)mbi.AllocationBase, &hmod))
            if (mbi.State == MEM_COMMIT && (mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) && !utils::is_valid_code_region(addr))
                raise_flag(flags::invalid_executable_page, std::format("Detected executable page not belonging to valid module: 0x{:X}", (uintptr_t)addr).c_str());
    }
}
