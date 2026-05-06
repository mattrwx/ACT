#include "vtable.hpp"
#include <VersionHelpers.h>
#include <d3d10_1.h>
#include <d3d11.h>
#include <d3d12.h>
#include <d3d9.h>
#include <dxgi1_2.h>
#include <print>
#include "../cache.hpp"
#include "../flags.hpp"
#include "../utils.hpp"


typedef HRESULT(WINAPI* d3d9createex_t)(UINT, IDirect3D9Ex**);
typedef HRESULT(WINAPI* d3d10create_t)(IDXGIAdapter*, D3D10_DRIVER_TYPE, HMODULE, UINT, UINT, DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**, ID3D10Device**);
typedef HRESULT(WINAPI* d3d11create_t)(
        IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT, const D3D_FEATURE_LEVEL*, UINT, UINT, const DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**, ID3D11Device**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext**
);
typedef HRESULT(WINAPI* d3d12create_t)(IUnknown*, D3D_FEATURE_LEVEL, REFIID, void**);
typedef HRESULT(WINAPI* create_fac_t)(REFIID, void**);

static const IID dxgiFactory2 = {
        0x50c83a1c, 0xe072, 0x4c48, {0x87, 0xb0, 0x36, 0x30, 0xfa, 0x36, 0xa6, 0xd0}
};

struct D3D8PP
{
    UINT BackBufferWidth, BackBufferHeight;
    DWORD BackBufferFormat;
    UINT BackBufferCount;
    DWORD MultiSampleType;
    DWORD SwapEffect;
    HWND hDeviceWindow;
    BOOL Windowed, EnableAutoDepthStencil;
    DWORD AutoDepthStencilFormat, Flags;
    UINT FullScreen_RefreshRateInHz, FullScreen_PresentationInterval;
};

struct d3d8_info
{
    HMODULE module;
    void* d3d8;
    void* device;
};

struct d3d9_info
{
    HMODULE module;
    IDirect3D9Ex* d3d9ex;
    IDirect3DDevice9Ex* device;
    IDirect3DSwapChain9* swap;
};

struct d3d10_info
{
    HMODULE module;
    IDXGISwapChain* swap;
};

struct d3d11_info
{
    HMODULE module;
    IDXGISwapChain* swap;
};

struct d3d12_info
{
    HMODULE module;
    HMODULE d3d12_module;
    IDXGISwapChain* swap;
    ID3D12CommandQueue* queue;
};

bool d3d8_init(d3d8_info& info)
{
    info.module = GetModuleHandleA("d3d8.dll");
    if (!info.module)
        return false;

    auto create = reinterpret_cast<void*(__stdcall*)(UINT)>(GetProcAddress(info.module, "Direct3DCreate8"));
    if (!create)
        return false;

    info.d3d8 = create(220);
    if (!info.d3d8)
        return false;

    D3D8PP pp = {};
    pp.Windowed = TRUE;
    pp.SwapEffect = 1;
    pp.BackBufferFormat = 21;
    pp.BackBufferWidth = pp.BackBufferHeight = 2;
    pp.BackBufferCount = 1;
    pp.hDeviceWindow = cache::game_hwnd;

    using CreateDevice_t = HRESULT(__stdcall*)(void*, UINT, DWORD, HWND, DWORD, D3D8PP*, void**);
    auto CreateDevice = reinterpret_cast<CreateDevice_t>((*(void***)info.d3d8)[D3D8_CREATE_DEVICE]);
    return SUCCEEDED(CreateDevice(info.d3d8, 0, 1, cache::game_hwnd, 0x40, &pp, &info.device));
}

bool d3d9_init(d3d9_info& info)
{
    info.module = GetModuleHandleA("d3d9.dll");
    if (!info.module)
        return false;

    auto create = (d3d9createex_t)GetProcAddress(info.module, "Direct3DCreate9Ex");
    if (!create)
        return false;

    if (FAILED(create(D3D_SDK_VERSION, &info.d3d9ex)))
        return false;

    D3DPRESENT_PARAMETERS pp = {};
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.BackBufferFormat = D3DFMT_A8R8G8B8;
    pp.BackBufferWidth = pp.BackBufferHeight = 2;
    pp.BackBufferCount = 1;
    pp.hDeviceWindow = cache::game_hwnd;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

    if (FAILED(info.d3d9ex->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, cache::game_hwnd, D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_NOWINDOWCHANGES, &pp, nullptr, &info.device)))
        return false;

    return SUCCEEDED(info.device->GetSwapChain(0, &info.swap));
}

