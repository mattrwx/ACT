#include "overlay.hpp"
#include <print>
#include <string>
#include <windows.h>
#include "flags.hpp"
#include "utils.hpp"


BOOL __stdcall overlay_callback(HWND hwnd, LPARAM)
{
    LONG_PTR window_flags = GetWindowLongA(hwnd, GWL_EXSTYLE);
    bool topmost = window_flags & WS_EX_TOPMOST;
    bool transparent = window_flags & WS_EX_TRANSPARENT;
    bool layered = window_flags & WS_EX_LAYERED;

    if (!(topmost && transparent && layered))
        return TRUE;

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);

    HANDLE h_process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h_process)
    {
        raise_flag(flags::overlay_handle_creation_failed, "overlay_handle_creation_failed");
        return TRUE;
    }

    wchar_t path[MAX_PATH];
    if (!GetModuleFileNameExW(h_process, NULL, path, MAX_PATH))
    {
        raise_flag(flags::failed_to_get_overlay_module, "failed_to_get_overlay_module");
        CloseHandle(h_process);
        return TRUE;
    }

    if (utils::verify_trust(path))
    {
        CloseHandle(h_process);
        return TRUE;
    }


    BOOL removeflag = SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    if (!removeflag)
    {
        raise_flag(flags::failed_to_remove_topmost_flag, "failed_to_remove_topmost_flag");
        CloseHandle(h_process);
        return TRUE;
    }
    CloseHandle(h_process);

    return TRUE;
}

void overlay::detect_overlay_window()
{
    EnumWindows(overlay_callback, 0);
}
