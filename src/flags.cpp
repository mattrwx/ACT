#include "flags.hpp"

void raise_flag(flags flag, const char* note)
{
    if (flags_raised.contains(flag))
        return;

    std::println("[-] {}", note);
    flags_raised.insert(flag);
}
