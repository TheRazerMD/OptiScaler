#include "DxgiFactory_Hooks.h"

#include <Config.h>

#include <spoofing/Dxgi_Spoofing.h>
#include <wrapped/wrapped_swapchain.h>

#include <magic_enum.hpp>
#include <detours/detours.h>

void DxgiFactoryHooks::CheckAdapter(IUnknown* unkAdapter)
{
    if (State::Instance().isRunningOnDXVK)
        return;

    // DXVK VkInterface GUID
    const GUID guid = { 0x907bf281, 0xea3c, 0x43b4, { 0xa8, 0xe4, 0x9f, 0x23, 0x11, 0x07, 0xb4, 0xff } };

    IDXGIAdapter* adapter = nullptr;
    bool adapterOk = unkAdapter->QueryInterface(IID_PPV_ARGS(&adapter)) == S_OK;

    void* dxvkAdapter = nullptr;
    if (adapterOk && adapter->QueryInterface(guid, &dxvkAdapter) == S_OK)
    {
        State::Instance().isRunningOnDXVK = dxvkAdapter != nullptr;
        ((IDXGIAdapter*) dxvkAdapter)->Release();
    }

    if (adapterOk)
        adapter->Release();
}

void DxgiFactoryHooks::HookToFactory(IDXGIFactory* pFactory)
{
    if (pFactory == nullptr)
        return;

    LOG_DEBUG();

    IDXGIFactory* real = nullptr;
    if (!Util::CheckForRealObject(__FUNCTION__, pFactory, (IUnknown**) &real))
        real = pFactory;

    void** pFactoryVTable = *reinterpret_cast<void***>(real);

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    if (o_EnumAdapters == nullptr)
    {
        o_EnumAdapters = (PFN_EnumAdapters) pFactoryVTable[7];

        if (o_EnumAdapters != nullptr)
            DetourAttach(&(PVOID&) o_EnumAdapters, DxgiFactoryHooks::EnumAdapters);
    }

    if (o_CreateSwapChain == nullptr)
    {
        o_CreateSwapChain = (PFN_CreateSwapChain) pFactoryVTable[10];

        if (o_CreateSwapChain != nullptr)
            DetourAttach(&(PVOID&) o_CreateSwapChain, DxgiFactoryHooks::CreateSwapChain);
    }

    IDXGIFactory1* factory1 = nullptr;
    if (pFactory->QueryInterface(IID_PPV_ARGS(&factory1)) == S_OK)
    {
        factory1->Release();

        if (o_EnumAdapters1 == nullptr)
        {
            o_EnumAdapters1 = (PFN_EnumAdapters1) pFactoryVTable[12];

            if (o_EnumAdapters1 != nullptr)
                DetourAttach(&(PVOID&) o_EnumAdapters1, DxgiFactoryHooks::EnumAdapters1);
        }
    }

    IDXGIFactory2* factory2 = nullptr;
    if (pFactory->QueryInterface(IID_PPV_ARGS(&factory2)) == S_OK)
    {
        factory2->Release();

        if (o_CreateSwapChainForHwnd == nullptr)
        {
            o_CreateSwapChainForHwnd = (PFN_CreateSwapChainForHwnd) pFactoryVTable[15];

            if (o_CreateSwapChainForHwnd != nullptr)
                DetourAttach(&(PVOID&) o_CreateSwapChainForHwnd, DxgiFactoryHooks::CreateSwapChainForHwnd);
        }

        if (o_CreateSwapChainForCoreWindow == nullptr)
        {
            o_CreateSwapChainForCoreWindow = (PFN_CreateSwapChainForCoreWindow) pFactoryVTable[16];

            if (o_CreateSwapChainForCoreWindow != nullptr)
                DetourAttach(&(PVOID&) o_CreateSwapChainForCoreWindow, DxgiFactoryHooks::CreateSwapChainForCoreWindow);
        }
    }

    IDXGIFactory4* factory4 = nullptr;
    if (pFactory->QueryInterface(IID_PPV_ARGS(&factory4)) == S_OK)
    {
        factory4->Release();

        if (o_EnumAdapterByLuid == nullptr)
        {
            o_EnumAdapterByLuid = (PFN_EnumAdapterByLuid) pFactoryVTable[26];

            if (o_EnumAdapterByLuid != nullptr)
                DetourAttach(&(PVOID&) o_EnumAdapterByLuid, DxgiFactoryHooks::EnumAdapterByLuid);
        }
    }

    IDXGIFactory6* factory6 = nullptr;
    if (pFactory->QueryInterface(IID_PPV_ARGS(&factory6)) == S_OK)
    {
        factory6->Release();

        if (o_EnumAdapterByGpuPreference == nullptr)
        {
            o_EnumAdapterByGpuPreference = (PFN_EnumAdapterByGpuPreference) pFactoryVTable[29];

            if (o_EnumAdapterByGpuPreference != nullptr)
                DetourAttach(&(PVOID&) o_EnumAdapterByGpuPreference, DxgiFactoryHooks::EnumAdapterByGpuPreference);
        }
    }

    DetourTransactionCommit();
}

