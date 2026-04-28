#include "exceptions.hpp"

void place_hook(EXCEPTION_RECORD* exception_record, CONTEXT* context_record)
{
    if (*cache::pointers::Wow64PrepareForExecution_pointer != (void*)place_hook)
    {
        std::println("[-] Detected Wow64PrepareForExecution hook.");
        cache::flags++;
    }

    if (cache::pointers::Wow64PrepareForExecution_pointer && cache::process::is_32_bit)
        ((LONG(*)(EXCEPTION_RECORD*, CONTEXT*))(cache::pointers::Wow64PrepareForExecution_original))(exception_record, context_record);
    
    if (!utils::is_valid_code_region((void*)context_record->Rip))
    {
        std::println("[-] Invalid RIP during exception.");
        cache::flags++;
    }
}

// NOTE: Fetching Wow64 pointer may differ on 32 bit, will have to check this later
void exceptions::place_exception_hook()
{
    std::println("[+] Placing exception hook.");
    auto ntdll = GetModuleHandleA("ntdll.dll");
    if (!ntdll)
    {
        std::println("[-] Failed to find ntdll.dll.");
        cache::flags++;
        return;
    }

    auto dispatcher = (uintptr_t)GetProcAddress(ntdll, "KiUserExceptionDispatcher");
    if (!dispatcher)
    {
        std::println("[-] Failed to find KiUserExceptionDispatcher.");
        cache::flags++;
        return;
    }

    auto rel_addr = *(int32_t*)(dispatcher + 4);
    void** function_ptr = (void**)(dispatcher + 8 + rel_addr);

    if (cache::process::is_32_bit && *function_ptr)
    {
        std::println("[-] Detected Wow64PrepareForExecution_pointer hook.");
        cache::flags++;
    }

    cache::pointers::Wow64PrepareForExecution_pointer = function_ptr;
    cache::pointers::Wow64PrepareForExecution_original = *function_ptr;

    DWORD old_protect{};
    VirtualProtect(function_ptr, sizeof(void*), PAGE_EXECUTE_READWRITE, &old_protect);
    *function_ptr = (void*)exception_handler;
    VirtualProtect(function_ptr, sizeof(void*), old_protect, &old_protect);

    return;
}