#include "gui.hpp"

#include <tlhelp32.h>
#include <shellapi.h>
#include <d3d11.h>
#include <string>
#include <vector>
#include <algorithm>

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

static ID3D11Device*           s_device  = nullptr;
static ID3D11DeviceContext*    s_ctx     = nullptr;
static IDXGISwapChain*         s_swap    = nullptr;
static ID3D11RenderTargetView* s_rtv     = nullptr;
static HWND                    s_hwnd    = nullptr;
static bool                    s_running = true;

static constexpr int   WIN_W      = 500;
static constexpr int   WIN_H      = 600;
static constexpr float TITLEBAR_H = 28.0f;

struct ProcessEntry
{
    DWORD       pid;
    std::string name;
};

static std::vector<ProcessEntry> s_procs;
static char                      s_filter[128] = {};
static std::string               s_status;
static float                     s_status_timer = 0.0f;
static bool                      s_status_ok    = true;

static LARGE_INTEGER s_freq;
static LARGE_INTEGER s_prev;

static void create_rtv()
{
    ID3D11Texture2D* bb = nullptr;
    s_swap->GetBuffer(0, IID_PPV_ARGS(&bb));
    if (bb) { s_device->CreateRenderTargetView(bb, nullptr, &s_rtv); bb->Release(); }
}

static void cleanup_rtv()
{
    if (s_rtv) { s_rtv->Release(); s_rtv = nullptr; }
}

static bool create_d3d(HWND hwnd)
{
    DXGI_SWAP_CHAIN_DESC sd     = {};
    sd.BufferCount              = 2;
    sd.BufferDesc.Format        = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate   = {60, 1};
    sd.Flags                    = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage              = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow             = hwnd;
    sd.SampleDesc.Count         = 1;
    sd.Windowed                 = TRUE;
    sd.SwapEffect               = DXGI_SWAP_EFFECT_DISCARD;

    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    D3D_FEATURE_LEVEL fl;

    if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                                             levels, 2, D3D11_SDK_VERSION,
                                             &sd, &s_swap, &s_device, &fl, &s_ctx)))
        return false;

    create_rtv();
    return true;
}

static void cleanup_d3d()
{
    cleanup_rtv();
    if (s_swap)   { s_swap->Release();   s_swap   = nullptr; }
    if (s_ctx)    { s_ctx->Release();    s_ctx    = nullptr; }
    if (s_device) { s_device->Release(); s_device = nullptr; }
}

static void request_elevation()
{
    HANDLE token = nullptr;
    OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token);

    TOKEN_ELEVATION elev = {};
    DWORD sz = sizeof(elev);
    GetTokenInformation(token, TokenElevation, &elev, sz, &sz);
    CloseHandle(token);

    if (elev.TokenIsElevated)
        return;

    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);

    SHELLEXECUTEINFOW sei = {};
    sei.cbSize       = sizeof(sei);
    sei.lpVerb       = L"runas";
    sei.lpFile       = path;
    sei.nShow        = SW_SHOWNORMAL;

    ShellExecuteExW(&sei);
    ExitProcess(0);
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
            if ((wp & 0xfff0) == SC_KEYMENU) return 0;
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

    s.WindowRounding    = 0.0f;
    s.ChildRounding     = 4.0f;
    s.FrameRounding     = 3.0f;
    s.ScrollbarRounding = 3.0f;
    s.GrabRounding      = 3.0f;
    s.WindowBorderSize  = 0.0f;
    s.ChildBorderSize   = 0.0f;
    s.FrameBorderSize   = 0.0f;
    s.WindowPadding     = {10.0f, 8.0f};
    s.FramePadding      = {8.0f, 4.0f};
    s.ItemSpacing       = {8.0f, 5.0f};
    s.ScrollbarSize     = 7.0f;

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]             = {0.06f, 0.06f, 0.09f, 1.00f};
    c[ImGuiCol_ChildBg]              = {0.04f, 0.04f, 0.07f, 1.00f};
    c[ImGuiCol_Border]               = {0.18f, 0.18f, 0.28f, 0.50f};
    c[ImGuiCol_FrameBg]              = {0.10f, 0.10f, 0.16f, 1.00f};
    c[ImGuiCol_FrameBgHovered]       = {0.14f, 0.14f, 0.22f, 1.00f};
    c[ImGuiCol_FrameBgActive]        = {0.18f, 0.18f, 0.28f, 1.00f};
    c[ImGuiCol_Text]                 = {0.86f, 0.86f, 0.90f, 1.00f};
    c[ImGuiCol_TextDisabled]         = {0.36f, 0.36f, 0.48f, 1.00f};
    c[ImGuiCol_ScrollbarBg]          = {0.04f, 0.04f, 0.07f, 1.00f};
    c[ImGuiCol_ScrollbarGrab]        = {0.18f, 0.20f, 0.34f, 1.00f};
    c[ImGuiCol_ScrollbarGrabHovered] = {0.26f, 0.28f, 0.46f, 1.00f};
    c[ImGuiCol_ScrollbarGrabActive]  = {0.34f, 0.36f, 0.58f, 1.00f};
    c[ImGuiCol_Separator]            = {0.14f, 0.14f, 0.22f, 1.00f};
    c[ImGuiCol_Header]               = {0.14f, 0.16f, 0.28f, 1.00f};
    c[ImGuiCol_HeaderHovered]        = {0.20f, 0.22f, 0.38f, 1.00f};
    c[ImGuiCol_HeaderActive]         = {0.26f, 0.28f, 0.48f, 1.00f};
    c[ImGuiCol_Button]               = {0.13f, 0.14f, 0.24f, 1.00f};
    c[ImGuiCol_ButtonHovered]        = {0.20f, 0.22f, 0.38f, 1.00f};
    c[ImGuiCol_ButtonActive]         = {0.28f, 0.30f, 0.52f, 1.00f};
    c[ImGuiCol_CheckMark]            = {0.46f, 0.56f, 1.00f, 1.00f};
    c[ImGuiCol_TitleBg]              = {0.07f, 0.07f, 0.11f, 1.00f};
    c[ImGuiCol_TitleBgActive]        = {0.09f, 0.10f, 0.20f, 1.00f};
}