bool d3d10_init(d3d10_info& info)
{
    info.module = GetModuleHandleA("dxgi.dll");
    if (!info.module)
        return false;

    auto create_factory = (create_fac_t)GetProcAddress(info.module, "CreateDXGIFactory1");
    if (!create_factory)
        return false;

    HMODULE d3d10_mod = GetModuleHandleA("d3d10.dll");
    if (!d3d10_mod)
        return false;

    auto create = (d3d10create_t)GetProcAddress(d3d10_mod, "D3D10CreateDeviceAndSwapChain");
    if (!create)
        return false;

    IID fac_iid = IsWindows8OrGreater() ? dxgiFactory2 : __uuidof(IDXGIFactory1);
    IDXGIFactory1* factory = nullptr;
    if (FAILED(create_factory(fac_iid, (void**)&factory)))
        return false;

    IDXGIAdapter1* adapter = nullptr;
    HRESULT hr = factory->EnumAdapters1(0, &adapter);
    factory->Release();
    if (FAILED(hr))
        return false;

    DXGI_SWAP_CHAIN_DESC desc = {};
    desc.BufferCount = 2;
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferDesc.Width = desc.BufferDesc.Height = 2;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.OutputWindow = cache::game_hwnd;
    desc.SampleDesc.Count = 1;
    desc.Windowed = TRUE;

    ID3D10Device* device = nullptr;
    hr = create(adapter, D3D10_DRIVER_TYPE_HARDWARE, nullptr, 0, D3D10_SDK_VERSION, &desc, &info.swap, &device);
    adapter->Release();
    if (FAILED(hr))
        return false;

    device->Release();
    return true;
}

bool d3d11_init(d3d11_info& info)
{
    info.module = GetModuleHandleA("dxgi.dll");
    if (!info.module)
        return false;

    HMODULE d3d11_mod = GetModuleHandleA("d3d11.dll");
    if (!d3d11_mod)
        return false;

    auto create = (d3d11create_t)GetProcAddress(d3d11_mod, "D3D11CreateDeviceAndSwapChain");
    if (!create)
        return false;

    DXGI_SWAP_CHAIN_DESC desc = {};
    desc.BufferCount = 2;
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferDesc.Width = desc.BufferDesc.Height = 2;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.OutputWindow = cache::game_hwnd;
    desc.SampleDesc.Count = 1;
    desc.Windowed = TRUE;

    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    D3D_FEATURE_LEVEL level = {};
    HRESULT hr = create(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &desc, &info.swap, &device, &level, &context);
    if (FAILED(hr))
        return false;

    if (context)
        context->Release();
    if (device)
        device->Release();
    return true;
}

bool d3d12_init(d3d12_info& info)
{
    info.module = GetModuleHandleA("dxgi.dll");
    info.d3d12_module = GetModuleHandleA("d3d12.dll");
    if (!info.module || !info.d3d12_module)
        return false;

    auto create_device = (d3d12create_t)GetProcAddress(info.d3d12_module, "D3D12CreateDevice");
    auto create_factory = (create_fac_t)GetProcAddress(info.module, "CreateDXGIFactory1");
    if (!create_device || !create_factory)
        return false;

    IDXGIFactory2* factory = nullptr;
    if (FAILED(create_factory(__uuidof(IDXGIFactory2), (void**)&factory)))
        return false;

    IDXGIAdapter1* adapter = nullptr;
    HRESULT hr = factory->EnumAdapters1(0, &adapter);
    if (FAILED(hr))
    {
        factory->Release();
        return false;
    }

    ID3D12Device* device = nullptr;
    hr = create_device(adapter, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), (void**)&device);
    adapter->Release();
    if (FAILED(hr))
    {
        factory->Release();
        return false;
    }

    D3D12_COMMAND_QUEUE_DESC queue_desc = {};
    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    hr = device->CreateCommandQueue(&queue_desc, __uuidof(ID3D12CommandQueue), (void**)&info.queue);
    if (FAILED(hr))
    {
        device->Release();
        factory->Release();
        return false;
    }

    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.BufferCount = 2;
    desc.Width = desc.Height = 2;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.SampleDesc.Count = 1;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    IDXGISwapChain1* swap = nullptr;
    hr = factory->CreateSwapChainForHwnd(info.queue, cache::game_hwnd, &desc, nullptr, nullptr, &swap);

    device->Release();
    factory->Release();

    if (FAILED(hr))
    {
        info.queue->Release();
        info.queue = nullptr;
        return false;
    }

    info.swap = swap;
    return true;
}

