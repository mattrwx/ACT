#include "gui.hpp"
#include "../flags.hpp"
#include "font.hpp"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

static ID3D11Device* s_device = nullptr;
static ID3D11DeviceContext* s_ctx = nullptr;
static IDXGISwapChain* s_swap = nullptr;
static ID3D11RenderTargetView* s_rtv = nullptr;
static HWND s_hwnd = nullptr;

static std::vector<std::string> s_log;
static bool s_scroll = false;
static bool s_running = true;

static constexpr int WIN_W = 500;
static constexpr int WIN_H = 700;
static constexpr float TITLEBAR_H = 28.0f;

static const std::unordered_map<flags, const char*> FLAG_DESCRIPTIONS = {
        {flags::failed_to_remove_topmost_flag,            "Failed to remove topmost flag from overlay window"   },
        {flags::failed_to_get_overlay_module,             "Failed to retrieve module path for overlay process"  },
        {flags::overlay_handle_creation_failed,           "Failed to open handle to overlay process"            },
        {flags::failed_to_open_threads_snapshot,          "Failed to create thread snapshot"                    },
        {flags::failed_to_open_first_thread,              "Failed to open first thread from snapshot"           },
        {flags::thread_handle_failed_to_open,             "Failed to open thread handle"                        },
        {flags::NtQueryInformationThread_failure,         "NtQueryInformationThread call failed"                },
        {flags::invalid_thread_start_address,             "Thread start address outside valid module range"     },
        {flags::exception_tampering,                      "Exception handler chain has been tampered"           },
        {flags::Wow64PrepareForExecution_hook,            "Hook detected on Wow64PrepareForExecution"           },
        {flags::failed_to_find_KiUserExceptionDispatcher, "Failed to locate KiUserExceptionDispatcher"          },
        {flags::failed_to_find_ntdll,                     "Failed to locate ntdll"                              },
        {flags::invalid_rip_during_exception,             "RIP outside valid executable region during exception"},
        {flags::invalid_executable_page,                  "Executable page found outside known modules"         },
        {flags::new_module_trust_verification,            "Untrusted module loaded into process"                },
        {flags::section_count_changed,                    "PE section count mismatch detected"                  },
        {flags::section_hash_changed,                     "PE section hash mismatch detected"                   },
        {flags::section_protection_changed,               "PE section memory protection modified"               },
        {flags::failed_to_open_module_file,               "Failed to open module file on disk"                  },
        {flags::vmt_hook,                                 "VTable hook detected!"                               }
};

static void create_rtv()
{
    ID3D11Texture2D* bb = nullptr;
    s_swap->GetBuffer(0, IID_PPV_ARGS(&bb));
    if (bb)
    {
        s_device->CreateRenderTargetView(bb, nullptr, &s_rtv);
        bb->Release();
    }
}

static void cleanup_rtv()
{
    if (s_rtv)
    {
        s_rtv->Release();
        s_rtv = nullptr;
    }
}

static bool create_d3d(HWND hwnd)
{
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate = {60, 1};
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    D3D_FEATURE_LEVEL fl;

    if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 2, D3D11_SDK_VERSION, &sd, &s_swap, &s_device, &fl, &s_ctx)))
        return false;

    create_rtv();
    return true;
}

static void cleanup_d3d()
{
    cleanup_rtv();
    if (s_swap)
    {
        s_swap->Release();
        s_swap = nullptr;
    }
    if (s_ctx)
    {
        s_ctx->Release();
        s_ctx = nullptr;
    }
    if (s_device)
    {
        s_device->Release();
        s_device = nullptr;
    }
}

static LRESULT WINAPI wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp))
        return true;

    switch (msg)
    {
        case WM_SIZE:
            if (s_device && wp != SIZE_MINIMIZED)
            {
                cleanup_rtv();
                s_swap->ResizeBuffers(0, LOWORD(lp), HIWORD(lp), DXGI_FORMAT_UNKNOWN, 0);
                create_rtv();
            }
            return 0;
        case WM_SYSCOMMAND:
            if ((wp & 0xfff0) == SC_KEYMENU)
                return 0;
            break;
        case WM_DESTROY:
            ::PostQuitMessage(0);
            return 0;
    }

    return ::DefWindowProcW(hwnd, msg, wp, lp);
}

