#include "XeFG_Dx12.h"

#include <hooks/HooksDx.h>
#include <hudfix/Hudfix_Dx12.h>
#include <menu/menu_overlay_dx.h>
#include <resource_tracking/ResTrack_dx12.h>

#include <magic_enum.hpp>

void XeFG_Dx12::xefgLogCallback(const char* message, xefg_swapchain_logging_level_t level, void* userData)
{
    switch (level)
    {
    case XEFG_SWAPCHAIN_LOGGING_LEVEL_DEBUG:
        spdlog::debug("XeFG Log: {}", message);
        return;

    case XEFG_SWAPCHAIN_LOGGING_LEVEL_INFO:
        spdlog::info("XeFG Log: {}", message);
        return;

    case XEFG_SWAPCHAIN_LOGGING_LEVEL_WARNING:
        spdlog::warn("XeFG Log: {}", message);
        return;

    default:
        spdlog::error("XeFG Log: {}", message);
        return;
    }
}

bool XeFG_Dx12::CreateSwapchainContext(ID3D12Device* device)
{
    if (XeFGProxy::Module() == nullptr && !XeFGProxy::InitXeFG())
    {
        LOG_ERROR("XeFG proxy can't find libxess_fg.dll!");
        return false;
    }

    State::Instance().skipSpoofing = true;
    auto result = XeFGProxy::D3D12CreateContext()(device, &_swapChainContext);
    State::Instance().skipSpoofing = false;

    if (result != XEFG_SWAPCHAIN_RESULT_SUCCESS)
    {
        LOG_ERROR("D3D12CreateContext error: {} ({})", magic_enum::enum_name(result), (UINT) result);
        return false;
    }

    LOG_INFO("XeFG context created");
    result = XeFGProxy::SetLoggingCallback()(_swapChainContext, XEFG_SWAPCHAIN_LOGGING_LEVEL_DEBUG, xefgLogCallback,
                                             nullptr);

    if (result != XEFG_SWAPCHAIN_RESULT_SUCCESS)
    {
        LOG_ERROR("SetLoggingCallback error: {} ({})", magic_enum::enum_name(result), (UINT) result);
    }

    if (XeLLProxy::Context() == nullptr)
        XeLLProxy::CreateContext(device);

    if (XeLLProxy::Context() != nullptr)
    {
        xell_sleep_params_t sleepParams = {};
        sleepParams.bLowLatencyMode = true;
        sleepParams.bLowLatencyBoost = false;
        sleepParams.minimumIntervalUs = 0;

        auto xellResult = XeLLProxy::SetSleepMode()(XeLLProxy::Context(), &sleepParams);
        if (xellResult != XELL_RESULT_SUCCESS)
        {
            LOG_ERROR("SetSleepMode error: {} ({})", magic_enum::enum_name(xellResult), (UINT) xellResult);
            return false;
        }

        auto fnaResult = fakenvapi::setModeAndContext(XeLLProxy::Context(), Mode::XeLL);
        LOG_DEBUG("fakenvapi::setModeAndContext: {}", fnaResult);

        result = XeFGProxy::SetLatencyReduction()(_swapChainContext, XeLLProxy::Context());

        if (result != XEFG_SWAPCHAIN_RESULT_SUCCESS)
        {
            LOG_ERROR("SetLatencyReduction error: {} ({})", magic_enum::enum_name(result), (UINT) result);
            return false;
        }
    };

    return true;
}

const char* XeFG_Dx12::Name() { return "XeFG"; }

feature_version XeFG_Dx12::Version()
{
    if (XeFGProxy::InitXeFG())
    {
        auto ver = XeFGProxy::Version();
        return ver;
    }

    return { 0, 0, 0 };
}

void XeFG_Dx12::StopAndDestroyContext(bool destroy, bool shutDown)
{
    LOG_DEBUG("");

    if (_isActive)
    {
        auto result = XeFGProxy::SetEnabled()(_swapChainContext, false);
        _isActive = false;
        if (!(shutDown || State::Instance().isShuttingDown))
            LOG_INFO("SetEnabled: false, result: {} ({})", magic_enum::enum_name(result), (UINT) result);
    }

    if (destroy && _fgContext != nullptr)
    {
        _fgContext = nullptr;
    }

    if (shutDown || State::Instance().isShuttingDown)
        ReleaseObjects();
}

