#pragma once
#include <Windows.h>
#include <winternl.h>
#include <iostream>
#include <print>
#include "cache.hpp"
#include "utils.hpp"

void handle_thread_creation();

#pragma once
#include <Windows.h>
#include <cstdint>

namespace threads
{
    //threads::validate_thread_start_addresses(); 
    void validate_thread_start_addresses(); //EVERYTHING WILL HAVE UNDERLINE :D! :D! :D!
}