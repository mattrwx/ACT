#include <Windows.h>
#include <iostream>
#include <print>
#include <thread>
#include "cache.hpp"
#include "internal/exceptions.hpp"

void main_thread()
{
    AllocConsole();

    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    freopen_s(&f, "CONIN$", "r", stdin);

    std::println("[+] Started: 0x{:X}", (uintptr_t)cache::local_module::handle);
        
    // Cache info about game module
    utils::populate_pe(GetModuleHandle(0), cache::main_pe::dos_header, cache::main_pe::nt_headers);

    place_exception_hook();

    while (true)
    {
        std::println("[+] Starting scan!");



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
            auto tid = GetCurrentThreadId();

            std::println("[+] Thread Started: {}", tid);

            OpenThread(THREAD_ALL_ACCESS, false, tid);
            
            if (!utils::is_valid_code_region())
            {

            }
        }

        case DLL_THREAD_DETACH:
        {

        }
    }
    return TRUE;
}