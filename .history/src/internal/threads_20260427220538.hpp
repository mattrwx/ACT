#pragma once
#include <Windows.h>
#include <winternl.h>
#include <tlhelp32.h>
#include <iostream>
#include <print>
#include "cache.hpp"
#include "utils.hpp"


namespace threads
{
    void validate_thread_start_addresses();

    void handle_thread_creation();
}