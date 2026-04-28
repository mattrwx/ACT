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

bool utils::is_valid_code_region(void* address)
{
    PROCESS_BASIC_INFORMATION basic_information;
    NtQueryInformationProcess(GetCurrentProcess(), PROCESSINFOCLASS::ProcessBasicInformation, &basic_information, sizeof(PROCESS_BASIC_INFORMATION), 0);

    LIST_ENTRY* module_list_start = &basic_information.PebBaseAddress->Ldr->InMemoryOrderModuleList;

    for (LIST_ENTRY* current_entry = module_list_start->Flink; current_entry != module_list_start; current_entry = current_entry->Flink)
    {
        auto module_base = reinterpret_cast<std::uintptr_t>(CONTAINING_RECORD(current_entry, LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks)->DllBase);

        IMAGE_DOS_HEADER* dos_header;
        IMAGE_NT_HEADERS* nt_headers;

        populate_pe((HMODULE)module_base, dos_header, nt_headers);

        auto section = IMAGE_FIRST_SECTION(nt_headers);

        for (WORD i{}; i < nt_headers->FileHeader.NumberOfSections; i++, section++)
        {
            if (!(section->Characteristics & IMAGE_SCN_MEM_EXECUTE))
                continue;

            auto section_start = module_base + section->VirtualAddress;
            auto section_end = section_start + section->Misc.VirtualSize;
            auto target  = (uintptr_t)address;

            if (target >= section_start && target < section_end)
                return true;
        }
    }

    return false;
}