HRESULT DxgiFactoryHooks::CreateSwapChain(IDXGIFactory* realFactory, IUnknown* pDevice, DXGI_SWAP_CHAIN_DESC* pDesc,
                                          IDXGISwapChain** ppSwapChain)
{
    *ppSwapChain = nullptr;

    if (State::Instance().vulkanCreatingSC)
    {
        LOG_WARN("Vulkan is creating swapchain!");

        if (pDesc != nullptr)
            LOG_DEBUG("Width: {}, Height: {}, Format: {}, Count: {}, Hwnd: {:X}, Windowed: {}, SkipWrapping: {}",
                      pDesc->BufferDesc.Width, pDesc->BufferDesc.Height, (UINT) pDesc->BufferDesc.Format,
                      pDesc->BufferCount, (SIZE_T) pDesc->OutputWindow, pDesc->Windowed, _skipFGSwapChainCreation);

        State::Instance().skipDxgiLoadChecks = true;
        State::Instance().skipParentWrapping = true;
        auto res = o_CreateSwapChain(realFactory, pDevice, pDesc, ppSwapChain);
        State::Instance().skipParentWrapping = false;
        State::Instance().skipDxgiLoadChecks = false;

        return res;
    }

    if (pDevice == nullptr || pDesc == nullptr)
    {
        LOG_WARN("pDevice or pDesc is nullptr!");

        State::Instance().skipDxgiLoadChecks = true;
        State::Instance().skipParentWrapping = true;
        auto res = o_CreateSwapChain(realFactory, pDevice, pDesc, ppSwapChain);
        State::Instance().skipParentWrapping = false;
        State::Instance().skipDxgiLoadChecks = false;

        return res;
    }

    if (pDesc->BufferDesc.Height < 100 || pDesc->BufferDesc.Width < 100)
    {
        LOG_WARN("Overlay call!");

        State::Instance().skipDxgiLoadChecks = true;
        State::Instance().skipParentWrapping = true;
        auto res = o_CreateSwapChain(realFactory, pDevice, pDesc, ppSwapChain);
        State::Instance().skipParentWrapping = false;
        State::Instance().skipDxgiLoadChecks = false;

        return res;
    }

    LOG_DEBUG("Width: {}, Height: {}, Format: {}, Count: {}, Flags: {:X}, Hwnd: {:X}, Windowed: {}, SkipWrapping: {}",
              pDesc->BufferDesc.Width, pDesc->BufferDesc.Height, (UINT) pDesc->BufferDesc.Format, pDesc->BufferCount,
              pDesc->Flags, (SIZE_T) pDesc->OutputWindow, pDesc->Windowed, _skipFGSwapChainCreation);

    LOG_DEBUG("pDesc->BufferCount: {}", pDesc->BufferCount);
    LOG_DEBUG("pDesc->BufferDesc.Width: {}", pDesc->BufferDesc.Width);
    LOG_DEBUG("pDesc->BufferDesc.Format: {}", magic_enum::enum_name(pDesc->BufferDesc.Format));
    LOG_DEBUG("pDesc->BufferDesc.Height: {}", pDesc->BufferDesc.Height);
    LOG_DEBUG("pDesc->BufferDesc.RefreshRate.Denominator: {}", pDesc->BufferDesc.RefreshRate.Denominator);
    LOG_DEBUG("pDesc->BufferDesc.RefreshRate.Numerator: {}", pDesc->BufferDesc.RefreshRate.Numerator);
    LOG_DEBUG("pDesc->BufferDesc.Scaling: {}", magic_enum::enum_name(pDesc->BufferDesc.Scaling));
    LOG_DEBUG("pDesc->BufferDesc.ScanlineOrdering: {}", magic_enum::enum_name(pDesc->BufferDesc.ScanlineOrdering));
    LOG_DEBUG("pDesc->Windowed: {}", pDesc->Windowed);
    LOG_DEBUG("pDesc->SampleDesc.Count: {}", pDesc->SampleDesc.Count);
    LOG_DEBUG("pDesc->SampleDesc.Quality: {}", pDesc->SampleDesc.Quality);
    LOG_DEBUG("pDesc->BufferUsage: {}", (UINT) pDesc->BufferUsage);
    LOG_DEBUG("pDesc->Flags: {}", pDesc->Flags);
    LOG_DEBUG("pDesc->OutputWindow: {:X}", (UINT64) pDesc->OutputWindow);
    LOG_DEBUG("pDesc->SwapEffect: {}", magic_enum::enum_name(pDesc->SwapEffect));

    if (State::Instance().activeFgOutput == FGOutput::XeFG &&
        Config::Instance()->FGXeFGForceBorderless.value_or_default())
    {
        if (!pDesc->Windowed)
        {
            State::Instance().SCExclusiveFullscreen = true;
            pDesc->Windowed = true;
        }

        pDesc->BufferDesc.Scaling = DXGI_MODE_SCALING_STRETCHED;
    }

    // For vsync override
    if (!pDesc->Windowed)
    {
        LOG_INFO("Game is creating fullscreen swapchain, disabled V-Sync overrides");
        Config::Instance()->OverrideVsync.set_volatile_value(false);
    }

    if (Config::Instance()->OverrideVsync.value_or_default())
    {
        pDesc->SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        pDesc->Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

        if (pDesc->BufferCount < 2)
            pDesc->BufferCount = 2;
    }

    State::Instance().SCAllowTearing = (pDesc->Flags & DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING) > 0;
    State::Instance().SCLastFlags = pDesc->Flags;
    State::Instance().realExclusiveFullscreen = !pDesc->Windowed;

    // Check for SL proxy, get real queue
    ID3D12CommandQueue* cq = nullptr;
    IUnknown* real = nullptr;
    HRESULT FGSCResult = E_NOTIMPL;

    if (pDevice->QueryInterface(IID_PPV_ARGS(&cq)) == S_OK)
    {
        cq->Release();

        if (!Util::CheckForRealObject(__FUNCTION__, cq, &real))
            real = cq;

        State::Instance().currentCommandQueue = (ID3D12CommandQueue*) real;

        // Create FG SwapChain
        if (!_skipFGSwapChainCreation)
        {
            _skipFGSwapChainCreation = true;
            FGSCResult = FGHooks::CreateSwapChain(realFactory, real, pDesc, ppSwapChain);
            _skipFGSwapChainCreation = false;

            if (FGSCResult == S_OK)
            {
                State::Instance().currentSwapchainDesc = *pDesc;
                return FGSCResult;
            }
        }
    }
    else
    {
        LOG_INFO("Failed to get ID3D12CommandQueue from pDevice, creating Dx11 swapchain!");
    }

    HRESULT result = E_FAIL;

    // If FG is disabled or call is coming from FG library
    // Create the DXGI SwapChain and wrap it
    if (_skipFGSwapChainCreation || FGSCResult != S_OK)
    {
        // !_skipFGSwapChainCreation for preventing early enablement flags
        if (!_skipFGSwapChainCreation)
        {
            State::Instance().skipDxgiLoadChecks = true;

            if (Config::Instance()->FGDontUseSwapchainBuffers.value_or_default())
                State::Instance().skipHeapCapture = true;
        }

        State::Instance().skipParentWrapping = true;
        result = o_CreateSwapChain(realFactory, pDevice, pDesc, ppSwapChain);
        State::Instance().skipParentWrapping = false;

        if (!_skipFGSwapChainCreation)
        {
            if (Config::Instance()->FGDontUseSwapchainBuffers.value_or_default())
                State::Instance().skipHeapCapture = false;

            State::Instance().skipDxgiLoadChecks = false;
        }

        if (result == S_OK)
        {
            State::Instance().currentSwapchainDesc = *pDesc;

            // Check for SL proxy
            IDXGISwapChain* realSC = nullptr;
            if (!Util::CheckForRealObject(__FUNCTION__, *ppSwapChain, (IUnknown**) &realSC))
                realSC = *ppSwapChain;

            State::Instance().currentRealSwapchain = realSC;

            IUnknown* realDevice = nullptr;
            if (!Util::CheckForRealObject(__FUNCTION__, pDevice, (IUnknown**) &realDevice))
                realDevice = pDevice;

            if (Util::GetProcessWindow() == pDesc->OutputWindow)
            {
                State::Instance().screenWidth = static_cast<float>(pDesc->BufferDesc.Width);
                State::Instance().screenHeight = static_cast<float>(pDesc->BufferDesc.Height);
            }

            LOG_DEBUG("Created new swapchain: {0:X}, hWnd: {1:X}", (UINT64) *ppSwapChain, (UINT64) pDesc->OutputWindow);
            *ppSwapChain = new WrappedIDXGISwapChain4(realSC, realDevice, pDesc->OutputWindow, pDesc->Flags, false);

            // Set as currentSwapchain is FG is disabled
            if (!_skipFGSwapChainCreation)
                State::Instance().currentSwapchain = *ppSwapChain;

            State::Instance().currentWrappedSwapchain = *ppSwapChain;

            LOG_DEBUG("Created new WrappedIDXGISwapChain4: {:X}, pDevice: {:X}", (size_t) *ppSwapChain,
                      (size_t) pDevice);
        }
    }
    else
    {
        LOG_ERROR("CreateSwapChain failed: {:X}", (UINT) result);
    }

    return result;
}

