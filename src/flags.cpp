#include "flags.hpp"

void raise_flag(flags flag, const char* note)
{
    static std::unordered_set<std::string> seen;

    if (!seen.insert(note).second)
        return;

#ifdef CONSOLE
    std::println("[-] {}", note);
#endif
    gui::print(std::format("[-] {}", note).c_str());

    flags_raised.insert(flag);
}