static bool is_x64_process(HANDLE proc)
{
    BOOL wow64 = FALSE;
    IsWow64Process(proc, &wow64);
    return !wow64;
}

static void refresh_processes()
{
    s_procs.clear();

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return;

    PROCESSENTRY32W pe = {};
    pe.dwSize = sizeof(pe);

    if (!Process32FirstW(snap, &pe))
    {
        CloseHandle(snap);
        return;
    }

    do
    {
        if (pe.th32ProcessID == 0 || pe.th32ProcessID == 4)
            continue;

        HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
        if (!proc)
            continue;

        if (is_x64_process(proc))
        {
            int len = WideCharToMultiByte(CP_UTF8, 0, pe.szExeFile, -1, nullptr, 0, nullptr, nullptr);
            std::string name(len - 1, '\0');
            WideCharToMultiByte(CP_UTF8, 0, pe.szExeFile, -1, name.data(), len, nullptr, nullptr);
            s_procs.push_back({pe.th32ProcessID, std::move(name)});
        }

        CloseHandle(proc);

    } while (Process32NextW(snap, &pe));

    CloseHandle(snap);
    std::ranges::sort(s_procs, {}, &ProcessEntry::name);
}

static void inject(DWORD pid)
{
    char self_path[MAX_PATH];
    GetModuleFileNameA(nullptr, self_path, MAX_PATH);

    std::string dll_path(self_path);
    const auto last_sep = dll_path.find_last_of("\\/");
    if (last_sep != std::string::npos)
        dll_path = dll_path.substr(0, last_sep + 1);
    dll_path += "ACT.dll";

    HANDLE proc = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION |
                              PROCESS_VM_WRITE | PROCESS_VM_READ |
                              PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!proc)
    {
        s_status       = "OpenProcess failed (error " + std::to_string(GetLastError()) + ")";
        s_status_ok    = false;
        s_status_timer = 4.0f;
        return;
    }

    const size_t path_size = dll_path.size() + 1;

    void* remote_buf = VirtualAllocEx(proc, nullptr, path_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote_buf)
    {
        s_status       = "VirtualAllocEx failed (error " + std::to_string(GetLastError()) + ")";
        s_status_ok    = false;
        s_status_timer = 4.0f;
        CloseHandle(proc);
        return;
    }

    if (!WriteProcessMemory(proc, remote_buf, dll_path.c_str(), path_size, nullptr))
    {
        s_status       = "WriteProcessMemory failed (error " + std::to_string(GetLastError()) + ")";
        s_status_ok    = false;
        s_status_timer = 4.0f;
        VirtualFreeEx(proc, remote_buf, 0, MEM_RELEASE);
        CloseHandle(proc);
        return;
    }

    HANDLE thread = CreateRemoteThread(proc, nullptr, 0,
        reinterpret_cast<LPTHREAD_START_ROUTINE>(
            GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA")),
        remote_buf, 0, nullptr);

    if (!thread)
    {
        s_status       = "CreateRemoteThread failed (error " + std::to_string(GetLastError()) + ")";
        s_status_ok    = false;
        s_status_timer = 4.0f;
        VirtualFreeEx(proc, remote_buf, 0, MEM_RELEASE);
        CloseHandle(proc);
        return;
    }

    WaitForSingleObject(thread, 5000);
    CloseHandle(thread);
    VirtualFreeEx(proc, remote_buf, 0, MEM_RELEASE);
    CloseHandle(proc);

    s_status       = "Injected into PID " + std::to_string(pid);
    s_status_ok    = true;
    s_status_timer = 4.0f;
    s_running      = false;
}

