#pragma once

#include <pch.h>

class VulkanHooks
{
  public:
    static void Hook(HMODULE vulkan1);
    static void Unhook();
};
