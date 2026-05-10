#include <Windows.h>
#include <optional>
#include <format>
#include "../utils.hpp"
#include "../flags.hpp"
#include "../gui/gui.hpp"
#include "internal/validate_modules.hpp"

namespace hooks
{   
    namespace
    {
        template <typename T>
        class push_ret;
    }

    template <typename Ret, typename... Args>
    class push_ret<Ret(*)(Args...)>
    {
        void* target;
        void* detour;

        unsigned char shellcode[12]{ 0x48, 0xA1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x50, 0xC3 };
        unsigned char original[12];
        
        void protected_write(void* destination, void* source)
        {
            DWORD old;
            VirtualProtect(destination, 12, PAGE_EXECUTE_READWRITE, &old);
            std::memcpy(destination, source, 12);
            VirtualProtect(destination, 12, old, &old);
            
            modules::rehash_containing_section(destination);
        }

        public:
        void hook()
        {
            protected_write(target, shellcode);
        }

        void unhook()
        {
            protected_write(target, original);
        }

        push_ret(void* target, void* detour)
        {
            this->target = target;
            this->detour = detour;

            *(void**)(shellcode+2) = detour;
            std::memcpy(original, target, 12);
            
            hook();
        }

        Ret call_original(Args... args)
        {
            unhook();
            struct rehook_guard { push_ret* self; ~rehook_guard() { self->hook(); } } guard{this};

            if constexpr (std::is_void_v<Ret>)
                ((Ret(*)(Args...))target)(std::forward<Args>(args)...);
            else
                return ((Ret(*)(Args...))target)(std::forward<Args>(args)...);
        }
    };

    void place_input_hooks();
}