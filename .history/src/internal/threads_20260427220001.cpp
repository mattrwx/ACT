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

            PVOID start_address = nullptr;
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

            }
}