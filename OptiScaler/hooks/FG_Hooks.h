#pragma once

#include <pch.h>

#include <dxgi1_6.h>

class FGHooks
{
  public:
    static HRESULT CreateSwapChain(IDXGIFactory* pFactory, IUnknown* pDevice, DXGI_SWAP_CHAIN_DESC* pDesc,
                                   IDXGISwapChain** ppSwapChain);

    static HRESULT CreateSwapChainForHwnd(IDXGIFactory* This, IUnknown* pDevice, HWND hWnd,
                                          DXGI_SWAP_CHAIN_DESC1* pDesc,
                                          DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pFullscreenDesc,
                                          IDXGIOutput* pRestrictToOutput, IDXGISwapChain1** ppSwapChain);

  private:
    typedef HRESULT (*PFN_Present)(void* This, UINT SyncInterval, UINT Flags);
    typedef HRESULT (*PFN_Present1)(void* This, UINT SyncInterval, UINT Flags,
                                    const DXGI_PRESENT_PARAMETERS* pPresentParameters);
    typedef HRESULT (*PFN_GetFullscreenDesc)(IDXGISwapChain* This, DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pDesc);
    typedef HRESULT (*PFN_SetFullscreenState)(IDXGISwapChain* This, BOOL Fullscreen, IDXGIOutput* pTarget);
    typedef HRESULT (*PFN_GetFullscreenState)(IDXGISwapChain* This, BOOL* pFullscreen, IDXGIOutput** ppTarget);
    typedef HRESULT (*PFN_ResizeBuffers)(IDXGISwapChain* This, UINT BufferCount, UINT Width, UINT Height,
                                         DXGI_FORMAT NewFormat, UINT SwapChainFlags);
    typedef HRESULT (*PFN_ResizeBuffers1)(IDXGISwapChain* This, UINT BufferCount, UINT Width, UINT Height,
                                          DXGI_FORMAT Format, UINT SwapChainFlags, const UINT* pCreationNodeMask,
                                          IUnknown* const* ppPresentQueue);
    typedef HRESULT (*PFN_ResizeTarget)(IDXGISwapChain* This, DXGI_MODE_DESC* pNewTargetParameters);
    typedef HRESULT (*PFN_Release)(IDXGISwapChain* This);

    inline static PFN_ResizeBuffers o_FGSCResizeBuffers = nullptr;
    inline static PFN_ResizeTarget o_FGSCResizeTarget = nullptr;
    inline static PFN_ResizeBuffers1 o_FGSCResizeBuffers1 = nullptr;
    inline static PFN_SetFullscreenState o_FGSCSetFullscreenState = nullptr;
    inline static PFN_GetFullscreenState o_FGSCGetFullscreenState = nullptr;
    inline static PFN_GetFullscreenDesc o_FGSCGetFullscreenDesc = nullptr;
    inline static PFN_Present o_FGSCPresent = nullptr;
    inline static PFN_Present1 o_FGSCPresent1 = nullptr;
    inline static PFN_Release o_FGRelease = nullptr;
    inline static HWND _hwnd = nullptr;
    inline static bool _skipResize = false;
    inline static bool _skipResize1 = false;
    inline static bool _skipPresent = false;
    inline static bool _skipPresent1 = false;
    inline static UINT _lastPresentFlags = 0;
    inline static double _lastFGFrameTime = 0.0;

    static void HookFGSwapchain(IDXGISwapChain* pSwapChain);

    static HRESULT hkSetFullscreenState(IDXGISwapChain* This, BOOL Fullscreen, IDXGIOutput* pTarget);
    static HRESULT hkGetFullscreenDesc(IDXGISwapChain* This, DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pDesc);
    static HRESULT hkGetFullscreenState(IDXGISwapChain* This, BOOL* pFullscreen, IDXGIOutput** ppTarget);
    static HRESULT hkResizeBuffers(IDXGISwapChain* This, UINT BufferCount, UINT Width, UINT Height,
                                   DXGI_FORMAT NewFormat, UINT SwapChainFlags);
    static HRESULT hkResizeTarget(IDXGISwapChain* This, DXGI_MODE_DESC* pNewTargetParameters);
    static HRESULT hkResizeBuffers1(IDXGISwapChain* This, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT Format,
                                    UINT SwapChainFlags, const UINT* pCreationNodeMask,
                                    IUnknown* const* ppPresentQueue);
    static HRESULT hkFGRelease(IDXGISwapChain* This);

    static HRESULT hkFGPresent(void* This, UINT SyncInterval, UINT Flags);
    static HRESULT hkFGPresent1(void* This, UINT SyncInterval, UINT Flags,
                                const DXGI_PRESENT_PARAMETERS* pPresentParameters);
    static HRESULT FGPresent(void* This, UINT SyncInterval, UINT Flags,
                             const DXGI_PRESENT_PARAMETERS* pPresentParameters);
};
