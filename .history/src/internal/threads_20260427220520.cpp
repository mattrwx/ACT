#include "threads.hpp"

void handle_thread_creation()
{
    auto tid = GetCurrentThreadId();

    std::println("[+] Thread Started: {}", tid);

    HANDLE h_thread = OpenThread(THREAD_ALL_ACCESS, false, tid);

    if (!h_thread)
    {
        std::println("[-] Thread handle failed to open.");
        cache::flags++;
        return;
    }

    void* start_address = nullptr;
    NTSTATUS status = NtQueryInformationThread(
        h_thread,
        ThreadQuerySetWin32StartAddress,
        &start_address,
        sizeof(start_address),
        nullptr
    );

    if (!status)
    {
        std::println("[-] NtQueryInformationThread failure.");
        cache::flags++;
        return;
    }

    if (!utils::is_valid_code_region(start_address))
    {
        std::println("[-] Start address from invalid region of memory: 0x{:X}", (uintptr_t)start_address);
        cache::flags++;
        return;
    }
}

void threads::validate_thread_start_addresses()
{
    std::println("[+] validate_thread_start_addresses() is starting...");

    auto nt_query = NtQueryInformationThread();
    if (!nt_query)
    {
        std::println("[!] Failed to get_nt_query_information_thread()");
        cache::flags++;
        return;
    }

    DWORD current_pid = GetCurrentProcessId();
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        std::println("[!] Failed to CreateToolhelp32Snapshot");
        cache::flags++;
        return;
    }

    THREADENTRY32 te32{};
    te32.dwSize = sizeof(THREADENTRY32);

    if (!Thread32First(snapshot, &te32))
    {
        CloseHandle(snapshot);
        std::println("[!] Failed to enumerate threads with Thread32First");
        cache::flags++;
        return;
    }

    uint32_t orospu_count = 0;
    uint32_t total_count = 0;

    do
    {
        if (te32.th32OwnerProcessID != current_pid)
            continue;

        total_count++;

        HANDLE h_thread = OpenThread(THREAD_QUERY_INFORMATION, FALSE, te32.th32ThreadID);
        if (!h_thread)
            continue;
            
        PVOID start_address = nullptr;
        NTSTATUS status = nt_query(
            h_thread,
            ThreadQuerySetWin32StartAddress,
            &start_address,
            sizeof(start_address),
            nullptr
        );

        CloseHandle(h_thread);

        if (status != 0)
        {
            std::println("[!] Failed to query thread {} start address (status: 0x{:X}).", te32.th32ThreadID, status);
            continue;
        }
        uintptr_t addr = reinterpret_cast<uintptr_t>(start_address);

        /*
            Check 1: IsValidCodeRegion - checks if the address is in a valid code section
            Check 2: Module bounds validation using cache::main_pe, ofc it should be developed
            Check 3: GetModuleHandleExA stealth/bypass or sth check
        */ 
        if (!utils::IsValidCodeRegion(start_address))
        {
            std::println("[!] Suspicious thread detected: TID={}, StartAddress=0x{:X} (unmapped/private memory)",
                te32.th32ThreadID, addr);
            orospu_count++;
            cache::flags++;
            continue;
        }

        bool within_module_bounds = false;
        if (cache::main_pe::dos_header && cache::main_pe::nt_headers)
        {
            uintptr_t image_base = reinterpret_cast<uintptr_t>(cache::main_pe::dos_header);
            uintptr_t image_size = cache::main_pe::nt_headers->OptionalHeader.SizeOfImage;
            if (addr >= image_base && addr < (image_base + image_size))
            {
                within_module_bounds = true;
            }
        }
        HMODULE h_module = nullptr;
        BOOL module_found = GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
            static_cast<LPCSTR>(start_address),
            &h_module
        );

        if (!module_found || !h_module)
        {
            std::println("[!] Suspicious thread detected: TID={}, StartAddress=0x{:X} (no associated module)",
                te32.th32ThreadID, addr);
            orospu_count++;
            cache::flags++;
            continue;
        }

        if (h_module)
            FreeLibrary(h_module);

        if (cache::process::is_32_bit)
        {
            if (addr > 0xFFFFFFFF)
            {
            std::println("[!] Suspicious thread detected: TID={}, StartAddress=0x{:X} (64-bit address in 32-bit process)",
                    te32.th32ThreadID, addr);
                orospu_count++;
                cache::flags++;
                continue;
            }
        }

    } while (Thread32Next(snapshot, &te32));

    CloseHandle(snapshot);

    std::println("[+] Thread validation complete. Total: {}, Suspicious: {}", total_count, orospu_count);

    if (orospu_count > 0)
    {
        std::println("[!] {} suspicious thread(s) detected. Flags incremented to {}.",
            orospu_count, cache::flags);
    }
}