#include "page_walk.hpp"

void walk_thread()
{
    SYSTEM_INFO system_info;
    GetSystemInfo(&system_info);

    for (auto addr{system_info.lpMinimumApplicationAddress}; addr < system_info.lpMaximumApplicationAddress; addr = (void*)((uintptr_t)addr + 1000))
    {
        MEMORY_BASIC_INFORMATION mbi;
        if (!VirtualQuery(addr, &mbi, 0x1000))
            continue;

        if (utils::is_valid_code_region(addr))
            continue;

        if (!(mbi.Protect & PAGE_EXECUTE))
            continue;

        if (!(mbi.Protect & PAGE_EXECUTE_READ))
            continue;

        if (!(mbi.Protect & PAGE_EXECUTE_READWRITE))
            continue;

        std::println("[-] Detected executable page not belonging to valid module: 0x{:X}", (uintptr_t)addr);
        flags_raised.insert(flags::invalid_executable_page);
    }
}

void pages::walk()
{
    std::thread(walk_thread).detach();
}
