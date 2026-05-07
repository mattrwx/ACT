#include "vtable.hpp"
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl.h>

bool get_dx11_vtable(void**& vtable)
{
    DXGI_SWAP_CHAIN_DESC swap_chain_desc{};
    swap_chain_desc.BufferCount = 1;
    swap_chain_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_desc.OutputWindow = GetForegroundWindow();
    swap_chain_desc.SampleDesc.Count = 1;
    swap_chain_desc.Windowed = TRUE;

    IDXGISwapChain* swap_chain{};
    ID3D11Device* device_ptr{};
    ID3D11DeviceContext* context{};

    if (FAILED(D3D11CreateDeviceAndSwapChain(0, D3D_DRIVER_TYPE_HARDWARE, 0, 0, 0, 0, 7, &swap_chain_desc, &swap_chain, &device_ptr, 0, &context)))
        return false;

    vtable = *reinterpret_cast<void***>(swap_chain);

    swap_chain->Release();
    device_ptr->Release();
    context->Release();

    return true;
}

bool get_dx12_vtable(void**& vtable)
{
    using Microsoft::WRL::ComPtr;

    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.BufferCount = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    ComPtr<ID3D12Device> device;
    if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device))))
        return false;

    ComPtr<IDXGISwapChain1> swapChain1;
    ComPtr<IDXGIFactory4> factory;

    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))
        return false;

    if (FAILED(factory->CreateSwapChainForHwnd(nullptr, GetForegroundWindow(), &desc, nullptr, nullptr, &swapChain1)))
        return false;

    ComPtr<IDXGISwapChain3> swapChain;
    swapChain1.As(&swapChain);

    vtable = *reinterpret_cast<void***>(swapChain.Get());

    return true;
}

void validate_vtable(void** vtable, size_t size)
{
    for (auto i{0}; i < size; i++)
        if (!utils::is_valid_code_region(vtable[i]))
            raise_flag(flags::vmt_hook, std::format("VTable hook detected {:X}", (uintptr_t)vtable[i]).c_str());
}

void validate_directx_vtables()
{
    void** vtable{};

    if (get_dx11_vtable(vtable))
        validate_vtable(vtable, 18);

    if (get_dx12_vtable(vtable))
        validate_vtable(vtable, 30);
}

void validate_rtti_vtables()
{
    for (auto rtti_vtable : rtti::get_vtables())
        validate_vtable(rtti_vtable.first, rtti_vtable.second);
}

void vtable::validate_all()
{
    validate_directx_vtables();

    validate_rtti_vtables();
}