HRESULT DxgiFactoryHooks::CreateSwapChainForHwnd(IDXGIFactory2* realFactory, IUnknown* pDevice, HWND hWnd,
                                                 DXGI_SWAP_CHAIN_DESC1* pDesc,
                                                 DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pFullscreenDesc,
                                                 IDXGIOutput* pRestrictToOutput, IDXGISwapChain1** ppSwapChain)
{
    *ppSwapChain = nullptr;

    static bool firstCall = static_cast<bool>(State::Instance().gameQuirks & GameQuirk::NoFSRFGFirstSwapchain);
    if (firstCall)
    {
        LOG_DEBUG("Skipping FG swapchain creation");
        _skipFGSwapChainCreation = true;
    }

    if (State::Instance().vulkanCreatingSC)
    {
        LOG_WARN("Vulkan is creating swapchain!");
        State::Instance().skipParentWrapping = true;
        State::Instance().skipDxgiLoadChecks = true;
        auto result = o_CreateSwapChainForHwnd(realFactory, pDevice, hWnd, pDesc, pFullscreenDesc, pRestrictToOutput,
                                               ppSwapChain);
        State::Instance().skipDxgiLoadChecks = false;
        State::Instance().skipParentWrapping = false;

        if (firstCall)
            _skipFGSwapChainCreation = false;

        return result;
    }

    if (pDevice == nullptr || pDesc == nullptr)
    {
        LOG_WARN("pDevice or pDesc is nullptr!");
        State::Instance().skipParentWrapping = true;
        State::Instance().skipDxgiLoadChecks = true;
        auto result = o_CreateSwapChainForHwnd(realFactory, pDevice, hWnd, pDesc, pFullscreenDesc, pRestrictToOutput,
                                               ppSwapChain);
        State::Instance().skipDxgiLoadChecks = false;
        State::Instance().skipParentWrapping = false;

        if (firstCall)
            _skipFGSwapChainCreation = false;

        return result;
    }

    if (pDesc->Height < 100 || pDesc->Width < 100)
    {
        LOG_WARN("Overlay call!");
        State::Instance().skipParentWrapping = true;
        State::Instance().skipDxgiLoadChecks = true;
        auto result = o_CreateSwapChainForHwnd(realFactory, pDevice, hWnd, pDesc, pFullscreenDesc, pRestrictToOutput,
                                               ppSwapChain);
        State::Instance().skipDxgiLoadChecks = false;
        State::Instance().skipParentWrapping = false;

        if (firstCall)
            _skipFGSwapChainCreation = false;

        return result;
    }

    LOG_DEBUG("Width: {}, Height: {}, Format: {}, Count: {}, Flags: {:X}, Hwnd: {:X}, SkipWrapping: {}", pDesc->Width,
              pDesc->Height, (UINT) pDesc->Format, pDesc->BufferCount, pDesc->Flags, (size_t) hWnd,
              _skipFGSwapChainCreation);

    if (pFullscreenDesc != nullptr)
        State::Instance().realExclusiveFullscreen = !pFullscreenDesc->Windowed;

    if (pFullscreenDesc != nullptr && State::Instance().activeFgOutput == FGOutput::XeFG &&
        Config::Instance()->FGXeFGForceBorderless.value_or_default())
    {
        if (!pFullscreenDesc->Windowed)
        {
            State::Instance().SCExclusiveFullscreen = true;
            pFullscreenDesc->Windowed = true;
        }

        pDesc->Scaling = DXGI_SCALING_STRETCH;
    }

    // For vsync override
    if (pFullscreenDesc != nullptr && !pFullscreenDesc->Windowed)
    {
        LOG_INFO("Game is creating fullscreen swapchain, disabled V-Sync overrides");
        Config::Instance()->OverrideVsync.set_volatile_value(false);
    }

    if (Config::Instance()->OverrideVsync.value_or_default())
    {
        pDesc->SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        pDesc->Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

        if (pDesc->BufferCount < 2)
            pDesc->BufferCount = 2;
    }

    State::Instance().SCAllowTearing = (pDesc->Flags & DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING) > 0;
    State::Instance().SCLastFlags = pDesc->Flags;
    State::Instance().realExclusiveFullscreen = pFullscreenDesc != nullptr && !pFullscreenDesc->Windowed;

    // Check for SL proxy, get real queue
    ID3D12CommandQueue* cq = nullptr;
    IUnknown* real = nullptr;
    HRESULT FGSCResult = E_NOTIMPL;

    if (pDevice->QueryInterface(IID_PPV_ARGS(&cq)) == S_OK)
    {
        cq->Release();

        if (!Util::CheckForRealObject(__FUNCTION__, cq, &real))
            real = cq;

        State::Instance().currentCommandQueue = (ID3D12CommandQueue*) real;

        // Create FG SwapChain
        if (!_skipFGSwapChainCreation)
        {
            _skipFGSwapChainCreation = true;
            FGSCResult = FGHooks::CreateSwapChainForHwnd(realFactory, real, hWnd, pDesc, pFullscreenDesc,
                                                         pRestrictToOutput, ppSwapChain);
            _skipFGSwapChainCreation = false;

            if (FGSCResult == S_OK)
            {
                ((IDXGISwapChain*) *ppSwapChain)->GetDesc(&State::Instance().currentSwapchainDesc);
                return FGSCResult;
            }
        }
    }
    else
    {
        LOG_INFO("Failed to get ID3D12CommandQueue from pDevice, creating Dx11 swapchain!");
    }

    HRESULT result = E_FAIL;

    // If FG is disabled or call is coming from FG library
    // Create the DXGI SwapChain and wrap it
    if (_skipFGSwapChainCreation || FGSCResult != S_OK)
    {

        // !_skipFGSwapChainCreation for preventing early enablement flags
        if (!_skipFGSwapChainCreation)
        {
            State::Instance().skipDxgiLoadChecks = true;

            if (Config::Instance()->FGDontUseSwapchainBuffers.value_or_default())
                State::Instance().skipHeapCapture = true;
        }

        State::Instance().skipParentWrapping = true;
        result = o_CreateSwapChainForHwnd(realFactory, pDevice, hWnd, pDesc, pFullscreenDesc, pRestrictToOutput,
                                          ppSwapChain);

        State::Instance().skipParentWrapping = false;

        if (!_skipFGSwapChainCreation)
        {
            if (Config::Instance()->FGDontUseSwapchainBuffers.value_or_default())
                State::Instance().skipHeapCapture = false;

            State::Instance().skipDxgiLoadChecks = false;
        }

        if (result == S_OK)
        {
            // check for SL proxy
            IDXGISwapChain1* realSC = nullptr;
            if (!Util::CheckForRealObject(__FUNCTION__, *ppSwapChain, (IUnknown**) &realSC))
                realSC = *ppSwapChain;

            State::Instance().currentRealSwapchain = realSC;

            IUnknown* readDevice = nullptr;
            if (!Util::CheckForRealObject(__FUNCTION__, pDevice, (IUnknown**) &readDevice))
                readDevice = pDevice;

            if (Util::GetProcessWindow() == hWnd)
            {
                State::Instance().screenWidth = static_cast<float>(pDesc->Width);
                State::Instance().screenHeight = static_cast<float>(pDesc->Height);
            }

            realSC->GetDesc(&State::Instance().currentSwapchainDesc);

            LOG_DEBUG("Created new swapchain: {0:X}, hWnd: {1:X}", (uintptr_t) *ppSwapChain, (uintptr_t) hWnd);
            *ppSwapChain = new WrappedIDXGISwapChain4(realSC, readDevice, hWnd, pDesc->Flags, false);

            LOG_DEBUG("Created new WrappedIDXGISwapChain4: {0:X}, pDevice: {1:X}", (uintptr_t) *ppSwapChain,
                      (uintptr_t) pDevice);

            if (!_skipFGSwapChainCreation)
                State::Instance().currentSwapchain = *ppSwapChain;

            State::Instance().currentWrappedSwapchain = *ppSwapChain;
        }
        else
        {
            LOG_ERROR("CreateSwapChainForHwnd failed: {:X}", (UINT) result);
        }
    }

    if (firstCall)
    {
        LOG_DEBUG("Unsetting skip FG swapchain creation");
        _skipFGSwapChainCreation = false;
        firstCall = false;
    }

    return result;
}

