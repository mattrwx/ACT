#include "validate_modules.hpp"

static void load_file(const std::string& path, std::unique_ptr<char[]>& buffer, size_t& size)
{
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file.is_open())
    {
        std::println("[-] Failed to open module file: {}", path);
        flags_raised.insert(flags::failed_to_open_module_file);
        return;
    }

    file.seekg(0, std::ios::end);
    size = file.tellg();
    buffer = std::make_unique<char[]>(size);
    file.seekg(std::ios::beg);
    file.read(buffer.get(), size);
}

static std::string md5(uintptr_t data, size_t size)
{
    BCRYPT_ALG_HANDLE hAlg{};
    BCRYPT_HASH_HANDLE hHash{};

    BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_MD5_ALGORITHM, nullptr, 0);

    DWORD hashObjSize{}, bytesWritten{};
    BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PBYTE)&hashObjSize, sizeof(DWORD), &bytesWritten, 0);

    auto hashObj = std::make_unique<uint8_t[]>(hashObjSize);

    BCryptCreateHash(hAlg, &hHash, hashObj.get(), hashObjSize, nullptr, 0, 0);
    BCryptHashData(hHash, (PUCHAR)data, (ULONG)size, 0);

    uint8_t raw[16]{};
    BCryptFinishHash(hHash, raw, sizeof(raw), 0);

    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    char hex[33]{};
    for (int i{}; i < 16; i++)
        snprintf(hex + i * 2, 3, "%02x", raw[i]);

    return std::string(hex, 32);
}

static bool skip_section(IMAGE_NT_HEADERS* nt, IMAGE_SECTION_HEADER* section)
{
    std::string_view loader_patched_sections[]{
            "fothk", ".00cfg", ".detourc", ".mrdata", ".rdata", "PAGECONS",
    };

    // name-based exclusions
    std::string_view name((char*)section->Name, strnlen((char*)section->Name, IMAGE_SIZEOF_SHORT_NAME));
    for (auto& excluded : loader_patched_sections)
        if (name == excluded)
            return true;

    // data directory exclusions
    for (int dir : {IMAGE_DIRECTORY_ENTRY_IAT, IMAGE_DIRECTORY_ENTRY_BASERELOC, IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT})
    {
        auto& entry = nt->OptionalHeader.DataDirectory[dir];
        if (!entry.VirtualAddress || !entry.Size)
            continue;

        DWORD sec_start = section->VirtualAddress;
        DWORD sec_end = sec_start + section->Misc.VirtualSize;
        DWORD dir_start = entry.VirtualAddress;
        DWORD dir_end = dir_start + entry.Size;

        if (sec_start < dir_end && sec_end > dir_start)
            return true;
    }

    return false;
}

static std::vector<section_t> get_disk_sections(const std::string& path)
{
    std::unique_ptr<char[]> buffer;
    size_t size{};
    load_file(path, buffer, size);

    if (!buffer)
        return {};

    auto* dos = (IMAGE_DOS_HEADER*)buffer.get();
    auto* nt = (IMAGE_NT_HEADERS*)(buffer.get() + dos->e_lfanew);
    auto* section = IMAGE_FIRST_SECTION(nt);

    std::vector<section_t> sections;
    for (WORD i{}; i < nt->FileHeader.NumberOfSections; i++, section++)
    {
        if (section->Characteristics & IMAGE_SCN_MEM_WRITE)
            continue;

        if (skip_section(nt, section))
            continue;

        sections.push_back(
                {.name = std::string((char*)section->Name, strnlen((char*)section->Name, IMAGE_SIZEOF_SHORT_NAME)),
                 .hash = md5((uintptr_t)buffer.get() + section->PointerToRawData, section->SizeOfRawData),
                 .protection = 0}
        );
    }

    return sections;
}

static std::vector<section_t> get_memory_sections(HMODULE module)
{
    auto base = reinterpret_cast<uintptr_t>(module);

    IMAGE_DOS_HEADER* dos{};
    IMAGE_NT_HEADERS* nt{};
    utils::populate_pe(module, dos, nt);

    auto* section = IMAGE_FIRST_SECTION(nt);

    std::vector<section_t> sections;
    for (WORD i{}; i < nt->FileHeader.NumberOfSections; i++, section++)
    {
        if (section->Characteristics & IMAGE_SCN_MEM_WRITE)
            continue;

        if (skip_section(nt, section))
            continue;

        MEMORY_BASIC_INFORMATION mbi{};
        VirtualQuery((LPCVOID)(base + section->VirtualAddress), &mbi, sizeof(mbi));

        sections.push_back(
                {.name = std::string((char*)section->Name, strnlen((char*)section->Name, IMAGE_SIZEOF_SHORT_NAME)),
                 .hash = md5(base + section->VirtualAddress, section->SizeOfRawData),
                 .protection = mbi.Protect}
        );
    }

    return sections;
}

