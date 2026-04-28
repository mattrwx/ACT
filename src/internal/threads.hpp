#pragma once
#include <Windows.h>
#include <winternl.h>
#include <tlhelp32.h>
#include "cache.hpp"
#include "utils.hpp"


namespace threads
{
    void handle_thread_creation();

    void validate_thread(DWORD tid);

    void validate_threads();
}