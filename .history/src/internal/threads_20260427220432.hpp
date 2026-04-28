#pragma once
#include <Windows.h>
#include <winternl.h>
#include <iostream>
#include <print>
#include "cache.hpp"
#include "utils.hpp"


namespace threads
{
    //threads::validate_thread_start_addresses(); 
    void validate_thread_start_addresses(); //EVERYTHING WILL HAVE UNDERLINE :D! :D! :D!
    
void handle_thread_creation();
}