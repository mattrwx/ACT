#pragma once
#include <print>
#include <unordered_set>


enum class flags : uint8_t
{
    failed_to_open_threads_snapshot,
    failed_to_open_first_thread,
    thread_handle_failed_to_open,
    NtQueryInformationThread_failure,
    invalid_thread_start_address,
    exception_tampering,
    Wow64PrepareForExecution_hook,
    failed_to_find_KiUserExceptionDispatcher,
    failed_to_find_ntdll,
    invalid_rip_during_exception,
    invalid_executable_page,
    new_module_trust_verification,
    section_count_changed,
    section_hash_changed,
    section_protection_changed,
    failed_to_open_module_file,
    directx_vmt_hook
};

inline std::unordered_set<flags> flags_raised;

void raise_flag(flags flag, const char* note);
