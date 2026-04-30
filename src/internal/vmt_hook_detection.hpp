#pragma once
#include <windows.h>
#include <stdint.h>
#include <cstddef>

constexpr size_t D3D8_DEVICE_RESET = 14;
constexpr size_t D3D8_CREATE_DEVICE = 15;
constexpr size_t D3D8_DEVICE_PRESENT = 15;

constexpr size_t D3D9_DEVICE_RESET = 16;
constexpr size_t D3D9_DEVICE_PRESENT = 17;
constexpr size_t D3D9_DEVICE_END_SCENE = 42;
constexpr size_t D3D9_DEVICE_PRESENT_EX = 121;
constexpr size_t D3D9_SWAPCHAIN_PRESENT = 3;

constexpr size_t DXGI_SWAPCHAIN_PRESENT = 8;
constexpr size_t DXGI_SWAPCHAIN_RESIZE_BUFFERS = 13;
constexpr size_t DXGI_SWAPCHAIN_RESIZE_TARGET = 14;
constexpr size_t DXGI_SWAPCHAIN_PRESENT1 = 22;

constexpr size_t D3D12_COMMAND_QUEUE_EXECUTE_COMMAND_LISTS = 10;

struct d3d8_offsets  { uint32_t present, reset; };
struct d3d9_offsets  { uint32_t present, present_ex, present_swap, reset, end_scene; };
struct d3d10_offsets { uint32_t present, resize, resize_target, present1; };
struct d3d11_offsets { uint32_t present, resize, resize_target, present1; };
struct d3d12_offsets { uint32_t present, resize, resize_target, present1, execute_command_lists; };

inline uint32_t vtable_offset(HMODULE module, void *cls, unsigned int offset)
{
    uintptr_t *vtable = *(uintptr_t **)cls;
    return (uint32_t)(vtable[offset] - (uintptr_t)module);
}

namespace gfx_offsets {
    extern d3d8_offsets  d3d8;
    extern d3d9_offsets  d3d9;
    extern d3d10_offsets d3d10;
    extern d3d11_offsets d3d11;
    extern d3d12_offsets d3d12;
    void collect();
    void check();
}
