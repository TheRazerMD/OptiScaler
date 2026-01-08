#pragma once

#include <pch.h>

#include <d3d12.h>

class D3D12Hooks
{
  public:
    static void Hook();
    static void HookAgility(HMODULE module);
    static void HookDevice(ID3D12Device* device);
    static void Unhook();
};