bool XeFG_Dx12::DestroySwapchainContext()
{
    LOG_DEBUG("");

    _isActive = false;

    if (_swapChainContext != nullptr)
    {
        auto result = XeFGProxy::Destroy()(_swapChainContext);

        if (!State::Instance().isShuttingDown)
            LOG_INFO("Destroy result: {} ({})", magic_enum::enum_name(result), (UINT) result);

        _swapChainContext = nullptr;
    }

    return true;
}

xefg_swapchain_d3d12_resource_data_t XeFG_Dx12::GetResourceData(FG_ResourceType type)
{
    auto fIndex = GetIndex();

    xefg_swapchain_d3d12_resource_data_t resourceParam = {};

    if (!_frameResources[fIndex].contains(type))
        return resourceParam;

    auto fResource = &_frameResources[fIndex][type];

    resourceParam.validity = (fResource->validity == FG_ResourceValidity::ValidNow)
                                 ? XEFG_SWAPCHAIN_RV_ONLY_NOW
                                 : XEFG_SWAPCHAIN_RV_UNTIL_NEXT_PRESENT;

    resourceParam.resourceSize = { fResource->width, fResource->height };
    resourceParam.pResource = fResource->GetResource();
    resourceParam.incomingState = fResource->state;

    xefg_swapchain_resource_type_t xessType;

    switch (type)
    {
    case FG_ResourceType::Depth:
        resourceParam.type = XEFG_SWAPCHAIN_RES_DEPTH;
        break;

    case FG_ResourceType::HudlessColor:
        resourceParam.type = XEFG_SWAPCHAIN_RES_HUDLESS_COLOR;
        break;

    case FG_ResourceType::UIColor:
        resourceParam.type = XEFG_SWAPCHAIN_RES_UI;
        break;

    case FG_ResourceType::Velocity:
        resourceParam.type = XEFG_SWAPCHAIN_RES_MOTION_VECTOR;
        break;
    }

    if (type == FG_ResourceType::UIColor || type == FG_ResourceType::HudlessColor)
    {
        uint32_t left = 0;
        uint32_t top = 0;

        // use swapchain buffer info
        DXGI_SWAP_CHAIN_DESC scDesc1 {};
        if (State::Instance().currentSwapchain->GetDesc(&scDesc1) == S_OK)
        {
            LOG_DEBUG("SwapChain Res: {}x{}, Upscaler Display Res: {}x{}", scDesc1.BufferDesc.Width,
                      scDesc1.BufferDesc.Height, _interpolationWidth, _interpolationHeight);

            auto calculatedLeft = ((int) scDesc1.BufferDesc.Width - (int) _interpolationWidth) / 2;
            if (calculatedLeft > 0)
                left = Config::Instance()->FGRectLeft.value_or(calculatedLeft);

            auto calculatedTop = ((int) scDesc1.BufferDesc.Height - (int) _interpolationHeight) / 2;
            if (calculatedTop > 0)
                top = Config::Instance()->FGRectTop.value_or(calculatedTop);
        }
        else
        {
            left = Config::Instance()->FGRectLeft.value_or(0);
            top = Config::Instance()->FGRectTop.value_or(0);
        }

        resourceParam.resourceBase = { left, top };
        resourceParam.resourceSize = { _interpolationWidth, _interpolationHeight };
    }

    return resourceParam;
}

