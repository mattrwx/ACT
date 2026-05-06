#include "rtti_validate_all.hpp"
#include <Windows.h>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <winternl.h>
#include "../flags.hpp"


struct rtti_complete_object_locator
{
    uint32_t signature;
    uint32_t offset;
    uint32_t cd_offset;
    uint32_t p_type_descriptor;
    uint32_t p_class_descriptor;
    uint32_t p_self;
};

struct section_range
{
    uintptr_t begin;
    uintptr_t end;
};

struct cached_vtable
{
    uintptr_t begin;
    uintptr_t end;
};

struct cached_module
{
    section_range text;
    section_range rdata;
    std::vector<cached_vtable> vtables;
};

static std::unordered_map<HMODULE, cached_module> vtable_cache;

void rtti::validate_all()
{
    PROCESS_BASIC_INFORMATION pbi{};
    NtQueryInformationProcess(GetCurrentProcess(), ProcessBasicInformation, &pbi, sizeof(pbi), nullptr);

    LIST_ENTRY* list_start = &pbi.PebBaseAddress->Ldr->InMemoryOrderModuleList;

    for (LIST_ENTRY* entry = list_start->Flink; entry != list_start; entry = entry->Flink)
    {
        auto* ldr = CONTAINING_RECORD(entry, LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks);
        auto hmodule = (HMODULE)ldr->DllBase;
        auto pmodule = (uintptr_t)ldr->DllBase;

        // check for cached vtables
        if (auto cached = vtable_cache.find(hmodule); cached != vtable_cache.end())
        {
            for (cached_vtable vtable : cached->second.vtables)
            {
                for (uintptr_t slot_addr = vtable.begin; slot_addr + sizeof(uintptr_t) <= vtable.end; slot_addr += sizeof(uintptr_t))
                {
                    uintptr_t slot = *(uintptr_t*)slot_addr;

                    // lukaszlipski.dev: "The vfptr is a pointer to vftable which contains virtual function pointers."
                    // a slot pointing outside .text is not a legitimate virtual function pointer it is a hook
                    if (!slot || !(slot >= cached->second.text.begin && slot < cached->second.text.end))
                    {
                        size_t index = (slot_addr - vtable.begin) / sizeof(uintptr_t);
                        raise_flag(flags::rtti_vmt_hook, std::format("RTTI VMT hook detected: module=0x{:X} vtable=0x{:X} slot[{}]=0x{:X}", pmodule, vtable.begin, index, slot).c_str());
                    }
                }
            }
            continue;
        }

        // find text and rdata sections
        //  lukaszlipski.dev: "The RTTI data is going to be placed in the .rdata section of the application."
        //  PE headers are always committed — PEB LDR only lists properly mapped modules, safe to cast directly
        auto* nt = (IMAGE_NT_HEADERS*)(pmodule + ((IMAGE_DOS_HEADER*)pmodule)->e_lfanew);
        uintptr_t image_size = nt->OptionalHeader.SizeOfImage;

        auto* section = IMAGE_FIRST_SECTION(nt);
        section_range text{}, rdata{};
        bool found_text = false;
        bool found_rdata = false;

        for (WORD i{}; i < nt->FileHeader.NumberOfSections; i++, section++)
        {
            std::string_view name((char*)section->Name, strnlen((char*)section->Name, IMAGE_SIZEOF_SHORT_NAME));

            if (name == ".text")
            {
                text = {pmodule + section->VirtualAddress, pmodule + section->VirtualAddress + section->Misc.VirtualSize};
                found_text = true;
            }
            else if (name == ".rdata")
            {
                rdata = {pmodule + section->VirtualAddress, pmodule + section->VirtualAddress + section->Misc.VirtualSize};
                found_rdata = true;
            }

            if (found_text && found_rdata)
                break;
        }

        if (!found_text || !found_rdata)
            continue;

        auto& cached = vtable_cache[hmodule];
        cached.text = text;
        cached.rdata = rdata;

        for (uintptr_t p = rdata.begin; p + sizeof(uintptr_t) <= rdata.end; p += sizeof(uintptr_t))
        {
            // find begin of vtable
            //  .rdata is always PAGE_READONLY mapped direct read is safe
            uintptr_t col_candidate = *(uintptr_t*)p;

            // col_candidate should fall within this modules image before we can read it as an rtti_complete_object_locator
            if (col_candidate < pmodule || col_candidate + sizeof(rtti_complete_object_locator) > pmodule + image_size)
                continue;

            // col_candidate is an arbitrary pointer found in memory the target page may be uncommitted
            rtti_complete_object_locator col{};
            SIZE_T read = 0;
            if (!ReadProcessMemory(GetCurrentProcess(), (void*)col_candidate, &col, sizeof(col), &read) || read != sizeof(col))
                continue;

            // lukaszlipski.dev: "signature for x64 is set to COL_SIG_REV1 which means pTypeDescriptor, pClassDescriptor and pSelf are going to be image base relative offsets."
            // COL_SIG_REV1 == 1; any other value means this is not a valid x64 COL or fields are raw pointers (x86 layout)
            if (col.signature != 1)
                continue;

            // lukaszlipski.dev: "pSelf contains the offset from image base to the current RTTICompleteObjectLocator.
            // This gives us a simple way to get the image base which we can use to get pTypeDescriptor and pClassDescriptor."
            // if col_candidate - pmodule != p_self, the memory we read is not actually a COL
            if (col.p_self != (uint32_t)(col_candidate - pmodule))
                continue;

            // lukaszlipski.dev: "pTypeDescriptor contains the offset from the image base to complete the object's TypeDescriptor."
            // an offset >= image_size would land outside the mapped image, so it cannot be a valid TypeDescriptor
            if (col.p_type_descriptor >= image_size)
                continue;

            // lukaszlipski.dev: "pClassDescriptor contains the offset from the image base to RTTIClassHierarchyDescriptor."
            // same bounds check as p_type_descriptor — must resolve to a valid address inside the image
            if (col.p_class_descriptor >= image_size)
                continue;

            cached_vtable vtable{};
            // quarkslab.com: "This structure [RTTICompleteObjectLocator] is located at VFT - sizeof(void*)"
            // therefore VFT (vtable first slot) = address of the pointer that references the COL + sizeof(uintptr_t)
            vtable.begin = p + sizeof(uintptr_t);

            // find end of vtable
            //  vtable slots reside inside .rdata — direct read is safe
            for (vtable.end = vtable.begin; vtable.end + sizeof(uintptr_t) <= rdata.end; vtable.end += sizeof(uintptr_t))
            {
                uintptr_t slot = *(uintptr_t*)vtable.end;

                // lukaszlipski.dev: "The vfptr is a pointer to vftable which contains virtual function pointers."
                // a null slot or a slot outside .text means we have stepped past the last legitimate function pointer — end of vtable
                if (!slot || !(slot >= text.begin && slot < text.end))
                    break;
            }

            if (vtable.begin == vtable.end)
                continue;

            cached.vtables.push_back(vtable);

            // iterate in vtable
            for (uintptr_t slot_addr = vtable.begin; slot_addr + sizeof(uintptr_t) <= vtable.end; slot_addr += sizeof(uintptr_t))
            {
                uintptr_t slot = *(uintptr_t*)slot_addr;

                // lukaszlipski.dev: "The vfptr is a pointer to vftable which contains virtual function pointers."
                // a slot pointing outside .text is not a legitimate virtual function pointer it is a hook
                if (!slot || !(slot >= text.begin && slot < text.end))
                {
                    size_t index = (slot_addr - vtable.begin) / sizeof(uintptr_t);
                    raise_flag(flags::rtti_vmt_hook, std::format("RTTI VMT hook detected: module=0x{:X} vtable=0x{:X} slot[{}]=0x{:X}", pmodule, vtable.begin, index, slot).c_str());
                }
            }
        }
    }
}
