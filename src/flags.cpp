#include "flags.hpp"

void raise_flag(flags flag, const char* note)
{
    if (!logs.insert(note).second)
        return;

    log_flag_map[note] = flag;

#ifdef CONSOLE
    std::println("[-] {}", note);
#endif
    gui::print(std::format("[-] {}", note).c_str());

    flags_raised.insert(flag);
}