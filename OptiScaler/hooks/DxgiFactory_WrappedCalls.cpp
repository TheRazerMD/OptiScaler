#include "DxgiFactory_WrappedCalls.h"

#include "FG_Hooks.h"

#include <Config.h>

#include <spoofing/Dxgi_Spoofing.h>
#include <wrapped/wrapped_swapchain.h>

// #define DETAILED_SC_LOGS

#ifdef DETAILED_SC_LOGS
#include <magic_enum.hpp>
#endif

void DxgiFactoryWrappedCalls::CheckAdapter(IUnknown* unkAdapter)
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

HRESULT DxgiFactoryWrappedCalls::CreateSwapChain(IDXGIFactory* realFactory, WrappedIDXGIFactory7* wrappedFactory,
                                                 IUnknown* pDevice, DXGI_SWAP_CHAIN_DESC* pDesc,
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
        auto res = realFactory->CreateSwapChain(pDevice, pDesc, ppSwapChain);
        State::Instance().skipParentWrapping = false;
        State::Instance().skipDxgiLoadChecks = false;

        return res;
    }

    if (pDevice == nullptr || pDesc == nullptr)
    {
        LOG_WARN("pDevice or pDesc is nullptr!");

        State::Instance().skipDxgiLoadChecks = true;
        State::Instance().skipParentWrapping = true;
        auto res = realFactory->CreateSwapChain(pDevice, pDesc, ppSwapChain);
        State::Instance().skipParentWrapping = false;
        State::Instance().skipDxgiLoadChecks = false;

        return res;
    }

    if (pDesc->BufferDesc.Height < 100 || pDesc->BufferDesc.Width < 100)
    {
        LOG_WARN("Overlay call!");

        State::Instance().skipDxgiLoadChecks = true;
        State::Instance().skipParentWrapping = true;
        auto res = realFactory->CreateSwapChain(pDevice, pDesc, ppSwapChain);
        State::Instance().skipParentWrapping = false;
        State::Instance().skipDxgiLoadChecks = false;

        return res;
    }

    LOG_DEBUG("Width: {}, Height: {}, Format: {}, Count: {}, Flags: {:X}, Hwnd: {:X}, Windowed: {}, SkipWrapping: {}",
              pDesc->BufferDesc.Width, pDesc->BufferDesc.Height, (UINT) pDesc->BufferDesc.Format, pDesc->BufferCount,
              pDesc->Flags, (SIZE_T) pDesc->OutputWindow, pDesc->Windowed, _skipFGSwapChainCreation);

    if (State::Instance().activeFgOutput == FGOutput::XeFG &&
        Config::Instance()->FGXeFGForceBorderless.value_or_default())
    {
        if (!pDesc->Windowed)
        {
            State::Instance().SCExclusiveFullscreen = true;
            pDesc->Windowed = true;
        }

        pDesc->Flags &= ~DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
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

#ifdef DETAILED_SC_LOGS
    LOG_TRACE("pDesc->BufferCount: {}", pDesc->BufferCount);
    LOG_TRACE("pDesc->BufferDesc.Format: {}", magic_enum::enum_name(pDesc->BufferDesc.Format));
    LOG_TRACE("pDesc->BufferDesc.Height: {}", pDesc->BufferDesc.Height);
    LOG_TRACE("pDesc->BufferDesc.RefreshRate.Denominator: {}", pDesc->BufferDesc.RefreshRate.Denominator);
    LOG_TRACE("pDesc->BufferDesc.RefreshRate.Numerator: {}", pDesc->BufferDesc.RefreshRate.Numerator);
    LOG_TRACE("pDesc->BufferDesc.Scaling: {}", magic_enum::enum_name(pDesc->BufferDesc.Scaling));
    LOG_TRACE("pDesc->BufferDesc.ScanlineOrdering: {}", magic_enum::enum_name(pDesc->BufferDesc.ScanlineOrdering));
    LOG_TRACE("pDesc->BufferDesc.Width: {}", pDesc->BufferDesc.Width);
    LOG_TRACE("pDesc->BufferUsage: {}", pDesc->BufferUsage);
    LOG_TRACE("pDesc->Flags: {}", pDesc->Flags);
    LOG_TRACE("pDesc->OutputWindow: {}", (UINT64) pDesc->OutputWindow);
    LOG_TRACE("pDesc->SampleDesc.Count: {}", pDesc->SampleDesc.Count);
    LOG_TRACE("pDesc->SampleDesc.Quality: {}", pDesc->SampleDesc.Quality);
    LOG_TRACE("pDesc->SwapEffect: {}", magic_enum::enum_name(pDesc->SwapEffect));
    LOG_TRACE("pDesc->Windowed: {}", pDesc->Windowed);
#endif //

    // Check for SL proxy, get real queue
    ID3D12CommandQueue* real = nullptr;
    if (!Util::CheckForRealObject(__FUNCTION__, pDevice, (IUnknown**) &real))
        real = (ID3D12CommandQueue*) pDevice;

    State::Instance().currentCommandQueue = real;

    // Create FG SwapChain
    HRESULT FGSCResult = E_NOTIMPL;
    if (!_skipFGSwapChainCreation)
    {
        _skipFGSwapChainCreation = true;
        FGSCResult = FGHooks::CreateSwapChain(wrappedFactory, real, pDesc, ppSwapChain);
        _skipFGSwapChainCreation = false;

        if (FGSCResult == S_OK)
        {
            State::Instance().currentSwapchainDesc = *pDesc;
            return FGSCResult;
        }
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
        result = realFactory->CreateSwapChain(pDevice, pDesc, ppSwapChain);
        State::Instance().skipParentWrapping = false;

        if (!_skipFGSwapChainCreation)
        {
            if (Config::Instance()->FGDontUseSwapchainBuffers.value_or_default())
                State::Instance().skipHeapCapture = false;

            State::Instance().skipDxgiLoadChecks = false;
        }

        if (result == S_OK)
        {
            // Check for SL proxy
            IDXGISwapChain* realSC = nullptr;
            if (!Util::CheckForRealObject(__FUNCTION__, *ppSwapChain, (IUnknown**) &realSC))
                realSC = *ppSwapChain;

            State::Instance().currentRealSwapchain = realSC;
            State::Instance().currentSwapchainDesc = *pDesc;

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

            LOG_DEBUG("Created new WrappedIDXGISwapChain4: {:X}, pDevice: {:X}", (uintptr_t) *ppSwapChain,
                      (uintptr_t) pDevice);
        }
    }

    return result;
}

