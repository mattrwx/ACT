#include "validate_modules.hpp"


void load_file(std::string name, std::unique_ptr<char[]>& buffer, size_t& size)
{
    std::ifstream file(name, std::ios::in | std::ios::binary);
    if (!file.is_open())
    {
        std::println("Failed to open module's file: {}", name);
        cache::flags++;
        return;
    }

    file.seekg(0, std::ios::end);
    size = file.tellg();
    buffer = std::make_unique<char[]>(size);

    file.seekg(std::ios::beg);
    file.read((char*)buffer.get(), size);
}

std::string md5(uintptr_t data, size_t size) {
    BCRYPT_ALG_HANDLE hAlg;
    BCRYPT_HASH_HANDLE hHash;

    BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_MD5_ALGORITHM, nullptr, 0);

    DWORD hashObjSize = 0, bytesWritten = 0;
    BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PBYTE)&hashObjSize, sizeof(DWORD), &bytesWritten, 0);

    auto hashObj = std::make_unique<uint8_t[]>(hashObjSize);

    BCryptCreateHash(hAlg, &hHash, hashObj.get(), hashObjSize, nullptr, 0, 0);
    BCryptHashData(hHash, (PUCHAR)data, (ULONG)size, 0);

    uint8_t raw[16];
    BCryptFinishHash(hHash, raw, sizeof(raw), 0);

    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    char hex[33];
    for (int i{}; i < 16; i++)
        snprintf(hex + i * 2, 3, "%02x", raw[i]);

    return std::string(hex, 32);
}

std::vector<std::string> get_disk_sections(const std::string& path)
{
    std::unique_ptr<char[]> buffer;
    size_t size{};
    load_file(path, buffer, size);

    if (!buffer)
        return {};

    auto* dos     = (IMAGE_DOS_HEADER*)buffer.get();
    auto* nt      = (IMAGE_NT_HEADERS*)(buffer.get() + dos->e_lfanew);
    auto* section = IMAGE_FIRST_SECTION(nt);

    std::vector<std::string> hashes;
    for (WORD i{}; i < nt->FileHeader.NumberOfSections; i++, section++)
    {
        if (!(section->Characteristics & IMAGE_SCN_MEM_EXECUTE))
            continue;

        hashes.push_back(md5((uintptr_t)buffer.get() + section->PointerToRawData, section->SizeOfRawData));
    }

    return hashes;
}

std::vector<std::string> get_module_sections(void* module)
{
    auto base = reinterpret_cast<uintptr_t>(module);

    IMAGE_DOS_HEADER* dos;
    IMAGE_NT_HEADERS* nt;
    utils::populate_pe((HMODULE)module, dos, nt);

    auto* section = IMAGE_FIRST_SECTION(nt);

    std::vector<std::string> hashes;
    for (WORD i{}; i < nt->FileHeader.NumberOfSections; i++, section++)
    {
        if (section->Characteristics & IMAGE_SCN_MEM_WRITE)
            continue;

        hashes.push_back(md5(base + section->VirtualAddress, section->SizeOfRawData));
    }

    return hashes;
}

void validate_modules::hash_module_sections()
{
    PROCESS_BASIC_INFORMATION basic_information;
    NtQueryInformationProcess(GetCurrentProcess(), PROCESSINFOCLASS::ProcessBasicInformation, &basic_information, sizeof(PROCESS_BASIC_INFORMATION), 0);

    LIST_ENTRY* module_list_start = &basic_information.PebBaseAddress->Ldr->InMemoryOrderModuleList;

    for (LIST_ENTRY* current_entry = module_list_start->Flink; current_entry != module_list_start; current_entry = current_entry->Flink)
    {
        auto* ldr_entry = CONTAINING_RECORD(current_entry, LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks);
        auto  module_sections = get_module_sections(ldr_entry->DllBase);

        hashes.insert({ (HMODULE)ldr_entry->DllBase, module_sections });
    }
}

void validate_modules::compare_to_disk()
{
    for (auto& [module, module_sections] : hashes)
    {
        wchar_t path_buf[MAX_PATH];
        GetModuleFileNameW(module, path_buf, MAX_PATH);

        std::string module_path(std::filesystem::path(path_buf).string());
        auto disk_sections = get_disk_sections(module_path);

        if (disk_sections != module_sections)
        {
            std::println("[-] Mismatched Module Sections: {:p}", (void*)module);
            cache::flags++;
        }
    }
}

void validate_modules::compare_module_hashes()
{
    PROCESS_BASIC_INFORMATION basic_information;
    NtQueryInformationProcess(GetCurrentProcess(), PROCESSINFOCLASS::ProcessBasicInformation, &basic_information, sizeof(PROCESS_BASIC_INFORMATION), 0);

    LIST_ENTRY* module_list_start = &basic_information.PebBaseAddress->Ldr->InMemoryOrderModuleList;

    for (LIST_ENTRY* current_entry = module_list_start->Flink; current_entry != module_list_start; current_entry = current_entry->Flink)
    {
        auto* ldr_entry = CONTAINING_RECORD(current_entry, LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks);
        auto  module    = (HMODULE)ldr_entry->DllBase;

        if (!hashes.contains(module))
        {
            wchar_t path_buf[MAX_PATH];
            GetModuleFileNameW(module, path_buf, MAX_PATH);

            if (!utils::verify_trust(path_buf))
            {
                std::println("[-] New module failed trust verification: {}", std::filesystem::path(path_buf).string());
                cache::flags++;
            }

            std::string module_path(std::filesystem::path(path_buf).string());
            std::println("module path: {}", module_path);
            auto disk_sections = get_disk_sections(module_path);
            for (auto section : disk_sections)
                std::println("{}", section);

            std::println("----");

            auto module_sections = get_module_sections(module);
            for (auto section : module_sections)
                std::println("{}", section);

            if (disk_sections != module_sections)
            {
                std::println("[-] New module has mismatched sections: {:X}", (uintptr_t)module);
                cache::flags++;
            }

            hashes.insert({ module, module_sections });
            continue;
        }

        auto current_sections = get_module_sections(module);

        if (current_sections != hashes[module])
        {
            std::println("[-] Module sections changed: {:X}", (uintptr_t)module);
            cache::flags++;
        }
    }
}