HRESULT DxgiFactoryHooks::CreateSwapChainForCoreWindow(IDXGIFactory2* realFactory, IUnknown* pDevice, IUnknown* pWindow,
                                                       DXGI_SWAP_CHAIN_DESC1* pDesc, IDXGIOutput* pRestrictToOutput,
                                                       IDXGISwapChain1** ppSwapChain)
{
    if (State::Instance().vulkanCreatingSC)
    {
        LOG_WARN("Vulkan is creating swapchain!");

        if (pDesc != nullptr)
            LOG_DEBUG("Width: {}, Height: {}, Format: {}, Flags: {:X}, Count: {}, SkipWrapping: {}", pDesc->Width,
                      pDesc->Height, (UINT) pDesc->Format, pDesc->Flags, pDesc->BufferCount, _skipFGSwapChainCreation);

        State::Instance().skipDxgiLoadChecks = true;
        auto res = realFactory->CreateSwapChainForCoreWindow(pDevice, pWindow, pDesc, pRestrictToOutput, ppSwapChain);
        State::Instance().skipDxgiLoadChecks = false;
        return res;
    }

    if (pDevice == nullptr || pDesc == nullptr)
    {
        LOG_WARN("pDevice or pDesc is nullptr!");
        State::Instance().skipDxgiLoadChecks = true;
        auto res = realFactory->CreateSwapChainForCoreWindow(pDevice, pWindow, pDesc, pRestrictToOutput, ppSwapChain);
        State::Instance().skipDxgiLoadChecks = false;
        return res;
    }

    if (pDesc->Height < 100 || pDesc->Width < 100)
    {
        LOG_WARN("Overlay call!");
        State::Instance().skipDxgiLoadChecks = true;
        auto res = realFactory->CreateSwapChainForCoreWindow(pDevice, pWindow, pDesc, pRestrictToOutput, ppSwapChain);
        State::Instance().skipDxgiLoadChecks = false;
        return res;
    }

    LOG_DEBUG("Width: {}, Height: {}, Format: {}, Count: {}, SkipWrapping: {}", pDesc->Width, pDesc->Height,
              (UINT) pDesc->Format, pDesc->BufferCount, _skipFGSwapChainCreation);

    // For vsync override
    if (Config::Instance()->OverrideVsync.value_or_default())
    {
        pDesc->SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        pDesc->Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

        if (pDesc->BufferCount < 2)
            pDesc->BufferCount = 2;
    }

    State::Instance().SCAllowTearing = (pDesc->Flags & DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING) > 0;
    State::Instance().SCLastFlags = pDesc->Flags;
    State::Instance().realExclusiveFullscreen = false;

    ID3D12CommandQueue* cq = nullptr;
    IUnknown* real = nullptr;
    if (pDevice->QueryInterface(IID_PPV_ARGS(&cq)) == S_OK)
    {
        cq->Release();

        if (!Util::CheckForRealObject(__FUNCTION__, cq, &real))
            real = cq;

        State::Instance().currentCommandQueue = (ID3D12CommandQueue*) real;
    }

    State::Instance().skipDxgiLoadChecks = true;
    auto result = o_CreateSwapChainForCoreWindow(realFactory, pDevice, pWindow, pDesc, pRestrictToOutput, ppSwapChain);
    State::Instance().skipDxgiLoadChecks = false;

    if (result == S_OK)
    {
        // check for SL proxy
        IDXGISwapChain* realSC = nullptr;
        if (!Util::CheckForRealObject(__FUNCTION__, *ppSwapChain, (IUnknown**) &realSC))
            realSC = *ppSwapChain;

        State::Instance().currentRealSwapchain = realSC;

        IUnknown* readDevice = nullptr;
        if (!Util::CheckForRealObject(__FUNCTION__, pDevice, (IUnknown**) &readDevice))
            readDevice = pDevice;

        realSC->GetDesc(&State::Instance().currentSwapchainDesc);

        State::Instance().screenWidth = static_cast<float>(pDesc->Width);
        State::Instance().screenHeight = static_cast<float>(pDesc->Height);

        LOG_DEBUG("Created new swapchain: {0:X}, hWnd: {1:X}", (UINT64) *ppSwapChain, (UINT64) pWindow);
        *ppSwapChain = new WrappedIDXGISwapChain4(realSC, readDevice, (HWND) pWindow, pDesc->Flags, true);

        if (!_skipFGSwapChainCreation)
            State::Instance().currentSwapchain = *ppSwapChain;

        State::Instance().currentWrappedSwapchain = *ppSwapChain;

        LOG_DEBUG("Created new WrappedIDXGISwapChain4: {0:X}, pDevice: {1:X}", (UINT64) *ppSwapChain, (UINT64) pDevice);
    }

    return result;
}