static void register_module(HMODULE module)
{
    wchar_t path_buf[MAX_PATH]{};
    GetModuleFileNameW(module, path_buf, MAX_PATH);
    std::string module_path = std::filesystem::path(path_buf).string();

    IMAGE_DOS_HEADER* dos{};
    IMAGE_NT_HEADERS* nt{};
    utils::populate_pe(module, dos, nt);

    auto disk_sections = get_disk_sections(module_path);
    auto memory_sections = get_memory_sections(module);

    if (disk_sections.size() != memory_sections.size())
    {
        std::println("[-] Section count mismatch on register: {}", module_path);
        flags_raised.insert(flags::section_count_changed);
    }
    else
    {
        for (size_t i{}; i < disk_sections.size(); i++)
        {
            if (disk_sections[i].hash != memory_sections[i].hash)
            {
                std::println("[-] Section hash mismatch on register: {} in {}", disk_sections[i].name, module_path);
                flags_raised.insert(flags::section_hash_changed);
            }
        }
    }

    modules::module_map.insert_or_assign(
            module, module_t{
                            .name = std::filesystem::path(module_path).filename().string(),
                            .base = reinterpret_cast<uintptr_t>(module),
                            .size = nt->OptionalHeader.SizeOfImage,
                            .dos = dos,
                            .nt = nt,
                            .sections = std::move(memory_sections)
                    }
    );
}

void modules::init()
{
    PROCESS_BASIC_INFORMATION pbi{};
    NtQueryInformationProcess(GetCurrentProcess(), ProcessBasicInformation, &pbi, sizeof(pbi), nullptr);

    LIST_ENTRY* list_start = &pbi.PebBaseAddress->Ldr->InMemoryOrderModuleList;

    for (LIST_ENTRY* entry = list_start->Flink; entry != list_start; entry = entry->Flink)
    {
        auto* ldr = CONTAINING_RECORD(entry, LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks);
        auto hmod = (HMODULE)ldr->DllBase;
        register_module(hmod);
    }
}

void modules::validate()
{
    PROCESS_BASIC_INFORMATION pbi{};
    NtQueryInformationProcess(GetCurrentProcess(), ProcessBasicInformation, &pbi, sizeof(pbi), nullptr);

    LIST_ENTRY* list_start = &pbi.PebBaseAddress->Ldr->InMemoryOrderModuleList;

    for (LIST_ENTRY* entry = list_start->Flink; entry != list_start; entry = entry->Flink)
    {
        auto* ldr = CONTAINING_RECORD(entry, LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks);
        auto hmod = (HMODULE)ldr->DllBase;

        if (!module_map.contains(hmod))
        {
            wchar_t path_buf[MAX_PATH]{};
            GetModuleFileNameW(hmod, path_buf, MAX_PATH);

            if (!utils::verify_trust(path_buf))
            {
                std::println("[-] New module failed trust verification: {}", std::filesystem::path(path_buf).string());
                flags_raised.insert(flags::new_module_trust_verification);
            }

            register_module(hmod);
            continue;
        }

        auto& stored = module_map[hmod];
        auto current = get_memory_sections(hmod);

        if (current.size() != stored.sections.size())
        {
            std::println("[-] Section count changed: {:X}", (uintptr_t)hmod);
            flags_raised.insert(flags::section_count_changed);
            continue;
        }

        for (size_t i{}; i < stored.sections.size(); i++)
        {
            // Hash check
            if (current[i].hash != stored.sections[i].hash)
            {
                std::println("[-] Hash changed: {} in {:X}", stored.sections[i].name, (uintptr_t)hmod);
                flags_raised.insert(flags::section_hash_changed);
            }

            // Page protection check
            if (current[i].protection != stored.sections[i].protection)
            {
                std::println("[-] Protection changed: {} in {:X}  ({:08X} -> {:08X})", stored.sections[i].name, (uintptr_t)hmod, stored.sections[i].protection, current[i].protection);
                flags_raised.insert(flags::section_protection_changed);
            }
        }
    }
}
