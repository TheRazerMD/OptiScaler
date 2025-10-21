#pragma once

#include "Config.h"

typedef uint64_t (*PFN_getModelBlob)(uint32_t preset, uint64_t unknown, uint64_t* source, uint64_t* size);

class FSR4ModelSelection
{
    static uint64_t hkgetModelBlob(uint32_t preset, uint64_t unknown, uint64_t* source, uint64_t* size);
    static PFN_getModelBlob o_getModelBlob;

  public:
    static void Hook(HMODULE module, bool unhookOld = true);
};
