#pragma once
#include <d3d11.h>
#include <dxgi.h>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <windows.h>
#include "../imgui/backends/imgui_impl_dx11.h"
#include "../imgui/backends/imgui_impl_win32.h"
#include "../imgui/imgui.h"

namespace gui
{
    void push_log(std::string msg);
    void init();
    void render();
    void shutdown();
    void print(const char* msg);
    bool alive();
}

inline void render_thread()
{
    gui::init();

    while (gui::alive())
    {
        gui::render();
        Sleep(1);
    }
}