typedef HRESULT(__stdcall* d3d8_present_t)(void*, const RECT*, const RECT*, HWND, const RGNDATA*);
static d3d8_present_t d3d8_original_present = nullptr;

typedef HRESULT(__stdcall* d3d9_present_t)(void*, const RECT*, const RECT*, HWND, const RGNDATA*);
static d3d9_present_t d3d9_original_present = nullptr;

typedef HRESULT(__stdcall* swapchain_present_t)(void*, UINT, UINT);
static swapchain_present_t swapchain_original_present = nullptr;

typedef HRESULT(__stdcall* swapchain_present1_t)(void*, UINT, UINT, const DXGI_PRESENT_PARAMETERS*);
static swapchain_present1_t swapchain_original_present1 = nullptr;

typedef void(__stdcall* d3d12_execute_command_lists_t)(void*, UINT, ID3D12CommandList* const*);
static d3d12_execute_command_lists_t d3d12_original_execute_command_lists = nullptr;

template <typename Interface, typename Source>
bool query_interface_vtable(Source* source, uintptr_t** vtable)
{
    Interface* queried = nullptr;
    if (FAILED(source->QueryInterface(__uuidof(Interface), reinterpret_cast<void**>(&queried))) || !queried)
        return false;

    *vtable = *reinterpret_cast<uintptr_t**>(queried);
    queried->Release();
    return *vtable != nullptr;
}

void cache_swapchain(void* swap_ptr, bool present1)
{
    auto* swap = static_cast<IDXGISwapChain*>(swap_ptr);
    IUnknown* owner = nullptr;

    if (SUCCEEDED(swap->GetDevice(__uuidof(ID3D12CommandQueue), (void**)&owner)))
    {
        cache::game_d3d12_swap = swap_ptr;
        if (present1)
            cache::game_d3d12_swap1 = swap_ptr;
        owner->Release();
        return;
    }

    if (SUCCEEDED(swap->GetDevice(__uuidof(ID3D11Device), (void**)&owner)))
    {
        cache::game_d3d11_swap = swap_ptr;
        if (present1)
            cache::game_d3d11_swap1 = swap_ptr;
        owner->Release();
        return;
    }

    if (SUCCEEDED(swap->GetDevice(__uuidof(ID3D10Device), (void**)&owner)))
    {
        cache::game_d3d10_swap = swap_ptr;
        if (present1)
            cache::game_d3d10_swap1 = swap_ptr;
        owner->Release();
        return;
    }
}

template <typename Original>
void install_vtable_hook(uintptr_t* vtable, size_t slot, void* hook, Original& original_out)
{
    DWORD old;
    VirtualProtect(&vtable[slot], sizeof(uintptr_t), PAGE_EXECUTE_READWRITE, &old);
    original_out = reinterpret_cast<Original>(vtable[slot]);
    vtable[slot] = reinterpret_cast<uintptr_t>(hook);
    VirtualProtect(&vtable[slot], sizeof(uintptr_t), old, &old);
}

void restore_vtable_slot(uintptr_t* vtable, size_t slot, uintptr_t replacement)
{
    DWORD old;
    VirtualProtect(&vtable[slot], sizeof(uintptr_t), PAGE_EXECUTE_READWRITE, &old);
    vtable[slot] = replacement;
    VirtualProtect(&vtable[slot], sizeof(uintptr_t), old, &old);
}

HRESULT __stdcall hooked_d3d8_present(void* rcx, const RECT* src, const RECT* dst, HWND wnd, const RGNDATA* dirty)
{
    uintptr_t* vtable = *(uintptr_t**)rcx;
    restore_vtable_slot(vtable, D3D8_DEVICE_PRESENT, reinterpret_cast<uintptr_t>(d3d8_original_present));
    cache::game_d3d8_device = rcx;
    return d3d8_original_present(rcx, src, dst, wnd, dirty);
}

