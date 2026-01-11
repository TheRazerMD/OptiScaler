#pragma once

#include <pch.h>

class D3D11Hooks
{
  public:
    static void Hook(HMODULE dx11Module);
    static void Unhook();
};
