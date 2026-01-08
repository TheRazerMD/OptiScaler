#pragma once

#include <pch.h>

class D3D12Hooks
{
  public:
    static void Hook();
    static void HookAgility(HMODULE module);
    static void Unhook();
};