bool XeFG_Dx12::CreateSwapchain(IDXGIFactory* factory, ID3D12CommandQueue* cmdQueue, DXGI_SWAP_CHAIN_DESC* desc,
                                IDXGISwapChain** swapChain)
{
    if (_swapChainContext == nullptr)
    {
        if (State::Instance().currentD3D12Device == nullptr)
            return false;

        CreateSwapchainContext(State::Instance().currentD3D12Device);

        if (_swapChainContext == nullptr)
            return false;

        _width = desc->BufferDesc.Width;
        _height = desc->BufferDesc.Height;
    }

    IDXGIFactory* realFactory = nullptr;
    ID3D12CommandQueue* realQueue = nullptr;

    if (!CheckForRealObject(__FUNCTION__, factory, (IUnknown**) &realFactory))
        realFactory = factory;

    if (!CheckForRealObject(__FUNCTION__, cmdQueue, (IUnknown**) &realQueue))
        realQueue = cmdQueue;

    IDXGIFactory2* factory12 = nullptr;
    if (realFactory->QueryInterface(IID_PPV_ARGS(&factory12)) != S_OK)
        return false;

    factory12->Release();

    HWND hwnd = desc->OutputWindow;
    DXGI_SWAP_CHAIN_DESC1 scDesc {};

    scDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED; // No info
    scDesc.BufferCount = desc->BufferCount;
    scDesc.BufferUsage = desc->BufferUsage;
    scDesc.Flags = desc->Flags;
    scDesc.Format = desc->BufferDesc.Format;
    scDesc.Height = desc->BufferDesc.Height;
    scDesc.SampleDesc = desc->SampleDesc;
    scDesc.Scaling = DXGI_SCALING_NONE; // No info
    scDesc.Stereo = false;              // No info
    scDesc.SwapEffect = desc->SwapEffect;
    scDesc.Width = desc->BufferDesc.Width;

    DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsDesc {};
    fsDesc.RefreshRate = desc->BufferDesc.RefreshRate;
    fsDesc.Scaling = desc->BufferDesc.Scaling;
    fsDesc.ScanlineOrdering = desc->BufferDesc.ScanlineOrdering;
    fsDesc.Windowed = desc->Windowed;

    xefg_swapchain_d3d12_init_params_t params {};
    params.maxInterpolatedFrames = 1;

    params.initFlags = XEFG_SWAPCHAIN_INIT_FLAG_NONE;
    if (Config::Instance()->FGXeFGDepthInverted.value_or_default())
        params.initFlags |= XEFG_SWAPCHAIN_INIT_FLAG_INVERTED_DEPTH;

    if (Config::Instance()->FGXeFGJitteredMV.value_or_default())
        params.initFlags |= XEFG_SWAPCHAIN_INIT_FLAG_JITTERED_MV;

    if (Config::Instance()->FGXeFGHighResMV.value_or_default())
        params.initFlags |= XEFG_SWAPCHAIN_INIT_FLAG_HIGH_RES_MV;

    LOG_DEBUG("Inverted Depth: {}", Config::Instance()->FGXeFGDepthInverted.value_or_default());
    LOG_DEBUG("Jittered Velocity: {}", Config::Instance()->FGXeFGJitteredMV.value_or_default());
    LOG_DEBUG("High Res MV: {}", Config::Instance()->FGXeFGHighResMV.value_or_default());

    auto result = XeFGProxy::D3D12InitFromSwapChainDesc()(_swapChainContext, hwnd, &scDesc, &fsDesc, realQueue,
                                                          factory12, &params);

    if (result != XEFG_SWAPCHAIN_RESULT_SUCCESS)
    {
        LOG_ERROR("D3D12InitFromSwapChainDesc error: {} ({})", magic_enum::enum_name(result), (UINT) result);
        return false;
    }

    LOG_INFO("XeFG swapchain created");
    result = XeFGProxy::D3D12GetSwapChainPtr()(_swapChainContext, IID_PPV_ARGS(swapChain));
    if (result != XEFG_SWAPCHAIN_RESULT_SUCCESS)
    {
        LOG_ERROR("D3D12GetSwapChainPtr error: {} ({})", magic_enum::enum_name(result), (UINT) result);
        return false;
    }

    _gameCommandQueue = realQueue;
    _swapChain = *swapChain;
    _hwnd = hwnd;

    return true;
}