HRESULT DxgiFactoryHooks::EnumAdapters(IDXGIFactory* realFactory, UINT Adapter, IDXGIAdapter** ppAdapter)
{
    HRESULT result = S_OK;

    if (!_skipHighPerfCheck && Config::Instance()->PreferDedicatedGpu.value_or_default())
    {
        if (Config::Instance()->PreferFirstDedicatedGpu.value_or_default() && Adapter > 0)
        {
            LOG_DEBUG("{}, returning not found", Adapter);
            return DXGI_ERROR_NOT_FOUND;
        }

        IDXGIFactory6* factory6 = nullptr;
        if (realFactory->QueryInterface(IID_PPV_ARGS(&factory6)) == S_OK && factory6 != nullptr)
        {
            LOG_DEBUG("Trying to select high performance adapter ({})", Adapter);

            _skipHighPerfCheck = true;
            State::Instance().skipDxgiLoadChecks = true;

            result = o_EnumAdapterByGpuPreference(factory6, Adapter, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                                                  __uuidof(IDXGIAdapter1), (IUnknown**) ppAdapter);

            State::Instance().skipDxgiLoadChecks = false;
            _skipHighPerfCheck = false;

            if (result != S_OK)
            {
                LOG_ERROR("Can't get high performance adapter: {:X}, fallback to standard method", Adapter);
                State::Instance().skipDxgiLoadChecks = true;
                result = o_EnumAdapters(realFactory, Adapter, (IUnknown**) ppAdapter);
                State::Instance().skipDxgiLoadChecks = false;
            }

            if (result == S_OK)
            {
                DXGI_ADAPTER_DESC desc;
                State::Instance().skipSpoofing = true;

                if ((*ppAdapter)->GetDesc(&desc) == S_OK)
                {
                    std::wstring name(desc.Description);
                    LOG_DEBUG("Adapter ({}) will be used", wstring_to_string(name));
                }
                else
                {
                    LOG_ERROR("Can't get adapter description!");
                }

                State::Instance().skipSpoofing = false;
            }

            factory6->Release();
        }
        else
        {
            State::Instance().skipDxgiLoadChecks = true;
            result = o_EnumAdapters(realFactory, Adapter, (IUnknown**) ppAdapter);
            State::Instance().skipDxgiLoadChecks = false;
        }
    }
    else
    {
        State::Instance().skipDxgiLoadChecks = true;
        result = o_EnumAdapters(realFactory, Adapter, (IUnknown**) ppAdapter);
        State::Instance().skipDxgiLoadChecks = false;
    }

    if (result == S_OK)
    {
        CheckAdapter(*ppAdapter);
        DxgiSpoofing::AttachToAdapter(*ppAdapter);
    }

#if _DEBUG
    LOG_TRACE("result: {:X}, Adapter: {}, pAdapter: {:X}", (UINT) result, Adapter, (uintptr_t) *ppAdapter);
#endif

    return result;
}

