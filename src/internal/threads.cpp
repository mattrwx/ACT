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
        raise_flag(flags::thread_handle_failed_to_open, "Thread handle failed to open.");
        return;
    }

    void* start_address{};
    NTSTATUS status = NtQueryInformationThread(h_thread, ThreadQuerySetWin32StartAddress, &start_address, sizeof(start_address), nullptr);

    if (!NT_SUCCESS(status))
    {
        raise_flag(flags::NtQueryInformationThread_failure, "NtQueryInformationThread failure.");
        return;
    }

    if (!utils::is_valid_code_region(start_address))
    {
        raise_flag(flags::invalid_thread_start_address, std::format("Start address from invalid region of memory: 0x{:X}", (uintptr_t)start_address).c_str());
        return;
    }
}

void threads::validate_threads()
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        raise_flag(flags::failed_to_open_threads_snapshot, "Failed to open threads snapshot.");
        return;
    }

    THREADENTRY32 thread_entry{};
    thread_entry.dwSize = sizeof(THREADENTRY32);

    if (!Thread32First(snapshot, &thread_entry))
    {
        raise_flag(flags::failed_to_open_first_thread, "Failed to open first thread.");
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