bool XeFG_Dx12::CreateSwapchain1(IDXGIFactory* factory, ID3D12CommandQueue* cmdQueue, HWND hwnd,
                                 DXGI_SWAP_CHAIN_DESC1* desc, DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pFullscreenDesc,
                                 IDXGISwapChain1** swapChain)
{
    if (_swapChainContext == nullptr)
    {
        if (State::Instance().currentD3D12Device == nullptr)
            return false;

        CreateSwapchainContext(State::Instance().currentD3D12Device);

        if (_swapChainContext == nullptr)
            return false;

        _width = desc->Width;
        _height = desc->Height;
    }

    IDXGIFactory* realFactory = nullptr;
    ID3D12CommandQueue* realQueue = nullptr;

    if (!CheckForRealObject(__FUNCTION__, factory, (IUnknown**) &realFactory))
        realFactory = factory;

    if (!CheckForRealObject(__FUNCTION__, cmdQueue, (IUnknown**) &realQueue))
        realQueue = cmdQueue;

    IDXGIFactory2* factory12 = nullptr;
    if (realFactory->QueryInterface(IID_PPV_ARGS(&factory12)) != S_OK)
        return false;

    factory12->Release();

    xefg_swapchain_d3d12_init_params_t params {};
    params.maxInterpolatedFrames = 1;

    params.initFlags = XEFG_SWAPCHAIN_INIT_FLAG_NONE;
    if (Config::Instance()->FGXeFGDepthInverted.value_or_default())
        params.initFlags |= XEFG_SWAPCHAIN_INIT_FLAG_INVERTED_DEPTH;

    if (Config::Instance()->FGXeFGJitteredMV.value_or_default())
        params.initFlags |= XEFG_SWAPCHAIN_INIT_FLAG_JITTERED_MV;

    if (Config::Instance()->FGXeFGHighResMV.value_or_default())
        params.initFlags |= XEFG_SWAPCHAIN_INIT_FLAG_HIGH_RES_MV;

    LOG_DEBUG("Inverted Depth: {}", Config::Instance()->FGXeFGDepthInverted.value_or_default());
    LOG_DEBUG("Jittered Velocity: {}", Config::Instance()->FGXeFGJitteredMV.value_or_default());
    LOG_DEBUG("High Res MV: {}", Config::Instance()->FGXeFGHighResMV.value_or_default());

    auto result = XeFGProxy::D3D12InitFromSwapChainDesc()(_swapChainContext, hwnd, desc, pFullscreenDesc, realQueue,
                                                          factory12, &params);

    if (result != XEFG_SWAPCHAIN_RESULT_SUCCESS)
    {
        LOG_ERROR("D3D12InitFromSwapChainDesc error: {} ({})", magic_enum::enum_name(result), (UINT) result);
        return false;
    }

    LOG_INFO("XeFG swapchain created");
    result = XeFGProxy::D3D12GetSwapChainPtr()(_swapChainContext, IID_PPV_ARGS(swapChain));
    if (result != XEFG_SWAPCHAIN_RESULT_SUCCESS)
    {
        LOG_ERROR("D3D12GetSwapChainPtr error: {} ({})", magic_enum::enum_name(result), (UINT) result);
        return false;
    }

    _gameCommandQueue = realQueue;
    _swapChain = *swapChain;
    _hwnd = hwnd;

    return true;
}

bool XeFG_Dx12::ReleaseSwapchain(HWND hwnd)
{
    if (hwnd != _hwnd || _hwnd == NULL)
        return false;

    LOG_DEBUG("");

    if (Config::Instance()->FGUseMutexForSwapchain.value_or_default())
    {
        LOG_TRACE("Waiting Mutex 1, current: {}", Mutex.getOwner());
        Mutex.lock(1);
        LOG_TRACE("Accuired Mutex: {}", Mutex.getOwner());
    }

    MenuOverlayDx::CleanupRenderTarget(true, NULL);

    if (_fgContext != nullptr)
        StopAndDestroyContext(true, true);

    if (_swapChainContext != nullptr)
        DestroySwapchainContext();

    if (Config::Instance()->FGUseMutexForSwapchain.value_or_default())
    {
        LOG_TRACE("Releasing Mutex: {}", Mutex.getOwner());
        Mutex.unlockThis(1);
    }

    return true;
}

void XeFG_Dx12::CreateContext(ID3D12Device* device, FG_Constants& fgConstants)
{
    if (_fgContext == nullptr && _swapChainContext != nullptr)
    {
        _fgContext = _swapChainContext;
    }

    if (!_isActive)
    {
        auto result = XeFGProxy::SetEnabled()(_swapChainContext, true);
        _isActive = true;
        LOG_INFO("SetEnabled: true, result: {} ({})", magic_enum::enum_name(result), (UINT) result);
    }
}