HRESULT DxgiFactoryWrappedCalls::CreateSwapChainForHwnd(IDXGIFactory2* realFactory,
                                                        WrappedIDXGIFactory7* wrappedFactory, IUnknown* pDevice,
                                                        HWND hWnd, DXGI_SWAP_CHAIN_DESC1* pDesc,
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
        auto res =
            realFactory->CreateSwapChainForHwnd(pDevice, hWnd, pDesc, pFullscreenDesc, pRestrictToOutput, ppSwapChain);
        State::Instance().skipDxgiLoadChecks = false;
        State::Instance().skipParentWrapping = false;

        if (firstCall)
            _skipFGSwapChainCreation = false;

        return res;
    }

    if (pDevice == nullptr || pDesc == nullptr)
    {
        LOG_WARN("pDevice or pDesc is nullptr!");
        State::Instance().skipParentWrapping = true;
        State::Instance().skipDxgiLoadChecks = true;
        auto res =
            realFactory->CreateSwapChainForHwnd(pDevice, hWnd, pDesc, pFullscreenDesc, pRestrictToOutput, ppSwapChain);
        State::Instance().skipDxgiLoadChecks = false;
        State::Instance().skipParentWrapping = false;

        if (firstCall)
            _skipFGSwapChainCreation = false;

        return res;
    }

    if (pDesc->Height < 100 || pDesc->Width < 100)
    {
        LOG_WARN("Overlay call!");
        State::Instance().skipParentWrapping = true;
        State::Instance().skipDxgiLoadChecks = true;
        auto res =
            realFactory->CreateSwapChainForHwnd(pDevice, hWnd, pDesc, pFullscreenDesc, pRestrictToOutput, ppSwapChain);
        State::Instance().skipDxgiLoadChecks = false;
        State::Instance().skipParentWrapping = false;

        if (firstCall)
            _skipFGSwapChainCreation = false;

        return res;
    }

    LOG_DEBUG("Width: {}, Height: {}, Format: {}, Count: {}, Flags: {:X}, Hwnd: {:X}, SkipWrapping: {}", pDesc->Width,
              pDesc->Height, (UINT) pDesc->Format, pDesc->BufferCount, pDesc->Flags, (uintptr_t) hWnd,
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

        pDesc->Flags &= ~DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
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

#ifdef DETAILED_SC_LOGS
    LOG_TRACE("pDesc->AlphaMode : {}", magic_enum::enum_name(pDesc->AlphaMode));
    LOG_TRACE("pDesc->BufferCount : {}", pDesc->BufferCount);
    LOG_TRACE("pDesc->BufferUsage : {}", pDesc->BufferUsage);
    LOG_TRACE("pDesc->Flags : {}", pDesc->Flags);
    LOG_TRACE("pDesc->Format : {}", magic_enum::enum_name(pDesc->Format));
    LOG_TRACE("pDesc->Height : {}", pDesc->Height);
    LOG_TRACE("pDesc->SampleDesc.Count : {}", pDesc->SampleDesc.Count);
    LOG_TRACE("pDesc->SampleDesc.Quality : {}", pDesc->SampleDesc.Quality);
    LOG_TRACE("pDesc->Scaling : {}", magic_enum::enum_name(pDesc->Scaling));
    LOG_TRACE("pDesc->Stereo : {}", pDesc->Stereo);

    if (pFullscreenDesc != nullptr)
    {
        LOG_TRACE("pFullscreenDesc->RefreshRate.Denominator : {}", pFullscreenDesc->RefreshRate.Denominator);
        LOG_TRACE("pFullscreenDesc->RefreshRate.Numerator : {}", pFullscreenDesc->RefreshRate.Numerator);
        LOG_TRACE("pFullscreenDesc->Scaling : {}", magic_enum::enum_name(pFullscreenDesc->Scaling));
        LOG_TRACE("pFullscreenDesc->ScanlineOrdering : {}", magic_enum::enum_name(pFullscreenDesc->ScanlineOrdering));
        LOG_TRACE("pFullscreenDesc->Windowed : {}", pFullscreenDesc->Windowed);
    }
#endif

    // Check for SL proxy, get real queue
    ID3D12CommandQueue* real = nullptr;
    if (!Util::CheckForRealObject(__FUNCTION__, pDevice, (IUnknown**) &real))
        real = (ID3D12CommandQueue*) pDevice;

    State::Instance().currentCommandQueue = real;

    // Create FG SwapChain
    HRESULT FGSCResult = E_NOTIMPL;
    if (!_skipFGSwapChainCreation)
    {
        _skipFGSwapChainCreation = true;
        FGSCResult = FGHooks::CreateSwapChainForHwnd(wrappedFactory, real, hWnd, pDesc, pFullscreenDesc,
                                                     pRestrictToOutput, ppSwapChain);
        _skipFGSwapChainCreation = false;

        if (FGSCResult == S_OK)
        {
            ((IDXGISwapChain*) *ppSwapChain)->GetDesc(&State::Instance().currentSwapchainDesc);
            return FGSCResult;
        }
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
        result =
            realFactory->CreateSwapChainForHwnd(pDevice, hWnd, pDesc, pFullscreenDesc, pRestrictToOutput, ppSwapChain);

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
            realSC->GetDesc(&State::Instance().currentSwapchainDesc);

            IUnknown* readDevice = nullptr;
            if (!Util::CheckForRealObject(__FUNCTION__, pDevice, (IUnknown**) &readDevice))
                readDevice = pDevice;

            if (Util::GetProcessWindow() == hWnd)
            {
                State::Instance().screenWidth = static_cast<float>(pDesc->Width);
                State::Instance().screenHeight = static_cast<float>(pDesc->Height);
            }

            LOG_DEBUG("Created new swapchain: {0:X}, hWnd: {1:X}", (UINT64) *ppSwapChain, (UINT64) hWnd);
            *ppSwapChain = new WrappedIDXGISwapChain4(realSC, readDevice, hWnd, pDesc->Flags, false);

            LOG_DEBUG("Created new WrappedIDXGISwapChain4: {0:X}, pDevice: {1:X}", (UINT64) *ppSwapChain,
                      (UINT64) pDevice);

            if (!_skipFGSwapChainCreation)
                State::Instance().currentSwapchain = *ppSwapChain;

            State::Instance().currentWrappedSwapchain = *ppSwapChain;
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

HRESULT DxgiFactoryWrappedCalls::CreateSwapChainForCoreWindow(IDXGIFactory2* realFactory, IUnknown* pDevice,
                                                              IUnknown* pWindow, DXGI_SWAP_CHAIN_DESC1* pDesc,
                                                              IDXGIOutput* pRestrictToOutput,
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

    ID3D12CommandQueue* realQ = nullptr;
    if (!Util::CheckForRealObject(__FUNCTION__, pDevice, (IUnknown**) &realQ))
        realQ = (ID3D12CommandQueue*) pDevice;

    State::Instance().currentCommandQueue = realQ;

    State::Instance().skipDxgiLoadChecks = true;
    auto result = realFactory->CreateSwapChainForCoreWindow(pDevice, pWindow, pDesc, pRestrictToOutput, ppSwapChain);
    State::Instance().skipDxgiLoadChecks = false;

    if (result == S_OK)
    {
        // check for SL proxy
        IDXGISwapChain* realSC = nullptr;
        if (!Util::CheckForRealObject(__FUNCTION__, *ppSwapChain, (IUnknown**) &realSC))
            realSC = *ppSwapChain;

        State::Instance().currentRealSwapchain = realSC;
        realSC->GetDesc(&State::Instance().currentSwapchainDesc);

        IUnknown* readDevice = nullptr;
        if (!Util::CheckForRealObject(__FUNCTION__, pDevice, (IUnknown**) &readDevice))
            readDevice = pDevice;

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

HRESULT DxgiFactoryWrappedCalls::EnumAdapters(IDXGIFactory* realFactory, UINT Adapter, IDXGIAdapter** ppAdapter)
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

            result = factory6->EnumAdapterByGpuPreference(Adapter, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                                                          __uuidof(IDXGIAdapter1), (void**) ppAdapter);

            State::Instance().skipDxgiLoadChecks = false;
            _skipHighPerfCheck = false;

            if (result != S_OK)
            {
                LOG_ERROR("Can't get high performance adapter: {:X}, fallback to standard method", Adapter);
                result = realFactory->EnumAdapters(Adapter, ppAdapter);
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
            result = realFactory->EnumAdapters(Adapter, ppAdapter);
            State::Instance().skipDxgiLoadChecks = false;
        }
    }
    else
    {
        State::Instance().skipDxgiLoadChecks = true;
        result = realFactory->EnumAdapters(Adapter, ppAdapter);
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

HRESULT DxgiFactoryWrappedCalls::EnumAdapters1(IDXGIFactory1* realFactory, UINT Adapter, IDXGIAdapter1** ppAdapter)
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

            result = factory6->EnumAdapterByGpuPreference(Adapter, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                                                          __uuidof(IDXGIAdapter1), (void**) ppAdapter);

            State::Instance().skipDxgiLoadChecks = false;
            _skipHighPerfCheck = false;

            if (result != S_OK)
            {
                LOG_ERROR("Can't get high performance adapter: {:X}, fallback to standard method", Adapter);
                result = realFactory->EnumAdapters1(Adapter, ppAdapter);
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
            result = realFactory->EnumAdapters1(Adapter, ppAdapter);
            State::Instance().skipDxgiLoadChecks = false;
        }
    }
    else
    {
        State::Instance().skipDxgiLoadChecks = true;
        result = realFactory->EnumAdapters1(Adapter, ppAdapter);
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

HRESULT DxgiFactoryWrappedCalls::EnumAdapterByLuid(IDXGIFactory4* realFactory, LUID AdapterLuid, REFIID riid,
                                                   void** ppvAdapter)
{
    State::Instance().skipDxgiLoadChecks = true;

    auto result = realFactory->EnumAdapterByLuid(AdapterLuid, riid, ppvAdapter);

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

HRESULT DxgiFactoryWrappedCalls::EnumAdapterByGpuPreference(IDXGIFactory6* realFactory, UINT Adapter,
                                                            DXGI_GPU_PREFERENCE GpuPreference, REFIID riid,
                                                            void** ppvAdapter)
{
    State::Instance().skipDxgiLoadChecks = true;

    auto result = realFactory->EnumAdapterByGpuPreference(Adapter, GpuPreference, riid, ppvAdapter);

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