static void apply_theme()
{
    ImGui::StyleColorsDark();
    ImGuiStyle& s = ImGui::GetStyle();

    s.WindowRounding = 0.0f;
    s.ChildRounding = 4.0f;
    s.FrameRounding = 3.0f;
    s.ScrollbarRounding = 3.0f;
    s.GrabRounding = 3.0f;
    s.WindowBorderSize = 0.0f;
    s.ChildBorderSize = 0.0f;
    s.FrameBorderSize = 0.0f;
    s.WindowPadding = {10.0f, 8.0f};
    s.FramePadding = {8.0f, 4.0f};
    s.ItemSpacing = {8.0f, 5.0f};
    s.ScrollbarSize = 7.0f;

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg] = {0.06f, 0.06f, 0.09f, 1.00f};
    c[ImGuiCol_ChildBg] = {0.04f, 0.04f, 0.07f, 1.00f};
    c[ImGuiCol_Border] = {0.18f, 0.18f, 0.28f, 0.50f};
    c[ImGuiCol_FrameBg] = {0.10f, 0.10f, 0.16f, 1.00f};
    c[ImGuiCol_FrameBgHovered] = {0.14f, 0.14f, 0.22f, 1.00f};
    c[ImGuiCol_FrameBgActive] = {0.18f, 0.18f, 0.28f, 1.00f};
    c[ImGuiCol_Text] = {0.86f, 0.86f, 0.90f, 1.00f};
    c[ImGuiCol_TextDisabled] = {0.36f, 0.36f, 0.48f, 1.00f};
    c[ImGuiCol_ScrollbarBg] = {0.04f, 0.04f, 0.07f, 1.00f};
    c[ImGuiCol_ScrollbarGrab] = {0.18f, 0.20f, 0.34f, 1.00f};
    c[ImGuiCol_ScrollbarGrabHovered] = {0.26f, 0.28f, 0.46f, 1.00f};
    c[ImGuiCol_ScrollbarGrabActive] = {0.34f, 0.36f, 0.58f, 1.00f};
    c[ImGuiCol_Separator] = {0.14f, 0.14f, 0.22f, 1.00f};
    c[ImGuiCol_Header] = {0.14f, 0.16f, 0.28f, 1.00f};
    c[ImGuiCol_HeaderHovered] = {0.20f, 0.22f, 0.38f, 1.00f};
    c[ImGuiCol_HeaderActive] = {0.26f, 0.28f, 0.48f, 1.00f};
    c[ImGuiCol_Button] = {0.13f, 0.14f, 0.24f, 1.00f};
    c[ImGuiCol_ButtonHovered] = {0.20f, 0.22f, 0.38f, 1.00f};
    c[ImGuiCol_ButtonActive] = {0.28f, 0.30f, 0.52f, 1.00f};
    c[ImGuiCol_CheckMark] = {0.46f, 0.56f, 1.00f, 1.00f};
    c[ImGuiCol_SliderGrab] = {0.36f, 0.46f, 0.86f, 1.00f};
    c[ImGuiCol_SliderGrabActive] = {0.46f, 0.56f, 1.00f, 1.00f};
    c[ImGuiCol_TitleBg] = {0.07f, 0.07f, 0.11f, 1.00f};
    c[ImGuiCol_TitleBgActive] = {0.09f, 0.10f, 0.20f, 1.00f};
}

void gui::push_log(std::string msg)
{
    s_log.push_back(std::move(msg));
    s_scroll = true;
}

void gui::print(const char* msg)
{
    push_log(msg);
}

bool gui::alive()
{
    return s_running;
}

void gui::shutdown()
{
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    cleanup_d3d();
}