bool XeFG_Dx12::Dispatch()
{
    LOG_DEBUG();

    _lastDispatchedFrame = _frameCount;

    auto fIndex = GetIndex();

    XeFGProxy::EnableDebugFeature()(_swapChainContext, XEFG_SWAPCHAIN_DEBUG_FEATURE_TAG_INTERPOLATED_FRAMES,
                                    Config::Instance()->FGXeFGDebugView.value_or_default(), nullptr);
    XeFGProxy::EnableDebugFeature()(_swapChainContext, XEFG_SWAPCHAIN_DEBUG_FEATURE_SHOW_ONLY_INTERPOLATION,
                                    State::Instance().FGonlyGenerated, nullptr);
    XeFGProxy::EnableDebugFeature()(_swapChainContext, XEFG_SWAPCHAIN_DEBUG_FEATURE_PRESENT_FAILED_INTERPOLATION,
                                    State::Instance().FGonlyGenerated, nullptr);

    uint32_t left = 0;
    uint32_t top = 0;
    uint32_t width = _width;
    uint32_t height = _height;

    // use swapchain buffer info
    DXGI_SWAP_CHAIN_DESC scDesc1 {};
    if (State::Instance().currentSwapchain->GetDesc(&scDesc1) == S_OK)
    {
        LOG_DEBUG("SwapChain Res: {}x{}, Upscaler Display Res: {}x{}", scDesc1.BufferDesc.Width,
                  scDesc1.BufferDesc.Height, _interpolationWidth, _interpolationHeight);

        auto calculatedLeft = ((int) scDesc1.BufferDesc.Width - (int) _interpolationWidth) / 2;
        if (calculatedLeft > 0)
            left = Config::Instance()->FGRectLeft.value_or(calculatedLeft);

        auto calculatedTop = ((int) scDesc1.BufferDesc.Height - (int) _interpolationHeight) / 2;
        if (calculatedTop > 0)
            top = Config::Instance()->FGRectTop.value_or(calculatedTop);

        width = Config::Instance()->FGRectWidth.value_or(_interpolationWidth);
        height = Config::Instance()->FGRectHeight.value_or(_interpolationHeight);
    }
    else
    {
        left = Config::Instance()->FGRectLeft.value_or(0);
        top = Config::Instance()->FGRectTop.value_or(0);
        width = Config::Instance()->FGRectWidth.value_or(_width);
        height = Config::Instance()->FGRectHeight.value_or(_height);
    }

    uint32_t renderWidth = width;
    uint32_t renderHeight = height;

    LOG_DEBUG("Output Base: {}:{}, Size: {}x{}", left, top, width, height);

    // xefg_swapchain_d3d12_resource_data_t backbuffer = {};
    // backbuffer.type = XEFG_SWAPCHAIN_RES_BACKBUFFER;
    // backbuffer.resourceBase = { left, top };
    // backbuffer.resourceSize = { width, height };

    // auto result = XeFGProxy::D3D12TagFrameResource()(_swapChainContext,
    // _copyCommandList[FG_ResourceType::Distortion],
    //                                                  _frameCount, &backbuffer);
    // if (result != XEFG_SWAPCHAIN_RESULT_SUCCESS)
    //{
    //     LOG_ERROR("D3D12TagFrameResource Backbuffer error: {} ({})", magic_enum::enum_name(result), (UINT) result);
    //     return false;
    // }

    xefg_swapchain_frame_constant_data_t constData = {};

    if (_cameraPosition[0] != 0.0 || _cameraPosition[1] != 0.0 || _cameraPosition[2] != 0.0)
    {
        XMVECTOR right = XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(_cameraRight));
        XMVECTOR up = XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(_cameraUp));
        XMVECTOR forward = XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(_cameraForward));
        XMVECTOR pos = XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(_cameraPosition));

        float x = -XMVectorGetX(XMVector3Dot(pos, right));
        float y = -XMVectorGetX(XMVector3Dot(pos, up));
        float z = -XMVectorGetX(XMVector3Dot(pos, forward));

        XMMATRIX view = { XMVectorSet(XMVectorGetX(right), XMVectorGetX(up), XMVectorGetX(forward), 0.0f),
                          XMVectorSet(XMVectorGetY(right), XMVectorGetY(up), XMVectorGetY(forward), 0.0f),
                          XMVectorSet(XMVectorGetZ(right), XMVectorGetZ(up), XMVectorGetZ(forward), 0.0f),
                          XMVectorSet(x, y, z, 1.0f) };

        memcpy(constData.viewMatrix, view.r, sizeof(view));
    }

    if (Config::Instance()->FGXeFGDepthInverted.value_or_default())
        std::swap(_cameraNear, _cameraFar);

    // Cyberpunk seems to be sending LH so do the same
    // it also sends some extra data in usually empty spots but no idea what that is
    auto projectionMatrix = XMMatrixPerspectiveFovLH(_cameraVFov, _cameraAspectRatio, _cameraNear, _cameraFar);
    memcpy(constData.projectionMatrix, projectionMatrix.r, sizeof(projectionMatrix));

    constData.jitterOffsetX = _jitterX;
    constData.jitterOffsetY = _jitterY;
    constData.motionVectorScaleX = _mvScaleX;
    constData.motionVectorScaleY = _mvScaleY;
    constData.resetHistory = _reset;
    constData.frameRenderTime = _ftDelta;

    LOG_DEBUG("Reset: {}, FTDelta: {}", _reset, _ftDelta);

    auto result = XeFGProxy::TagFrameConstants()(_swapChainContext, _frameCount, &constData);
    if (result != XEFG_SWAPCHAIN_RESULT_SUCCESS)
    {
        LOG_ERROR("TagFrameConstants error: {} ({})", magic_enum::enum_name(result), (UINT) result);
        return false;
    }

    result = XeFGProxy::SetPresentId()(_swapChainContext, _frameCount);
    if (result != XEFG_SWAPCHAIN_RESULT_SUCCESS)
    {
        LOG_ERROR("SetPresentId error: {} ({})", magic_enum::enum_name(result), (UINT) result);
        return false;
    }

    LOG_DEBUG("Result: Ok");

    return true;
}