void gui::init()
{
    request_elevation();

    WNDCLASSEXW wc   = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_CLASSDC;
    wc.lpfnWndProc   = wnd_proc;
    wc.hInstance     = GetModuleHandle(nullptr);
    wc.lpszClassName = L"act_loader_wnd";
    ::RegisterClassExW(&wc);

    const int sx = GetSystemMetrics(SM_CXSCREEN);
    const int sy = GetSystemMetrics(SM_CYSCREEN);

    s_hwnd = ::CreateWindowExW(WS_EX_TOPMOST, wc.lpszClassName, L"ACT Loader",
        WS_POPUP, (sx - WIN_W) / 2, (sy - WIN_H) / 2, WIN_W, WIN_H,
        nullptr, nullptr, wc.hInstance, nullptr);

    if (!create_d3d(s_hwnd))
    {
        cleanup_d3d();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        MessageBoxW(nullptr, L"D3D11 device creation failed.", L"ACT Loader", MB_ICONERROR);
        ExitProcess(1);
    }

    ::ShowWindow(s_hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(s_hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io    = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    apply_theme();
    ImGui_ImplWin32_Init(s_hwnd);
    ImGui_ImplDX11_Init(s_device, s_ctx);

    refresh_processes();

    QueryPerformanceFrequency(&s_freq);
    QueryPerformanceCounter(&s_prev);
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
    ::DestroyWindow(s_hwnd);
}

void gui::render()
{
    static POINT drag_origin_cur;
    static RECT  drag_origin_rect;

    constexpr ImU32 COL_TITLEBAR      = IM_COL32(12,  12,  20,  255);
    constexpr ImU32 COL_TITLE_TEXT    = IM_COL32(190, 190, 215, 255);
    constexpr ImU32 COL_DIVIDER       = IM_COL32(28,  28,  46,  255);
    constexpr ImU32 COL_CLOSE_BG      = IM_COL32(160, 40,  40,  255);
    constexpr ImU32 COL_CLOSE_ICON    = IM_COL32(255, 255, 255, 255);
    constexpr ImU32 COL_CLOSE_IDLE    = IM_COL32(110, 110, 140, 255);
    constexpr ImU32 COL_SECTION_LABEL = IM_COL32(100, 100, 140, 255);
    constexpr ImU32 COL_DOT           = IM_COL32(55,  55,  255, 255);
    constexpr ImU32 COL_STATUS_OK     = IM_COL32(80,  200, 120, 255);
    constexpr ImU32 COL_STATUS_ERR    = IM_COL32(200, 70,  70,  255);

    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    const float dt = static_cast<float>(now.QuadPart - s_prev.QuadPart) / s_freq.QuadPart;
    s_prev = now;

    MSG msg = {};
    while (::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
        if (msg.message == WM_QUIT) { s_running = false; return; }
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    const ImVec2 display   = ImGui::GetIO().DisplaySize;
    const float  content_h = display.y - TITLEBAR_H;

    ImGui::SetNextWindowPos({0.0f, 0.0f});
    ImGui::SetNextWindowSize(display);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0f, 0.0f});
    ImGui::Begin("##root", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleVar();

    ImDrawList* dl = ImGui::GetWindowDrawList();

    dl->AddRectFilled({0.0f, 0.0f}, {display.x, TITLEBAR_H}, COL_TITLEBAR);
    dl->AddLine({0.0f, TITLEBAR_H}, {display.x, TITLEBAR_H}, COL_DIVIDER);

    const float text_y = (TITLEBAR_H - ImGui::GetTextLineHeight()) * 0.5f;
    dl->AddText({10.0f, text_y}, COL_TITLE_TEXT, "ACT Loader");

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
        SetWindowPos(s_hwnd, nullptr,
            drag_origin_rect.left + (cur.x - drag_origin_cur.x),
            drag_origin_rect.top  + (cur.y - drag_origin_cur.y),
            0, 0, SWP_NOSIZE | SWP_NOZORDER);
    }

    const ImVec2 close_tl = {display.x - TITLEBAR_H, 0.0f};
    const ImVec2 close_br = {display.x, TITLEBAR_H};

    ImGui::SetCursorPos({close_tl.x, close_tl.y});
    ImGui::InvisibleButton("##close", {TITLEBAR_H, TITLEBAR_H});
    const bool close_hov = ImGui::IsItemHovered();
    if (close_hov)
        dl->AddRectFilled(close_tl, close_br, COL_CLOSE_BG);

    {
        const float cx  = close_tl.x + TITLEBAR_H * 0.5f;
        const float cy  = TITLEBAR_H * 0.5f;
        const float r   = 4.5f;
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
    ImGui::BeginChild("##content", {display.x, content_h}, false,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();

    const float inner_w = ImGui::GetContentRegionAvail().x;
    const float label_h = ImGui::GetTextLineHeight() + 6.0f;

    {
        ImDrawList* cdl = ImGui::GetWindowDrawList();
        cdl->AddText(ImGui::GetCursorScreenPos(), COL_SECTION_LABEL, "PROCESSES  (x64)");
        ImGui::Dummy({0.0f, label_h});
    }

    ImGui::SetNextItemWidth(inner_w - 80.0f);
    ImGui::InputTextWithHint("##filter", "filter...", s_filter, sizeof(s_filter));
    ImGui::SameLine();
    if (ImGui::Button("Refresh", {72.0f, 0.0f}))
        refresh_processes();

    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.03f, 0.03f, 0.06f, 1.0f));
    const float list_h = content_h - label_h - ImGui::GetTextLineHeight() - 44.0f
                       - (s_status_timer > 0.0f ? ImGui::GetTextLineHeight() + 10.0f : 0.0f);
    ImGui::BeginChild("##procs", {inner_w, list_h}, false);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {4.0f, 6.0f});

    const std::string_view filt = s_filter;

    for (const auto& entry : s_procs)
    {
        if (!filt.empty())
        {
            auto it = std::search(entry.name.begin(), entry.name.end(),
                                  filt.begin(), filt.end(),
                                  [](char a, char b){ return std::tolower(a) == std::tolower(b); });
            if (it == entry.name.end())
                continue;
        }

        const ImVec2 p  = ImGui::GetCursorScreenPos();
        const float  lh = ImGui::GetTextLineHeight();

        ImGui::GetWindowDrawList()->AddCircleFilled({p.x + 5.0f, p.y + lh * 0.5f}, 3.5f, COL_DOT);
        ImGui::SetCursorScreenPos({p.x + 14.0f, p.y});

        std::string label = entry.name + "##" + std::to_string(entry.pid);
        if (ImGui::Selectable(label.c_str(), false, 0, {inner_w - 14.0f - 60.0f, 0.0f}))
            inject(entry.pid);

        char pid_buf[16];
        snprintf(pid_buf, sizeof(pid_buf), "%lu", entry.pid);
        const float pid_w = ImGui::CalcTextSize(pid_buf).x;
        ImGui::SameLine(inner_w - pid_w - 10.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.31f, 0.31f, 0.47f, 1.0f));
        ImGui::TextUnformatted(pid_buf);
        ImGui::PopStyleColor();
    }

    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::PopStyleColor();

    if (s_status_timer > 0.0f)
    {
        s_status_timer -= dt;
        ImGui::Spacing();
        ImDrawList* cdl = ImGui::GetWindowDrawList();
        cdl->AddText(ImGui::GetCursorScreenPos(), s_status_ok ? COL_STATUS_OK : COL_STATUS_ERR, s_status.c_str());
        ImGui::Dummy({0.0f, ImGui::GetTextLineHeight()});
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