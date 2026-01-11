#pragma once

#include <dxgi.h>
#include <d3d12.h>
#include <fakenvapi_inc.h>
#include "NvApiTypes.h"

class fakenvapi
{
    inline static struct AntiLag2Data
    {
        void* context;
        bool enabled;
    } antilag2_data;

    inline static decltype(&Fake_InformFGState) Fake_InformFGState = nullptr;
    inline static decltype(&Fake_InformPresentFG) Fake_InformPresentFG = nullptr;
    inline static decltype(&Fake_GetAntiLagCtx) Fake_GetAntiLagCtx = nullptr;
    inline static decltype(&Fake_GetLowLatencyCtx) Fake_GetLowLatencyCtx = nullptr;
    inline static decltype(&Fake_SetLowLatencyCtx) Fake_SetLowLatencyCtx = nullptr;

    inline static bool _inited = false;
    inline static bool _initedForNvidia = false;
    inline static void* _lowLatencyContext = nullptr;
    inline static Mode _lowLatencyMode = Mode::LatencyFlex;
    inline static HMODULE _dllForNvidia = nullptr;

  public:
    inline static const GUID IID_IFfxAntiLag2Data = {
        0x5083ae5b, 0x8070, 0x4fca, { 0x8e, 0xe5, 0x35, 0x82, 0xdd, 0x36, 0x7d, 0x13 }
    };

    inline static decltype(&NvAPI_D3D_SetSleepMode) ForNvidia_SetSleepMode = nullptr;
    inline static decltype(&NvAPI_D3D_Sleep) ForNvidia_Sleep = nullptr;
    inline static decltype(&NvAPI_D3D_GetLatency) ForNvidia_GetLatency = nullptr;
    inline static decltype(&NvAPI_D3D_SetLatencyMarker) ForNvidia_SetLatencyMarker = nullptr;
    inline static decltype(&NvAPI_D3D12_SetAsyncFrameMarker) ForNvidia_SetAsyncFrameMarker = nullptr;

    static void Init(PFN_NvApi_QueryInterface& queryInterface);
    static void reportFGPresent(IDXGISwapChain* pSwapChain, bool fg_state, bool frame_interpolated);
    static bool updateModeAndContext();
    static bool setModeAndContext(void* context, Mode mode);
    static bool loadForNvidia();
    static Mode getCurrentMode();
    static bool isUsingFakenvapi();
    static bool isUsingFakenvapiOnNvidia();
};