void* XeFG_Dx12::FrameGenerationContext() { return _fgContext; }

void* XeFG_Dx12::SwapchainContext() { return _swapChainContext; }

void XeFG_Dx12::EvaluateState(ID3D12Device* device, FG_Constants& fgConstants)
{
    LOG_FUNC();

    if (!Config::Instance()->OverlayMenu.value_or_default())
        return;

    static bool lastInfiniteDepth = false;
    bool currentInfiniteDepth = static_cast<bool>(fgConstants.flags & FG_Flags::InfiniteDepth);
    if (lastInfiniteDepth != currentInfiniteDepth)
    {
        lastInfiniteDepth = currentInfiniteDepth;
        LOG_DEBUG("Infinite Depth changed: {}", currentInfiniteDepth);
        State::Instance().FGchanged = true;
    }

    if (!State::Instance().FGchanged && Config::Instance()->FGEnabled.value_or_default() && !IsPaused() &&
        XeFGProxy::InitXeFG() && !IsActive() && HooksDx::CurrentSwapchainFormat() != DXGI_FORMAT_UNKNOWN)
    {
        CreateObjects(device);
        CreateContext(device, fgConstants);
        ResetCounters();
        UpdateTarget();
    }
    else if ((!Config::Instance()->FGEnabled.value_or_default() || State::Instance().FGchanged) && IsActive())
    {
        StopAndDestroyContext(State::Instance().SCchanged, false);

        if (State::Instance().activeFgInput == FGInput::Upscaler)
        {
            State::Instance().ClearCapturedHudlesses = true;
            Hudfix_Dx12::ResetCounters();
        }
    }

    if (State::Instance().FGchanged)
    {
        LOG_DEBUG("(FG) Frame generation paused");
        ResetCounters();
        UpdateTarget();

        if (State::Instance().activeFgInput == FGInput::Upscaler)
            Hudfix_Dx12::ResetCounters();

        // Release FG mutex
        if (Mutex.getOwner() == 2)
            Mutex.unlockThis(2);

        State::Instance().FGchanged = false;
    }

    State::Instance().SCchanged = false;
}

void XeFG_Dx12::ReleaseObjects()
{
    // For each FG_ResourceType
    for (size_t i = 0; i < 5; i++)
    {
        if (this->_copyCommandAllocator.contains((FG_ResourceType) i))
            this->_copyCommandAllocator[(FG_ResourceType) i]->Release();

        if (this->_copyCommandList.contains((FG_ResourceType) i))
            this->_copyCommandList[(FG_ResourceType) i]->Release();
    }

    _copyCommandAllocator.clear();
    _copyCommandList.clear();

    _mvFlip.reset();
    _depthFlip.reset();
}

