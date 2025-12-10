#pragma once

#include "Config.h"

typedef uint64_t (*PFN_getModelBlob)(uint32_t preset, uint64_t unknown, uint64_t* source, uint64_t* size);
typedef uint64_t (*PFN_createModel)(void* context, uint32_t preset);

class FSR4ModelSelection
{
    static uint64_t hkgetModelBlob(uint32_t preset, uint64_t unknown, uint64_t* source, uint64_t* size);
    static PFN_getModelBlob o_getModelBlob;
    static uint64_t hkcreateModel(void* context, uint32_t preset);
    static PFN_createModel o_createModel;

  public:
    static void Hook(HMODULE module, bool unhookOld = true);
};
