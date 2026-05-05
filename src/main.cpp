#include <Windows.h>
#include <print>
#include <thread>
#include "cache.hpp"
#include "internal/exceptions.hpp"
#include "internal/page_walk.hpp"
#include "internal/threads.hpp"
#include "internal/validate_modules.hpp"
#include "internal/vmt_hook_detection.hpp"

void main_thread()
{
    AllocConsole();

    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    freopen_s(&f, "CONIN$", "r", stdin);

    std::println("Hewwo");

    cache::init();

    // Cache info about game module
    utils::populate_pe(GetModuleHandle(0), cache::main_pe::dos_header, cache::main_pe::nt_headers);

    gfx_offsets::collect();

    // Not overwriting any read only sections
    exceptions::place_hook();

    // Shit must run early on (at least before WE make any modification)
    modules::init();

    while (true)
    {
        threads::validate_threads();

        modules::validate();

        gfx_offsets::check();

        pages::walk();

        // exceptions::force_exception();
    }
}

BOOL APIENTRY DllMain(HMODULE h_module, DWORD ul_reason_for_call, LPVOID)
{
    switch (ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
        {
            cache::local_module::handle = h_module;

            std::thread t(main_thread);
            t.detach();
            break;
        }

        case DLL_PROCESS_DETACH:
        {
            std::println("[X] Detected unload");
            FreeConsole();
            break;
        }

        case DLL_THREAD_ATTACH:
        {
            threads::handle_thread_creation();
            break;
        }

        case DLL_THREAD_DETACH:
            break;
    }
    return TRUE;
}