void XeFG_Dx12::CreateObjects(ID3D12Device* InDevice)
{
    _device = InDevice;

    if (_copyCommandList.size() > 0)
        ReleaseObjects();

    LOG_DEBUG("");

    do
    {
        HRESULT result;
        ID3D12CommandAllocator* allocator = nullptr;
        ID3D12GraphicsCommandList* cmdList = nullptr;
        ID3D12CommandQueue* cmdQueue = nullptr;

        // For each FG_ResourceType
        for (size_t i = 0; i < 5; i++)
        {
            auto val = (FG_ResourceType) i;

            ID3D12CommandAllocator* enumAllocator = nullptr;
            ID3D12GraphicsCommandList* enumCmdList = nullptr;

            // Copy
            auto result = InDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                           IID_PPV_ARGS(&this->_copyCommandAllocator[val]));
            if (result != S_OK)
            {
                LOG_ERROR("CreateCommandAllocators _copyCommandAllocator[{}]: {:X}", (UINT) val,
                          (unsigned long) result);
                return;
            }

            this->_copyCommandAllocator[val]->SetName(std::format(L"_copyCommandAllocator[{}]", (UINT) val).c_str());
            if (CheckForRealObject(__FUNCTION__, this->_copyCommandAllocator[val], (IUnknown**) &enumAllocator))
                this->_copyCommandAllocator[val] = enumAllocator;

            result = InDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, this->_copyCommandAllocator[val],
                                                 NULL, IID_PPV_ARGS(&this->_copyCommandList[val]));
            if (result != S_OK)
            {
                LOG_ERROR("CreateCommandList _copyCommandList[{}]: {:X}", (UINT) val, (unsigned long) result);
                return;
            }

            this->_copyCommandList[val]->SetName(std::format(L"_copyCommandAllocator[{}]", (UINT) val).c_str());
            if (CheckForRealObject(__FUNCTION__, this->_copyCommandList[val], (IUnknown**) &enumCmdList))
                this->_copyCommandList[val] = enumCmdList;

            result = this->_copyCommandList[val]->Close();
            if (result != S_OK)
            {
                LOG_ERROR("_copyCommandList[{}]->Close: {:X}", (UINT) val, (unsigned long) result);
                return;
            }
        }

    } while (false);
}

bool XeFG_Dx12::Present() { return Dispatch(); }