HRESULT __stdcall hooked_d3d9_present(void* rcx, const RECT* src, const RECT* dst, HWND wnd, const RGNDATA* dirty)
{
    uintptr_t* vtable = *(uintptr_t**)rcx;
    restore_vtable_slot(vtable, D3D9_DEVICE_PRESENT, reinterpret_cast<uintptr_t>(d3d9_original_present));
    cache::game_d3d9_device = rcx;

    IDirect3DSwapChain9* swap = nullptr;
    if (SUCCEEDED(static_cast<IDirect3DDevice9*>(rcx)->GetSwapChain(0, &swap)) && swap)
    {
        cache::game_d3d9_swap = swap;
        swap->Release();
    }

    return d3d9_original_present(rcx, src, dst, wnd, dirty);
}

HRESULT __stdcall hooked_swapchain_present(void* rcx, UINT sync, UINT flags)
{
    uintptr_t* vtable = *(uintptr_t**)rcx;
    restore_vtable_slot(vtable, DXGI_SWAPCHAIN_PRESENT, reinterpret_cast<uintptr_t>(swapchain_original_present));
    cache_swapchain(rcx, false);
    return swapchain_original_present(rcx, sync, flags);
}

HRESULT __stdcall hooked_swapchain_present1(void* rcx, UINT sync, UINT flags, const DXGI_PRESENT_PARAMETERS* params)
{
    uintptr_t* vtable = *(uintptr_t**)rcx;
    restore_vtable_slot(vtable, DXGI_SWAPCHAIN_PRESENT1, reinterpret_cast<uintptr_t>(swapchain_original_present1));
    cache_swapchain(rcx, true);
    return swapchain_original_present1(rcx, sync, flags, params);
}

void __stdcall hooked_d3d12_execute_command_lists(void* rcx, UINT num_command_lists, ID3D12CommandList* const* command_lists)
{
    uintptr_t* vtable = *(uintptr_t**)rcx;
    restore_vtable_slot(vtable, D3D12_COMMAND_QUEUE_EXECUTE_COMMAND_LISTS, reinterpret_cast<uintptr_t>(d3d12_original_execute_command_lists));
    cache::game_d3d12_queue = rcx;
    d3d12_original_execute_command_lists(rcx, num_command_lists, command_lists);
}

void get_d3d8_offsets(d3d8_offsets* offsets)
{
    d3d8_info info = {};
    bool success = d3d8_init(info);

    if (success)
    {
        offsets->present = vtable_offset(info.module, info.device, D3D8_DEVICE_PRESENT);
        offsets->reset = vtable_offset(info.module, info.device, D3D8_DEVICE_RESET);

        if (!d3d8_original_present)
        {
            uintptr_t* vtable = *(uintptr_t**)info.device;
            install_vtable_hook(vtable, D3D8_DEVICE_PRESENT, (void*)hooked_d3d8_present, d3d8_original_present);
        }
    }

    if (info.device)
        reinterpret_cast<IUnknown*>(info.device)->Release();
    if (info.d3d8)
        reinterpret_cast<IUnknown*>(info.d3d8)->Release();
}

void get_d3d9_offsets(d3d9_offsets* offsets)
{
    d3d9_info info = {};
    bool success = d3d9_init(info);

    if (success)
    {
        offsets->present = vtable_offset(info.module, info.device, D3D9_DEVICE_PRESENT);
        offsets->present_ex = vtable_offset(info.module, info.device, D3D9_DEVICE_PRESENT_EX);
        offsets->present_swap = vtable_offset(info.module, info.swap, D3D9_SWAPCHAIN_PRESENT);
        offsets->reset = vtable_offset(info.module, info.device, D3D9_DEVICE_RESET);
        offsets->end_scene = vtable_offset(info.module, info.device, D3D9_DEVICE_END_SCENE);

        if (!d3d9_original_present)
        {
            uintptr_t* vtable = *(uintptr_t**)info.device;
            install_vtable_hook(vtable, D3D9_DEVICE_PRESENT, (void*)hooked_d3d9_present, d3d9_original_present);
        }
    }

    if (info.swap)
        info.swap->Release();
    if (info.device)
        info.device->Release();
    if (info.d3d9ex)
        info.d3d9ex->Release();
}

