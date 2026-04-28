#include "utils.hpp"








void utils::populate_pe(HMODULE h_module, IMAGE_DOS_HEADER*& dos_header, IMAGE_NT_HEADERS*& nt_headers)
{
    dos_header = (IMAGE_DOS_HEADER*)h_module;
    nt_headers = (IMAGE_NT_HEADERS*)((uintptr_t)h_module + dos_header->e_lfanew);

    // Special logic to fill in information about the games pe
    if (h_module != GetModuleHandle(0))
        return;

    if (nt_headers->FileHeader.Machine == IMAGE_FILE_MACHINE_I386)
        cache::process::is_32_bit = true;
    
    else if (nt_headers->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64)
    {
        std::println("[!] Invalid architecture detected!");
        FreeLibraryAndExitThread(cache::local_module::handle, 0);
    }
}