void XeFG_Dx12::SetResource(FG_ResourceType type, ID3D12GraphicsCommandList* cmdList, ID3D12Resource* resource,
                            UINT width, UINT height, D3D12_RESOURCE_STATES state, FG_ResourceValidity validity)
{
    if (resource == nullptr)
        return;

    if (cmdList == nullptr && validity == FG_ResourceValidity::ValidNow && _gameCommandQueue == nullptr)
    {
        LOG_ERROR("{}, validity == ValidNow but _gameCommandQueue is nullptr!", magic_enum::enum_name(type));
        return;
    }

    auto fIndex = GetIndex();
    _frameResources[fIndex][type] = {};
    auto fResource = &_frameResources[fIndex][type];
    fResource->type = type;
    fResource->state = state;
    fResource->validity = validity;
    fResource->resource = resource;
    fResource->width = width;
    fResource->height = height;

    auto willFlip = State::Instance().activeFgInput == FGInput::Upscaler &&
                    Config::Instance()->FGResourceFlip.value_or_default() &&
                    (type == FG_ResourceType::Velocity || type == FG_ResourceType::Depth);

    auto usingLocalCmdList = false;

    if (cmdList == nullptr && (validity == FG_ResourceValidity::ValidNow || willFlip))
    {
        if (!_copyCommandAllocator.contains(type) || !_copyCommandList.contains(type))
        {
            LOG_ERROR("{}, _copyCommandAllocator or _copyCommandList is nullptr!", magic_enum::enum_name(type));
            return;
        }

        auto allocator = _copyCommandAllocator[type];
        cmdList = _copyCommandList[type];

        allocator->Reset();
        cmdList->Reset(allocator, nullptr);

        usingLocalCmdList = true;
    }

    fResource->cmdList = cmdList;

    // Resource flipping
    if (willFlip && _device != nullptr)
    {
        if (type == FG_ResourceType::Velocity)
        {
            ID3D12Resource* flipOutput = nullptr;

            flipOutput = _resourceCopy[fIndex][type];

            if (!CreateBufferResource(_device, resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, &flipOutput, true,
                                      false))
            {
                LOG_ERROR("{}, CreateBufferResource for flip is failed!", magic_enum::enum_name(type));
                return;
            }

            _resourceCopy[fIndex][type] = flipOutput;

            if (_mvFlip.get() == nullptr)
            {
                _mvFlip = std::make_unique<RF_Dx12>("VelocityFlip", _device);
            }
            else if (_mvFlip->IsInit())
            {
                auto feature = State::Instance().currentFeature;
                UINT width = feature->LowResMV() ? feature->RenderWidth() : feature->DisplayWidth();
                UINT height = feature->LowResMV() ? feature->RenderHeight() : feature->DisplayHeight();

                auto result = _mvFlip->Dispatch(_device, cmdList, resource, flipOutput, width, height, true);

                if (result)
                {
                    LOG_TRACE("Setting velocity from flip, index: {}", fIndex);
                    fResource->copy = flipOutput;
                    fResource->state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
                }
            }
        }
        else if (type == FG_ResourceType::Depth)
        {
            ID3D12Resource* flipOutput = nullptr;

            flipOutput = _resourceCopy[fIndex][type];

            if (!CreateBufferResource(_device, resource, state, &flipOutput, true, true))
            {
                LOG_ERROR("{}, CreateBufferResource for flip is failed!", magic_enum::enum_name(type));
                return;
            }

            _resourceCopy[fIndex][type] = flipOutput;

            if (_depthFlip.get() == nullptr)
            {
                _depthFlip = std::make_unique<RF_Dx12>("DepthFlip", _device);
            }
            else if (_depthFlip->IsInit())
            {
                auto feature = State::Instance().currentFeature;
                UINT width = feature->LowResMV() ? feature->RenderWidth() : feature->DisplayWidth();
                UINT height = feature->LowResMV() ? feature->RenderHeight() : feature->DisplayHeight();

                ResourceBarrier(cmdList, flipOutput, state, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
                auto result = _depthFlip->Dispatch(_device, cmdList, resource, flipOutput, width, height, false);
                ResourceBarrier(cmdList, flipOutput, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, state);

                if (result)
                {
                    LOG_TRACE("Setting velocity from flip, index: {}", fIndex);
                    fResource->copy = flipOutput;
                }
            }
        }
    }

    fResource->validity = (validity != FG_ResourceValidity::ValidNow && !willFlip) ? FG_ResourceValidity::UntilPresent
                                                                                   : FG_ResourceValidity::ValidNow;

    xefg_swapchain_d3d12_resource_data_t resourceParam = GetResourceData(type);

    if (validity == FG_ResourceValidity::UntilPresent ||
        (cmdList != nullptr && validity == FG_ResourceValidity::ValidNow))
    {
        auto result = XeFGProxy::D3D12TagFrameResource()(_swapChainContext, cmdList, _frameCount, &resourceParam);
        if (result != XEFG_SWAPCHAIN_RESULT_SUCCESS)
        {
            LOG_ERROR("D3D12TagFrameResource Depth error: {} ({})", magic_enum::enum_name(result), (UINT) result);
            return;
        }

        SetResourceReady(type);

        if (usingLocalCmdList)
        {
            cmdList->Close();
            _gameCommandQueue->ExecuteCommandLists(1, (ID3D12CommandList**) &cmdList);
        }
    }
    else
    {
        ResTrack_Dx12::SetResourceCmdList(type, cmdList);

        if (!_copyCommandAllocator.contains(type) || !_copyCommandList.contains(type))
        {
            LOG_ERROR("{}, _copyCommandAllocator or _copyCommandList is nullptr!", magic_enum::enum_name(type));
            return;
        }

        auto allocator = _copyCommandAllocator[type];
        cmdList = _copyCommandList[type];

        allocator->Reset();
        cmdList->Reset(allocator, nullptr);

        fResource->cmdList = cmdList;
    }

    LOG_TRACE("_frameResources[{}][{}]: {:X}", fIndex, magic_enum::enum_name(type), (size_t) fResource->GetResource());
}

void XeFG_Dx12::SetResourceReady(FG_ResourceType type) { _resourceReady[GetIndex()][type] = true; }

void XeFG_Dx12::SetCommandQueue(FG_ResourceType type, ID3D12CommandQueue* queue)
{
    //_gameCommandQueue = queue;

    auto fIndex = GetIndex();

    if (!_frameResources[fIndex].contains(type))
        return;

    auto fResource = &_frameResources[fIndex][type];

    auto resourceParam = GetResourceData(type);
    if (resourceParam.pResource == nullptr)
        return;

    if (fResource->validity == FG_ResourceValidity::JustTrackCmdlist)
        resourceParam.validity = XEFG_SWAPCHAIN_RV_UNTIL_NEXT_PRESENT;

    auto result =
        XeFGProxy::D3D12TagFrameResource()(_swapChainContext, fResource->cmdList, _frameCount, &resourceParam);
    if (result != XEFG_SWAPCHAIN_RESULT_SUCCESS)
    {
        LOG_ERROR("D3D12TagFrameResource {} error: {} ({})", magic_enum::enum_name(type), magic_enum::enum_name(result),
                  (UINT) result);
        return;
    }

    ((ID3D12GraphicsCommandList*) fResource->cmdList)->Close();
    _gameCommandQueue->ExecuteCommandLists(1, (ID3D12CommandList**) &fResource->cmdList);
}