void get_d3d10_offsets(d3d10_offsets* offsets)
{
    d3d10_info info = {};
    bool success = d3d10_init(info);

    if (success)
    {
        offsets->present = vtable_offset(info.module, info.swap, DXGI_SWAPCHAIN_PRESENT);
        offsets->resize = vtable_offset(info.module, info.swap, DXGI_SWAPCHAIN_RESIZE_BUFFERS);
        offsets->resize_target = vtable_offset(info.module, info.swap, DXGI_SWAPCHAIN_RESIZE_TARGET);

        if (!swapchain_original_present)
        {
            uintptr_t* vtable = *(uintptr_t**)info.swap;
            install_vtable_hook(vtable, DXGI_SWAPCHAIN_PRESENT, (void*)hooked_swapchain_present, swapchain_original_present);
        }

        uintptr_t* swap1_vtable = nullptr;
        if (query_interface_vtable<IDXGISwapChain1>(info.swap, &swap1_vtable))
        {
            offsets->present1 = (uint32_t)(swap1_vtable[DXGI_SWAPCHAIN_PRESENT1] - (uintptr_t)info.module);

            if (!swapchain_original_present1)
                install_vtable_hook(swap1_vtable, DXGI_SWAPCHAIN_PRESENT1, (void*)hooked_swapchain_present1, swapchain_original_present1);
        }
    }

    if (info.swap)
        info.swap->Release();
}

static uintptr_t resolve_real(uintptr_t slot_value, void* hook_fn, void* original_fn)
{
    if (slot_value == reinterpret_cast<uintptr_t>(hook_fn) && original_fn)
        return reinterpret_cast<uintptr_t>(original_fn);
    return slot_value;
}

void get_d3d11_offsets(d3d11_offsets* offsets)
{
    d3d11_info info = {};
    bool success = d3d11_init(info);

    if (success)
    {
        uintptr_t* vtable = *(uintptr_t**)info.swap;
        uintptr_t present = resolve_real(vtable[DXGI_SWAPCHAIN_PRESENT], (void*)hooked_swapchain_present, (void*)swapchain_original_present);

        offsets->present = (uint32_t)(present - (uintptr_t)info.module);
        offsets->resize = vtable_offset(info.module, info.swap, DXGI_SWAPCHAIN_RESIZE_BUFFERS);
        offsets->resize_target = vtable_offset(info.module, info.swap, DXGI_SWAPCHAIN_RESIZE_TARGET);

        if (!swapchain_original_present)
        {
            install_vtable_hook(vtable, DXGI_SWAPCHAIN_PRESENT, (void*)hooked_swapchain_present, swapchain_original_present);
        }

        uintptr_t* swap1_vtable = nullptr;
        if (query_interface_vtable<IDXGISwapChain1>(info.swap, &swap1_vtable))
        {
            uintptr_t present1 = resolve_real(swap1_vtable[DXGI_SWAPCHAIN_PRESENT1], (void*)hooked_swapchain_present1, (void*)swapchain_original_present1);
            offsets->present1 = (uint32_t)(present1 - (uintptr_t)info.module);

            if (!swapchain_original_present1)
                install_vtable_hook(swap1_vtable, DXGI_SWAPCHAIN_PRESENT1, (void*)hooked_swapchain_present1, swapchain_original_present1);
        }
    }

    if (info.swap)
        info.swap->Release();
}

