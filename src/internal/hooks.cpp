#include "hooks.hpp"
#include <format>

using SendInput_t = UINT(WINAPI*)(UINT, LPINPUT, int);

std::optional<hooks::push_ret<SendInput_t>> SendInput_hk;

UINT WINAPI SendInput_dt(UINT cInputs, LPINPUT pInputs, int cbSize)
{
    raise_flag(flags::mouse_movement, "SendInput Called");

    if (!utils::is_valid_code_region(__builtin_return_address(0)))
        raise_flag(flags::illegal_return_address, std::format("Illegal return address: 0x{:X}", (uintptr_t)__builtin_return_address(0)).c_str());

    return SendInput_hk->call_original(cInputs, pInputs, cbSize);
}

void hooks::place_input_hooks()
{
    SendInput_hk.emplace((void*)SendInput, (void*)SendInput_dt);
}