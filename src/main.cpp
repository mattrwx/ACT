#include <Windows.h>
#include <print>
#include <thread>
#include "cache.hpp"
#include "internal/exceptions.hpp"
#include "internal/threads.hpp"

void main_thread()
{
    AllocConsole();

    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    freopen_s(&f, "CONIN$", "r", stdin);

    cache::init();

    std::println("[+] Started: 0x{:X}", (uintptr_t)cache::local_module::handle);
        
    // Cache info about game module
    utils::populate_pe(GetModuleHandle(0), cache::main_pe::dos_header, cache::main_pe::nt_headers);

    exceptions::place_hook();

    while (true)
    {
        threads::validate_threads();

        std::println("[+] Scan complete ({} flags)", cache::flags);
        Sleep(5000);
    }
}

BOOL APIENTRY DllMain(HMODULE h_module, DWORD ul_reason_for_call, LPVOID) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
        {
            cache::local_module::handle = h_module;

            std::thread t(main_thread);
            t.detach();
            break;
        }
        
        case DLL_PROCESS_DETACH:
        {
            MessageBox(0,"Detected Unload", "ACT", 0);
            FreeConsole();
            break;
        }

        case DLL_THREAD_ATTACH:
        {
            threads::handle_thread_creation();
            break;
        }

        case DLL_THREAD_DETACH:
        {

        }
    }
    return TRUE;
}