void get_d3d12_offsets(d3d12_offsets* offsets)
{
    d3d12_info info = {};
    bool success = d3d12_init(info);

    if (success)
    {
        uintptr_t* swap_vt = *(uintptr_t**)info.swap;
        uintptr_t present = resolve_real(swap_vt[DXGI_SWAPCHAIN_PRESENT], (void*)hooked_swapchain_present, (void*)swapchain_original_present);

        offsets->present = (uint32_t)(present - (uintptr_t)info.module);
        offsets->resize = vtable_offset(info.module, info.swap, DXGI_SWAPCHAIN_RESIZE_BUFFERS);
        offsets->resize_target = vtable_offset(info.module, info.swap, DXGI_SWAPCHAIN_RESIZE_TARGET);
        offsets->execute_command_lists = vtable_offset(info.d3d12_module, info.queue, D3D12_COMMAND_QUEUE_EXECUTE_COMMAND_LISTS);

        if (!swapchain_original_present)
        {
            install_vtable_hook(swap_vt, DXGI_SWAPCHAIN_PRESENT, (void*)hooked_swapchain_present, swapchain_original_present);
        }

        uintptr_t* swap1_vtable = nullptr;
        if (query_interface_vtable<IDXGISwapChain1>(info.swap, &swap1_vtable))
        {
            uintptr_t present1 = resolve_real(swap1_vtable[DXGI_SWAPCHAIN_PRESENT1], (void*)hooked_swapchain_present1, (void*)swapchain_original_present1);
            offsets->present1 = (uint32_t)(present1 - (uintptr_t)info.module);

            if (!swapchain_original_present1)
                install_vtable_hook(swap1_vtable, DXGI_SWAPCHAIN_PRESENT1, (void*)hooked_swapchain_present1, swapchain_original_present1);
        }

        if (!d3d12_original_execute_command_lists)
        {
            uintptr_t* queue_vt = *(uintptr_t**)info.queue;
            install_vtable_hook(queue_vt, D3D12_COMMAND_QUEUE_EXECUTE_COMMAND_LISTS, (void*)hooked_d3d12_execute_command_lists, d3d12_original_execute_command_lists);
        }
    }

    if (info.swap)
        info.swap->Release();
    if (info.queue)
        info.queue->Release();
}

namespace gfx_offsets
{
    d3d8_offsets d3d8 = {};
    d3d9_offsets d3d9 = {};
    d3d10_offsets d3d10 = {};
    d3d11_offsets d3d11 = {};
    d3d12_offsets d3d12 = {};

    HWND find_game_hwnd()
    {
        HWND result = nullptr;
        const HWND console_hwnd = GetConsoleWindow();

        struct ctx
        {
            HWND* out;
            HWND console;
        };
        ctx c{&result, console_hwnd};

        EnumWindows(
                [](HWND h, LPARAM param) -> BOOL
                {
                    auto* c = reinterpret_cast<ctx*>(param);

                    if (h == c->console)
                        return TRUE;
                    if (!IsWindowVisible(h))
                        return TRUE;
                    if (GetWindow(h, GW_OWNER) != nullptr)
                        return TRUE;

                    DWORD pid = 0;
                    GetWindowThreadProcessId(h, &pid);
                    if (pid != GetCurrentProcessId())
                        return TRUE;

                    LONG style = GetWindowLongA(h, GWL_STYLE);
                    if (!(style & WS_VISIBLE))
                        return TRUE;

                    RECT rc{};
                    if (!GetClientRect(h, &rc))
                        return TRUE;
                    if (rc.right - rc.left < 32 || rc.bottom - rc.top < 32)
                        return TRUE;

                    *c->out = h;
                    return FALSE;
                },
                reinterpret_cast<LPARAM>(&c)
        );
        return result;
    }

    void collect()
    {
        cache::game_hwnd = find_game_hwnd();

        get_d3d8_offsets(&d3d8);
        get_d3d9_offsets(&d3d9);
        get_d3d10_offsets(&d3d10);
        get_d3d11_offsets(&d3d11);
        get_d3d12_offsets(&d3d12);
    }