HRESULT DxgiFactoryHooks::EnumAdapters1(IDXGIFactory1* realFactory, UINT Adapter, IDXGIAdapter1** ppAdapter)
{
    HRESULT result = S_OK;

    if (!_skipHighPerfCheck && Config::Instance()->PreferDedicatedGpu.value_or_default())
    {
        LOG_WARN("High perf GPU selection");

        if (Config::Instance()->PreferFirstDedicatedGpu.value_or_default() && Adapter > 0)
        {
            LOG_DEBUG("{}, returning not found", Adapter);
            return DXGI_ERROR_NOT_FOUND;
        }

        IDXGIFactory6* factory6 = nullptr;
        if (realFactory->QueryInterface(IID_PPV_ARGS(&factory6)) == S_OK && factory6 != nullptr)
        {
            LOG_DEBUG("Trying to select high performance adapter ({})", Adapter);

            _skipHighPerfCheck = true;
            State::Instance().skipDxgiLoadChecks = true;

            result = o_EnumAdapterByGpuPreference(factory6, Adapter, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                                                  __uuidof(IDXGIAdapter1), (IUnknown**) ppAdapter);

            State::Instance().skipDxgiLoadChecks = false;
            _skipHighPerfCheck = false;

            if (result != S_OK)
            {
                LOG_ERROR("Can't get high performance adapter: {:X}, fallback to standard method", Adapter);
                State::Instance().skipDxgiLoadChecks = true;
                result = o_EnumAdapters1(realFactory, Adapter, (IUnknown**) ppAdapter);
                State::Instance().skipDxgiLoadChecks = false;
            }

            if (result == S_OK)
            {
                DXGI_ADAPTER_DESC desc;
                State::Instance().skipSpoofing = true;
                if ((*ppAdapter)->GetDesc(&desc) == S_OK)
                {
                    std::wstring name(desc.Description);
                    LOG_DEBUG("High performance adapter ({}) will be used", wstring_to_string(name));
                }
                else
                {
                    LOG_DEBUG("High performance adapter (Can't get description!) will be used");
                }
                State::Instance().skipSpoofing = false;
            }

            factory6->Release();
        }
        else
        {
            State::Instance().skipDxgiLoadChecks = true;
            result = o_EnumAdapters1(realFactory, Adapter, (IUnknown**) ppAdapter);
            State::Instance().skipDxgiLoadChecks = false;
        }
    }
    else
    {
        State::Instance().skipDxgiLoadChecks = true;
        result = o_EnumAdapters1(realFactory, Adapter, (IUnknown**) ppAdapter);
        State::Instance().skipDxgiLoadChecks = false;
    }

    if (result == S_OK)
    {
        CheckAdapter(*ppAdapter);
        DxgiSpoofing::AttachToAdapter(*ppAdapter);
    }

#if _DEBUG
    LOG_TRACE("result: {:X}, Adapter: {}, pAdapter: {:X}", (UINT) result, Adapter, (uintptr_t) *ppAdapter);
#endif

    return result;
}