void gui::init()
{
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = GetModuleHandle(0);
    wc.lpszClassName = L"act_wnd";
    ::RegisterClassExW(&wc);

    const int sx = GetSystemMetrics(SM_CXSCREEN);
    const int sy = GetSystemMetrics(SM_CYSCREEN);

    s_hwnd = ::CreateWindowExW(WS_EX_TOPMOST, wc.lpszClassName, L"ACT", WS_POPUP, (sx - WIN_W) / 2, (sy - WIN_H) / 2, WIN_W, WIN_H, nullptr, nullptr, wc.hInstance, nullptr);

    if (!create_d3d(s_hwnd))
    {
        cleanup_d3d();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        MessageBoxW(nullptr, L"D3D11 device creation failed.", L"ACT", MB_ICONERROR);
        std::exit(1);
    }

    ::ShowWindow(s_hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(s_hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    apply_theme();

    ImGui_ImplWin32_Init(s_hwnd);
    ImGui_ImplDX11_Init(s_device, s_ctx);

    io.Fonts->AddFontFromMemoryTTF(Inter, sizeof(Inter), 13.0f);
}

void gui::render()
{
    static POINT drag_origin_cur;
    static RECT drag_origin_rect;

    constexpr ImU32 COL_TITLEBAR = IM_COL32(12, 12, 20, 255);
    constexpr ImU32 COL_TITLE_TEXT = IM_COL32(190, 190, 215, 255);
    constexpr ImU32 COL_DIVIDER = IM_COL32(28, 28, 46, 255);
    constexpr ImU32 COL_CLOSE_BG = IM_COL32(160, 40, 40, 255);
    constexpr ImU32 COL_CLOSE_ICON = IM_COL32(255, 255, 255, 255);
    constexpr ImU32 COL_CLOSE_IDLE = IM_COL32(110, 110, 140, 255);
    constexpr ImU32 COL_FLAG_MARKER = IM_COL32(210, 55, 55, 255);
    constexpr ImU32 COL_SECTION_LABEL = IM_COL32(100, 100, 140, 255);

    MSG msg = {};
    while (::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
        if (msg.message == WM_QUIT)
        {
            s_running = false;
            return;
        }
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float content_h = display.y - TITLEBAR_H;
    const float half_h = content_h * 0.5f;

    ImGui::SetNextWindowPos({0.0f, 0.0f});
    ImGui::SetNextWindowSize(display);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0f, 0.0f});
    ImGui::Begin(
            "##root", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                    ImGuiWindowFlags_NoBringToFrontOnFocus
    );
    ImGui::PopStyleVar();

    ImDrawList* dl = ImGui::GetWindowDrawList();

    dl->AddRectFilled({0.0f, 0.0f}, {display.x, TITLEBAR_H}, COL_TITLEBAR);
    dl->AddLine({0.0f, TITLEBAR_H}, {display.x, TITLEBAR_H}, COL_DIVIDER);

    const float text_y = (TITLEBAR_H - ImGui::GetTextLineHeight()) * 0.5f;
    dl->AddText({10.0f, text_y}, COL_TITLE_TEXT, "ACT");

    ImGui::SetCursorPos({0.0f, 0.0f});
    ImGui::InvisibleButton("##drag", {display.x - TITLEBAR_H, TITLEBAR_H});
    if (ImGui::IsItemActive())
    {
        POINT cur;
        GetCursorPos(&cur);
        if (ImGui::IsItemActivated())
        {
            drag_origin_cur = cur;
            GetWindowRect(s_hwnd, &drag_origin_rect);
        }
        SetWindowPos(s_hwnd, nullptr, drag_origin_rect.left + (cur.x - drag_origin_cur.x), drag_origin_rect.top + (cur.y - drag_origin_cur.y), 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    }

    const ImVec2 close_tl = {display.x - TITLEBAR_H, 0.0f};
    const ImVec2 close_br = {display.x, TITLEBAR_H};

    ImGui::SetCursorPos({close_tl.x, close_tl.y});
    ImGui::InvisibleButton("##close", {TITLEBAR_H, TITLEBAR_H});
    const bool close_hov = ImGui::IsItemHovered();
    if (close_hov)
        dl->AddRectFilled(close_tl, close_br, COL_CLOSE_BG);

    {
        const float cx = close_tl.x + TITLEBAR_H * 0.5f;
        const float cy = TITLEBAR_H * 0.5f;
        const float r = 4.5f;
        const ImU32 col = close_hov ? COL_CLOSE_ICON : COL_CLOSE_IDLE;
        dl->AddLine({cx - r, cy - r}, {cx + r, cy + r}, col, 1.5f);
        dl->AddLine({cx + r, cy - r}, {cx - r, cy + r}, col, 1.5f);
    }

    if (ImGui::IsItemClicked())
    {
        s_running = false;
        ::PostQuitMessage(0);
    }

    ImGui::SetCursorPos({0.0f, TITLEBAR_H});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {8.0f, 8.0f});
    ImGui::BeginChild("##content", {display.x, content_h}, false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();

    const float inner_w = ImGui::GetContentRegionAvail().x;
    const float label_h = ImGui::GetTextLineHeight() + 6.0f;

    {
        ImDrawList* cdl = ImGui::GetWindowDrawList();
        const ImVec2 label_pos = ImGui::GetCursorScreenPos();
        cdl->AddText(label_pos, COL_SECTION_LABEL, "DETECTIONS");
        ImGui::Dummy({0.0f, label_h});

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.03f, 0.03f, 0.06f, 1.0f));
        ImGui::BeginChild("##flags", {inner_w, half_h - label_h - 4.0f}, false);

        if (flags_raised.empty())
        {
            ImGui::SetCursorPos({ImGui::GetCursorPosX() + 4.0f, ImGui::GetCursorPosY() + 6.0f});
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.30f, 0.30f, 0.42f, 1.0f));
            ImGui::TextUnformatted("No detections.");
            ImGui::PopStyleColor();
        }
        else
        {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {4.0f, 6.0f});
            for (const auto& flag : flags_raised)
            {
                const ImVec2 p = ImGui::GetCursorScreenPos();
                const float lh = ImGui::GetTextLineHeight();
                ImGui::GetWindowDrawList()->AddCircleFilled({p.x + 5.0f, p.y + lh * 0.5f}, 3.5f, COL_FLAG_MARKER);
                ImGui::SetCursorScreenPos({p.x + 14.0f, p.y});

                const auto it = FLAG_DESCRIPTIONS.find(flag);
                ImGui::TextUnformatted(it != FLAG_DESCRIPTIONS.end() ? it->second : "Unknown detection");
            }
            ImGui::PopStyleVar();
        }

        ImGui::EndChild();
        ImGui::PopStyleColor();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    {
        ImDrawList* cdl = ImGui::GetWindowDrawList();
        const ImVec2 label_pos = ImGui::GetCursorScreenPos();
        cdl->AddText(label_pos, COL_SECTION_LABEL, "CONSOLE");
        ImGui::Dummy({0.0f, label_h});

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.03f, 0.03f, 0.06f, 1.0f));
        ImGui::BeginChild("##log", {inner_w, 0.0f}, false, ImGuiWindowFlags_HorizontalScrollbar);

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {4.0f, 3.0f});

        constexpr float BTN_R      = 4.0f;
        constexpr float BTN_W      = (BTN_R + 2.0f) * 2.0f;
        constexpr ImU32 COL_RM     = IM_COL32(160, 40, 40, 210);
        constexpr ImU32 COL_RM_HOV = IM_COL32(220, 55, 55, 255);

        int remove_idx = -1;

        for (int i = 0; i < static_cast<int>(s_log.size()); i++)
        {
            ImGui::PushID(i);

            const ImVec2 p  = ImGui::GetCursorScreenPos();
            const float  lh = ImGui::GetTextLineHeight();
            const float  cx = p.x + BTN_R + 2.0f;
            const float  cy = p.y + lh * 0.5f;

            ImGui::InvisibleButton("##rm", {BTN_W, lh});
            const bool hov = ImGui::IsItemHovered();
            ImGui::GetWindowDrawList()->AddCircleFilled({cx, cy}, BTN_R, hov ? COL_RM_HOV : COL_RM);
            if (ImGui::IsItemClicked())
                remove_idx = i;

            ImGui::SameLine();
            ImGui::TextUnformatted(s_log[i].c_str());

            ImGui::PopID();
        }

        ImGui::PopStyleVar();

        if (remove_idx >= 0)
        {
            s_log.erase(s_log.begin() + remove_idx);

            flags_raised.clear();
            for (const auto& line : s_log)
            {
                constexpr std::string_view PREFIX = "[-] ";
                const std::string note = line.size() > PREFIX.size()
                                       ? line.substr(PREFIX.size())
                                       : line;
                const auto it = log_flag_map.find(note);
                if (it != log_flag_map.end())
                    flags_raised.insert(it->second);
            }
        }

        if (s_scroll)
        {
            ImGui::SetScrollHereY(1.0f);
            s_scroll = false;
        }

        ImGui::EndChild();
        ImGui::PopStyleColor();
    }

    ImGui::EndChild();
    ImGui::End();

    ImGui::Render();
    constexpr float bg[4] = {0.06f, 0.06f, 0.09f, 1.0f};
    s_ctx->OMSetRenderTargets(1, &s_rtv, nullptr);
    s_ctx->ClearRenderTargetView(s_rtv, bg);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    s_swap->Present(1, 0);
}