    void check()
    {
        if (cache::game_d3d8_device && d3d8.present)
        {
            uintptr_t* vt = *(uintptr_t**)cache::game_d3d8_device;
            auto base = (uintptr_t)GetModuleHandleA("d3d8.dll");
            if (vt[D3D8_DEVICE_PRESENT] - base != gfx_offsets::d3d8.present)
                if (!utils::is_valid_code_region((void*)vt[D3D8_DEVICE_PRESENT]))
                    raise_flag(flags::directx_vmt_hook, "D3D8 Present VMT hook detected");

            if (vt[D3D8_DEVICE_RESET] - base != gfx_offsets::d3d8.reset)
                if (!utils::is_valid_code_region((void*)vt[D3D8_DEVICE_RESET]))
                    raise_flag(flags::directx_vmt_hook, "D3D8 Reset VMT hook detected");
        }

        if (cache::game_d3d9_device && d3d9.present)
        {
            uintptr_t* vt = *(uintptr_t**)cache::game_d3d9_device;
            auto base = (uintptr_t)GetModuleHandleA("d3d9.dll");
            if (vt[D3D9_DEVICE_PRESENT] - base != gfx_offsets::d3d9.present)
                if (!utils::is_valid_code_region((void*)vt[D3D9_DEVICE_PRESENT]))
                    raise_flag(flags::directx_vmt_hook, "D3D9 Present VMT hook detected");

            uintptr_t* d3d9ex_vt = nullptr;
            auto* device = static_cast<IDirect3DDevice9*>(cache::game_d3d9_device);
            if (query_interface_vtable<IDirect3DDevice9Ex>(device, &d3d9ex_vt))
            {
                auto present_ex = d3d9ex_vt[D3D9_DEVICE_PRESENT_EX];
                if (present_ex - base != gfx_offsets::d3d9.present_ex)
                    if (!utils::is_valid_code_region((void*)present_ex))
                        raise_flag(flags::directx_vmt_hook, "D3D9 PresentEx VMT hook detected");
            }

            if (vt[D3D9_DEVICE_RESET] - base != gfx_offsets::d3d9.reset)
                if (!utils::is_valid_code_region((void*)vt[D3D9_DEVICE_RESET]))
                    raise_flag(flags::directx_vmt_hook, "D3D9 Reset VMT hook detected");

            if (vt[D3D9_DEVICE_END_SCENE] - base != gfx_offsets::d3d9.end_scene)
                if (!utils::is_valid_code_region((void*)vt[D3D9_DEVICE_END_SCENE]))
                    raise_flag(flags::directx_vmt_hook, "D3D9 EndScene VMT hook detected");

            if (cache::game_d3d9_swap && gfx_offsets::d3d9.present_swap)
            {
                uintptr_t* swap_vt = *(uintptr_t**)cache::game_d3d9_swap;
                if (swap_vt[D3D9_SWAPCHAIN_PRESENT] - base != gfx_offsets::d3d9.present_swap)
                    if (!utils::is_valid_code_region((void*)swap_vt[D3D9_SWAPCHAIN_PRESENT]))
                        raise_flag(flags::directx_vmt_hook, "D3D9 SwapChain Present VMT hook detected");
            }
        }

        if (cache::game_d3d10_swap && d3d10.present)
        {
            uintptr_t* vt = *(uintptr_t**)cache::game_d3d10_swap;
            auto base = (uintptr_t)GetModuleHandleA("dxgi.dll");
            if (vt[DXGI_SWAPCHAIN_PRESENT] - base != gfx_offsets::d3d10.present)
                if (!utils::is_valid_code_region((void*)vt[DXGI_SWAPCHAIN_PRESENT]))
                    raise_flag(flags::directx_vmt_hook, "D3D10 Present VMT hook detected");

            if (vt[DXGI_SWAPCHAIN_RESIZE_BUFFERS] - base != gfx_offsets::d3d10.resize)
                if (!utils::is_valid_code_region((void*)vt[DXGI_SWAPCHAIN_RESIZE_BUFFERS]))
                    raise_flag(flags::directx_vmt_hook, "D3D10 ResizeBuffers VMT hook detected");

            if (vt[DXGI_SWAPCHAIN_RESIZE_TARGET] - base != gfx_offsets::d3d10.resize_target)
                if (!utils::is_valid_code_region((void*)vt[DXGI_SWAPCHAIN_RESIZE_TARGET]))
                    raise_flag(flags::directx_vmt_hook, "D3D10 ResizeTarget VMT hook detected");
        }

        if (cache::game_d3d10_swap1 && d3d10.present1)
        {
            uintptr_t* vt = *(uintptr_t**)cache::game_d3d10_swap1;
            auto base = (uintptr_t)GetModuleHandleA("dxgi.dll");
            if (vt[DXGI_SWAPCHAIN_PRESENT1] - base != gfx_offsets::d3d10.present1)
                if (!utils::is_valid_code_region((void*)vt[DXGI_SWAPCHAIN_PRESENT1]))
                    raise_flag(flags::directx_vmt_hook, "D3D10 Present1 VMT hook detected");
        }

        if (cache::game_d3d11_swap && d3d11.present)
        {
            uintptr_t* vt = *(uintptr_t**)cache::game_d3d11_swap;
            auto base = (uintptr_t)GetModuleHandleA("d3d11.dll");
            if (vt[DXGI_SWAPCHAIN_PRESENT] - base != gfx_offsets::d3d11.present)
                if (!utils::is_valid_code_region((void*)vt[DXGI_SWAPCHAIN_PRESENT]))
                    raise_flag(flags::directx_vmt_hook, "D3D11 Present VMT hook detected");

            if (vt[DXGI_SWAPCHAIN_RESIZE_BUFFERS] - base != gfx_offsets::d3d11.resize)
                if (!utils::is_valid_code_region((void*)vt[DXGI_SWAPCHAIN_RESIZE_BUFFERS]))
                    raise_flag(flags::directx_vmt_hook, "D3D11 ResizeBuffers VMT hook detected");

            if (vt[DXGI_SWAPCHAIN_RESIZE_TARGET] - base != gfx_offsets::d3d11.resize_target)
                if (!utils::is_valid_code_region((void*)vt[DXGI_SWAPCHAIN_RESIZE_TARGET]))
                    raise_flag(flags::directx_vmt_hook, "D3D11 ResizeTarget VMT hook detected");
        }

        if (cache::game_d3d11_swap1 && d3d11.present1)
        {
            uintptr_t* vt = *(uintptr_t**)cache::game_d3d11_swap1;
            auto base = (uintptr_t)GetModuleHandleA("dxgi.dll");
            if (vt[DXGI_SWAPCHAIN_PRESENT1] - base != gfx_offsets::d3d11.present1)
                if (!utils::is_valid_code_region((void*)vt[DXGI_SWAPCHAIN_PRESENT1]))
                    raise_flag(flags::directx_vmt_hook, "D3D11 Present1 VMT hook detected");
        }

        if (cache::game_d3d12_swap && d3d12.present)
        {
            uintptr_t* vt = *(uintptr_t**)cache::game_d3d12_swap;
            auto base = (uintptr_t)GetModuleHandleA("dxgi.dll");
            if (vt[DXGI_SWAPCHAIN_PRESENT] - base != gfx_offsets::d3d12.present)
                if (!utils::is_valid_code_region((void*)vt[DXGI_SWAPCHAIN_PRESENT]))
                    raise_flag(flags::directx_vmt_hook, "D3D12 Present VMT hook detected");

            if (vt[DXGI_SWAPCHAIN_RESIZE_BUFFERS] - base != gfx_offsets::d3d12.resize)
                if (!utils::is_valid_code_region((void*)vt[DXGI_SWAPCHAIN_RESIZE_BUFFERS]))
                    raise_flag(flags::directx_vmt_hook, "D3D12 ResizeBuffers VMT hook detected");

            if (vt[DXGI_SWAPCHAIN_RESIZE_TARGET] - base != gfx_offsets::d3d12.resize_target)
                if (!utils::is_valid_code_region((void*)vt[DXGI_SWAPCHAIN_RESIZE_TARGET]))
                    raise_flag(flags::directx_vmt_hook, "D3D12 ResizeTarget VMT hook detected");
        }

        if (cache::game_d3d12_swap1 && d3d12.present1)
        {
            uintptr_t* vt = *(uintptr_t**)cache::game_d3d12_swap1;
            auto base = (uintptr_t)GetModuleHandleA("dxgi.dll");
            if (vt[DXGI_SWAPCHAIN_PRESENT1] - base != gfx_offsets::d3d12.present1)
                if (!utils::is_valid_code_region((void*)vt[DXGI_SWAPCHAIN_PRESENT1]))
                    raise_flag(flags::directx_vmt_hook, "D3D12 Present1 VMT hook detected");
        }

        if (cache::game_d3d12_queue && d3d12.execute_command_lists)
        {
            uintptr_t* vt = *(uintptr_t**)cache::game_d3d12_queue;
            auto base = (uintptr_t)GetModuleHandleA("d3d12.dll");
            if (vt[D3D12_COMMAND_QUEUE_EXECUTE_COMMAND_LISTS] - base != gfx_offsets::d3d12.execute_command_lists)
                if (!utils::is_valid_code_region((void*)vt[D3D12_COMMAND_QUEUE_EXECUTE_COMMAND_LISTS]))
                    raise_flag(flags::directx_vmt_hook, "D3D12 ExecuteCommandLists VMT hook detected");
        }
    }
}