HRESULT DxgiFactoryHooks::EnumAdapterByLuid(IDXGIFactory4* realFactory, LUID AdapterLuid, REFIID riid,
                                            void** ppvAdapter)
{
    State::Instance().skipDxgiLoadChecks = true;

    auto result = o_EnumAdapterByLuid(realFactory, AdapterLuid, riid, (IUnknown**) ppvAdapter);

    State::Instance().skipDxgiLoadChecks = false;

    if (result == S_OK)
    {
        CheckAdapter((IUnknown*) *ppvAdapter);
        DxgiSpoofing::AttachToAdapter((IUnknown*) *ppvAdapter);
    }

#if _DEBUG
    LOG_TRACE("result: {:X}, pAdapter: {:X}", (UINT) result, (uintptr_t) *ppvAdapter);
#endif

    return result;
}

HRESULT DxgiFactoryHooks::EnumAdapterByGpuPreference(IDXGIFactory6* realFactory, UINT Adapter,
                                                     DXGI_GPU_PREFERENCE GpuPreference, REFIID riid, void** ppvAdapter)
{
    State::Instance().skipDxgiLoadChecks = true;

    auto result = o_EnumAdapterByGpuPreference(realFactory, Adapter, GpuPreference, riid, (IUnknown**) ppvAdapter);

    State::Instance().skipDxgiLoadChecks = false;

    if (result == S_OK)
    {
        CheckAdapter((IUnknown*) *ppvAdapter);
        DxgiSpoofing::AttachToAdapter((IUnknown*) *ppvAdapter);
    }

#if _DEBUG
    LOG_TRACE("result: {:X}, Adapter: {}, pAdapter: {:X}", (UINT) result, Adapter, (uintptr_t) *ppvAdapter);
#endif

    return result;
}
