#include "threads.hpp"

void threads::handle_thread_creation()
{
    auto tid = GetCurrentThreadId();

    validate_thread(tid);
}

void threads::validate_thread(DWORD tid)
{

    HANDLE h_thread = OpenThread(THREAD_ALL_ACCESS, false, tid);

    if (!h_thread)
    {
        std::println("[-] Thread handle failed to open.");
        flags_raised.insert(flags::thread_handle_failed_to_open);
        return;
    }

    void* start_address{};
    NTSTATUS status = NtQueryInformationThread(h_thread, ThreadQuerySetWin32StartAddress, &start_address, sizeof(start_address), nullptr);

    if (!NT_SUCCESS(status))
    {
        std::println("[-] NtQueryInformationThread failure.");
        flags_raised.insert(flags::NtQueryInformationThread_failure);
        return;
    }

    if (!utils::is_valid_code_region(start_address))
    {
        std::println("[-] Start address from invalid region of memory: 0x{:X}", (uintptr_t)start_address);
        flags_raised.insert(flags::invalid_thread_start_address);
        return;
    }
}

void threads::validate_threads()
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        std::println("[-] Failed to open threads snapshot.");
        flags_raised.insert(flags::failed_to_open_threads_snapshot);
        return;
    }

    THREADENTRY32 thread_entry{};
    thread_entry.dwSize = sizeof(THREADENTRY32);

    if (!Thread32First(snapshot, &thread_entry))
    {
        std::println("[-] Failed to open first thread.");
        flags_raised.insert(flags::failed_to_open_first_thread);
        CloseHandle(snapshot);
        return;
    }

    do
    {
        if (thread_entry.th32OwnerProcessID != cache::process::id)
            continue;

        validate_thread(thread_entry.th32ThreadID);

    } while (Thread32Next(snapshot, &thread_entry));
}
