#pragma once
#include <Windows.h>
#include <tlhelp32.h>
#include <winternl.h>
#include "../flags.hpp"
#include "cache.hpp"
#include "utils.hpp"


namespace threads
{
    void handle_thread_creation();

    void validate_thread(DWORD tid);

    void validate_threads();
}
