#ifdef _WIN32
#include <Windows.h>
#include <iostream>
#include <print>
#include <thread>
#include "cache.hpp"
#include "internal/exceptions.hpp"

void main_thread()
{
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
            AllocConsole();
            FILE *f;
            fopen_s(&f, "CONOUT$", "w");
            fopen_s(&f, "CONIN$", "w");

            std::println("[+] Started: 0x{:X}", (uintptr_t)h_module);

            cache::local_module::handle = h_module;

            std::thread t(main_thread);
            t.join();

            return TRUE;
        }
        case DLL_PROCESS_DETACH:
        {
            std::println("[-] Detected process unload!");
            FreeConsole();
        }
        case DLL_THREAD_ATTACH:
            break;
        case DLL_THREAD_DETACH:
            break;
    }
    return TRUE;
}
#endif