#include "exceptions.hpp"

void exception_handler(EXCEPTION_RECORD* exception_record, CONTEXT* context_record)
{
    if (*cache::pointers::Wow64PrepareForExecution_pointer != (void*)exception_handler)
    {
        std::println("[-] Detected Wow64PrepareForExecution hook.");
        flags_raised.insert(flags::Wow64PrepareForExecution_hook);
    }

    if (!utils::is_valid_code_region((void*)context_record->Rip))
    {
        std::println("[-] Invalid RIP during exception.");
        flags_raised.insert(flags::invalid_rip_during_exception);
    }
}

// NOTE: Fetching Wow64 pointer may differ on 32 bit, will have to check this later
void exceptions::place_hook()
{
    std::println("[+] Placing exception hook.");
    auto ntdll = GetModuleHandleA("ntdll.dll");
    if (!ntdll)
    {
        std::println("[-] Failed to find ntdll.dll.");
        flags_raised.insert(flags::failed_to_find_ntdll);
        return;
    }

    auto dispatcher = (uintptr_t)GetProcAddress(ntdll, "KiUserExceptionDispatcher");
    if (!dispatcher)
    {
        std::println("[-] Failed to find KiUserExceptionDispatcher.");
        flags_raised.insert(flags::failed_to_find_KiUserExceptionDispatcher);
        return;
    }

    auto rel_addr = *(int32_t*)(dispatcher + 4);
    void** function_ptr = (void**)(dispatcher + 8 + rel_addr);

    if (*function_ptr)
    {
        std::println("[-] Detected Wow64PrepareForExecution_pointer hook.");
        flags_raised.insert(flags::Wow64PrepareForExecution_hook);
    }

    cache::pointers::Wow64PrepareForExecution_pointer = function_ptr;

    DWORD old_protect{};
    VirtualProtect(function_ptr, sizeof(void*), PAGE_EXECUTE_READWRITE, &old_protect);
    *function_ptr = (void*)exception_handler;
    VirtualProtect(function_ptr, sizeof(void*), old_protect, &old_protect);

    return;
}

// If no exception is ever thrown then we can't ensure that Wow64PrepareForException is not being hooked.
// A exception based debugger using this hook would just ZwContinue or iret before anything even happened.
void exceptions::force_exception()
{
    flags_raised.insert(flags::exception_tampering);
    try
    {
        *(volatile int*)0 = 0;
    }
    catch (...)
    {
        flags_raised.erase(flags::exception_tampering);
    }
}
