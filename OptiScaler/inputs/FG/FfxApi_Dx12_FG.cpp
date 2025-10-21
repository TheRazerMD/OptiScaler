#include "FfxApi_Dx12_FG.h"

#include "pch.h"

#include <Util.h>
#include <Config.h>

#include <magic_enum.hpp>

#include "ffx_framegeneration.h"
#include "dx12/ffx_api_dx12.h"

static ID3D12Device* _device = nullptr;
static FG_Constants _fgConst {};
static UINT64 _currentFrameId = 0;

static FfxApiPresentCallbackFunc _presentCallback = nullptr;
static void* _presentCallbackUserContext = nullptr;
static FfxApiFrameGenerationDispatchFunc _fgCallback = nullptr;
static void* _fgCallbackUserContext = nullptr;
static UINT64 _callbackFrameId = 0;
static FfxApiRect2D _callbackRect = {};

static std::mutex _newFrameMutex;

static ID3D12Resource* _hudless[BUFFER_COUNT] = {};
static ID3D12Resource* _interpolation[BUFFER_COUNT] = {};
static Dx12Resource _uiRes[BUFFER_COUNT] = {};

// #define PASSTHRU

static FfxApiResourceState GetFfxApiState(D3D12_RESOURCE_STATES state)
{
    switch (state)
    {
    case D3D12_RESOURCE_STATE_COMMON:
        return FFX_API_RESOURCE_STATE_COMMON;
    case D3D12_RESOURCE_STATE_UNORDERED_ACCESS:
        return FFX_API_RESOURCE_STATE_UNORDERED_ACCESS;
    case D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE:
        return FFX_API_RESOURCE_STATE_COMPUTE_READ;
    case D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE:
        return FFX_API_RESOURCE_STATE_PIXEL_READ;
    case (D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE):
        return FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ;
    case D3D12_RESOURCE_STATE_COPY_SOURCE:
        return FFX_API_RESOURCE_STATE_COPY_SRC;
    case D3D12_RESOURCE_STATE_COPY_DEST:
        return FFX_API_RESOURCE_STATE_COPY_DEST;
    case D3D12_RESOURCE_STATE_GENERIC_READ:
        return FFX_API_RESOURCE_STATE_GENERIC_READ;
    case D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT:
        return FFX_API_RESOURCE_STATE_INDIRECT_ARGUMENT;
    case D3D12_RESOURCE_STATE_RENDER_TARGET:
        return FFX_API_RESOURCE_STATE_RENDER_TARGET;
    default:
        return FFX_API_RESOURCE_STATE_COMMON;
    }
}

static D3D12_RESOURCE_STATES GetD3D12State(FfxApiResourceState state)
{
    switch (state)
    {
    case FFX_API_RESOURCE_STATE_COMMON:
        return D3D12_RESOURCE_STATE_COMMON;
    case FFX_API_RESOURCE_STATE_UNORDERED_ACCESS:
        return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    case FFX_API_RESOURCE_STATE_COMPUTE_READ:
        return D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    case FFX_API_RESOURCE_STATE_PIXEL_READ:
        return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    case FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ:
        return (D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    case FFX_API_RESOURCE_STATE_COPY_SRC:
        return D3D12_RESOURCE_STATE_COPY_SOURCE;
    case FFX_API_RESOURCE_STATE_COPY_DEST:
        return D3D12_RESOURCE_STATE_COPY_DEST;
    case FFX_API_RESOURCE_STATE_GENERIC_READ:
        return D3D12_RESOURCE_STATE_GENERIC_READ;
    case FFX_API_RESOURCE_STATE_INDIRECT_ARGUMENT:
        return D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
    case FFX_API_RESOURCE_STATE_RENDER_TARGET:
        return D3D12_RESOURCE_STATE_RENDER_TARGET;
    default:
        return D3D12_RESOURCE_STATE_COMMON;
    }
}

static bool CreateBufferResource(ID3D12Device* InDevice, ID3D12Resource* InResource, D3D12_RESOURCE_STATES InState,
                                 ID3D12Resource** OutResource)
{
    if (InDevice == nullptr || InResource == nullptr)
        return false;

    auto inDesc = InResource->GetDesc();

    if (*OutResource != nullptr)
    {
        auto bufDesc = (*OutResource)->GetDesc();

        if (bufDesc.Width != inDesc.Width || bufDesc.Height != inDesc.Height || bufDesc.Format != inDesc.Format)
        {
            (*OutResource)->Release();
            (*OutResource) = nullptr;
            LOG_WARN("Release {}x{}, new one: {}x{}", bufDesc.Width, bufDesc.Height, inDesc.Width, inDesc.Height);
        }
        else
        {
            return true;
        }
    }

    HRESULT hr;

    // D3D12_HEAP_PROPERTIES heapProperties;
    // D3D12_HEAP_FLAGS heapFlags;
    // hr = InResource->GetHeapProperties(&heapProperties, &heapFlags);
    // if (hr != S_OK)
    //{
    //     LOG_ERROR("GetHeapProperties result: {:X}", (UINT64) hr);
    //     return false;
    // }

    CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);

    inDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET; //    | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    hr = InDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &inDesc, InState, nullptr,
                                           IID_PPV_ARGS(OutResource));

    if (hr != S_OK)
    {
        LOG_ERROR("CreateCommittedResource result: {:X}", (UINT64) hr);
        return false;
    }

    LOG_DEBUG("Created new one: {}x{}", inDesc.Width, inDesc.Height);
    return true;
}

static void ResourceBarrier(ID3D12GraphicsCommandList* InCommandList, ID3D12Resource* InResource,
                            D3D12_RESOURCE_STATES InBeforeState, D3D12_RESOURCE_STATES InAfterState)
{
    if (InBeforeState == InAfterState)
        return;

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = InResource;
    barrier.Transition.StateBefore = InBeforeState;
    barrier.Transition.StateAfter = InAfterState;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    InCommandList->ResourceBarrier(1, &barrier);
}

ffxReturnCode_t ffxCreateContext_Dx12FG(ffxContext* context, ffxCreateContextDescHeader* desc,
                                        const ffxAllocationCallbacks* memCb)
{
#ifdef PASSTHRU
    return (ffxReturnCode_t) 0xFFFFFFFF;
#endif

    LOG_DEBUG("");

    if (desc->type == FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATION)
    {
        ffxCreateContextDescHeader* next = nullptr;
        next = desc;
        while (next->pNext != nullptr)
        {
            next = next->pNext;

            if (next->type == FFX_API_CREATE_CONTEXT_DESC_TYPE_BACKEND_DX12)
            {
                auto cbDesc = (ffxCreateBackendDX12Desc*) next;
                _device = cbDesc->device;
                LOG_DEBUG("Device found: {:X}", (size_t) _device);
                break;
            }
        }

        if (_device != nullptr && State::Instance().currentFG != nullptr)
        {
            if (State::Instance().currentFG->FrameGenerationContext() != nullptr)
            {
                LOG_INFO("There is already an active FG context: {:X}, destroying it.",
                         (size_t) State::Instance().currentFG->FrameGenerationContext());

                State::Instance().currentFG->DestroyFGContext();
            }

            auto ccDesc = (ffxCreateContextDescFrameGeneration*) desc;

            _fgConst = {};

            _fgConst.displayHeight = ccDesc->displaySize.height;
            _fgConst.displayWidth = ccDesc->displaySize.width;

            if ((ccDesc->flags & FFX_FRAMEGENERATION_ENABLE_ASYNC_WORKLOAD_SUPPORT) > 0)
                _fgConst.flags |= FG_Flags::Async;

            if ((ccDesc->flags & FFX_FRAMEGENERATION_ENABLE_DISPLAY_RESOLUTION_MOTION_VECTORS) > 0)
                _fgConst.flags |= FG_Flags::DisplayResolutionMVs;

            if ((ccDesc->flags & FFX_FRAMEGENERATION_ENABLE_MOTION_VECTORS_JITTER_CANCELLATION) > 0)
                _fgConst.flags |= FG_Flags::JitteredMVs;

            if ((ccDesc->flags & FFX_FRAMEGENERATION_ENABLE_DEPTH_INVERTED) > 0)
                _fgConst.flags |= FG_Flags::InvertedDepth;

            if ((ccDesc->flags & FFX_FRAMEGENERATION_ENABLE_DEPTH_INFINITE) > 0)
                _fgConst.flags |= FG_Flags::InfiniteDepth;

            if ((ccDesc->flags & FFX_FRAMEGENERATION_ENABLE_HIGH_DYNAMIC_RANGE) > 0)
                _fgConst.flags |= FG_Flags::Hdr;

            Config::Instance()->FGXeFGDepthInverted = _fgConst.flags[FG_Flags::InvertedDepth];
            Config::Instance()->FGXeFGJitteredMV = _fgConst.flags[FG_Flags::JitteredMVs];
            Config::Instance()->FGXeFGHighResMV = _fgConst.flags[FG_Flags::DisplayResolutionMVs];
            LOG_DEBUG("XeFG DepthInverted: {}", Config::Instance()->FGXeFGDepthInverted.value_or_default());
            LOG_DEBUG("XeFG JitteredMV: {}", Config::Instance()->FGXeFGJitteredMV.value_or_default());
            LOG_DEBUG("XeFG HighResMV: {}", Config::Instance()->FGXeFGHighResMV.value_or_default());
            Config::Instance()->SaveXeFG();

            State::Instance().currentFG->CreateContext(_device, _fgConst);

            *context = (ffxContext) fgContext;
            return FFX_API_RETURN_OK;
        }
    }
    else if (desc->type == FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_WRAP_DX12)
    {
        auto cDesc = (ffxCreateContextDescFrameGenerationSwapChainWrapDX12*) desc;

        if (State::Instance().currentFG != nullptr && State::Instance().currentFGSwapchain != nullptr)
        {
            *context = (ffxContext) scContext; // State::Instance().currentFG->SwapchainContext();
            *cDesc->swapchain = (IDXGISwapChain4*) State::Instance().currentFGSwapchain;
            return FFX_API_RETURN_OK;
        }
        else
        {
            LOG_ERROR("currentFG: {:X}, currentFGSwapchain: {:X}", (size_t) State::Instance().currentFG,
                      (size_t) State::Instance().currentFGSwapchain);
            return FFX_API_RETURN_ERROR_RUNTIME_ERROR;
        }
    }
    else if (desc->type == FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_NEW_DX12)
    {
        auto cDesc = (ffxCreateContextDescFrameGenerationSwapChainNewDX12*) desc;
        auto result =
            cDesc->dxgiFactory->CreateSwapChain(cDesc->gameQueue, cDesc->desc, (IDXGISwapChain**) cDesc->swapchain);

        if (result == S_OK)
        {
            LOG_INFO("Swapchain created");

            if (State::Instance().currentFG != nullptr && State::Instance().currentFGSwapchain != nullptr)
            {
                *context = (ffxContext) scContext;
                return FFX_API_RETURN_OK;
            }
            else
            {
                LOG_ERROR("FG Swapchain creation error, currentFG: {:X}, currentFGSwapchain: {:X}",
                          (size_t) State::Instance().currentFG, (size_t) State::Instance().currentFGSwapchain);
            }
        }
        else
        {
            LOG_ERROR("Swapchain creation error: {:X}", (UINT) result);
        }

        return FFX_API_RETURN_ERROR_PARAMETER;
    }
    else if (desc->type == FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_FOR_HWND_DX12)
    {
        auto cDesc = (ffxCreateContextDescFrameGenerationSwapChainForHwndDX12*) desc;

        IDXGIFactory2* factory = nullptr;
        auto scResult = cDesc->dxgiFactory->QueryInterface(IID_PPV_ARGS(&factory));

        if (factory == nullptr)
            return FFX_API_RETURN_ERROR_PARAMETER;

        factory->Release();

        auto result = factory->CreateSwapChainForHwnd(cDesc->gameQueue, cDesc->hwnd, cDesc->desc, cDesc->fullscreenDesc,
                                                      nullptr, (IDXGISwapChain1**) cDesc->swapchain);

        if (result == S_OK)
        {
            LOG_INFO("Swapchain created");

            if (State::Instance().currentFG != nullptr && State::Instance().currentFGSwapchain != nullptr)
            {
                *context = (ffxContext) scContext;
                return FFX_API_RETURN_OK;
            }
            else
            {
                LOG_ERROR("FG Swapchain creation error, currentFG: {:X}, currentFGSwapchain: {:X}",
                          (size_t) State::Instance().currentFG, (size_t) State::Instance().currentFGSwapchain);
            }
        }
        else
        {
            LOG_ERROR("Swapchain creation error: {:X}", (UINT) result);
        }

        return FFX_API_RETURN_ERROR_PARAMETER;
    }

    return rcContinue;
}

ffxReturnCode_t ffxDestroyContext_Dx12FG(ffxContext* context, const ffxAllocationCallbacks* memCb)
{
#ifdef PASSTHRU
    return (ffxReturnCode_t) 0xFFFFFFFF;
#endif

    LOG_DEBUG("");

    if (State::Instance().currentFG != nullptr && (void*) scContext == *context)
    {
        LOG_INFO("Destroying Swapchain Context: {:X}", (size_t) State::Instance().currentFG);
        State::Instance().currentFG->Shutdown();
        return FFX_API_RETURN_OK;
    }
    else if (State::Instance().currentFG != nullptr && (void*) fgContext == *context)
    {
        LOG_INFO("Destroying FG Context: {:X}", (size_t) State::Instance().currentFG);
        State::Instance().currentFG->DestroyFGContext();
        return FFX_API_RETURN_OK;
    }

    return rcContinue;
}

ffxReturnCode_t ffxConfigure_Dx12FG(ffxContext* context, ffxConfigureDescHeader* desc)
{

#ifdef PASSTHRU
    if (desc->type == FFX_API_CONFIGURE_DESC_TYPE_FRAMEGENERATION)
    {
        auto cDesc = (ffxConfigureDescFrameGeneration*) desc;

        if (cDesc->presentCallback != nullptr)
        {
            LOG_DEBUG("presentCallback exist");

            _presentCallback = cDesc->presentCallback;

            cDesc->presentCallback = [](ffxCallbackDescFrameGenerationPresent* params,
                                        void* pUserCtx) -> ffxReturnCode_t
            {
                IDXGISwapChain3* sc = (IDXGISwapChain3*) State::Instance().currentFGSwapchain;
                auto scIndex = sc->GetCurrentBackBufferIndex();

                ID3D12Resource* currentBuffer = nullptr;
                auto hr = sc->GetBuffer(scIndex, IID_PPV_ARGS(&currentBuffer));
                currentBuffer->Release();

                auto result = _presentCallback(params, pUserCtx);
                return result;
            };
        }
    }

    return (ffxReturnCode_t) 0xFFFFFFFF;
#endif

    auto fg = State::Instance().currentFG;

    if (fg == nullptr)
    {
        LOG_ERROR("No FG Feature!");
        return FFX_API_RETURN_ERROR_RUNTIME_ERROR;
    }

    bool UIDisabled = Config::Instance()->FGDisableUI.value_or_default();

    if (desc->type == FFX_API_CONFIGURE_DESC_TYPE_FRAMEGENERATION)
    {
        auto cDesc = (ffxConfigureDescFrameGeneration*) desc;

        {
            std::lock_guard<std::mutex> lock(_newFrameMutex);
            if (cDesc->frameID > _currentFrameId)
            {
                _currentFrameId = cDesc->frameID;
                fg->StartNewFrame();
                _uiRes[fg->GetIndex()] = {};
            }
        }

        LOG_DEBUG("FFX_API_CONFIGURE_DESC_TYPE_FRAMEGENERATION frameID: {}, enabled: {} ", cDesc->frameID,
                  cDesc->frameGenerationEnabled);

        State::Instance().FSRFGInputActive = cDesc->frameGenerationEnabled;

        if (cDesc->frameGenerationEnabled && !fg->IsActive() && Config::Instance()->FGEnabled.value_or_default())
        {
            fg->Activate();
            fg->ResetCounters();
        }
        else if (!cDesc->frameGenerationEnabled && fg->IsActive())
        {
            fg->Deactivate();
            fg->ResetCounters();
        }

        fg->SetInterpolationRect(cDesc->generationRect.width, cDesc->generationRect.height);
        fg->SetInterpolationPos(cDesc->generationRect.left, cDesc->generationRect.top);

        UINT width = cDesc->generationRect.width;
        UINT height = cDesc->generationRect.height;
        UINT left = cDesc->generationRect.left;
        UINT top = cDesc->generationRect.top;

        if (width == 0)
        {
            DXGI_SWAP_CHAIN_DESC scDesc {};
            State::Instance().currentFGSwapchain->GetDesc(&scDesc);

            width = scDesc.BufferDesc.Width;
            height = scDesc.BufferDesc.Height;
            top = 0;
            left = 0;
        }

        ffxConfigureDescHeader* next = nullptr;
        next = desc;
        while (next->pNext != nullptr)
        {
            next = next->pNext;

            if (next->type == FFX_API_CONFIGURE_DESC_TYPE_FRAMEGENERATION_REGISTERDISTORTIONRESOURCE)
            {
                auto crDesc = (ffxConfigureDescFrameGenerationRegisterDistortionFieldResource*) next;
                LOG_DEBUG("DistortionFieldResource found: {:X}", (size_t) crDesc->distortionField.resource);

                if (fg->FrameGenerationContext() != nullptr && crDesc->distortionField.resource != nullptr)
                {
                    Dx12Resource dfr {};
                    dfr.cmdList = nullptr; // Not sure about this
                    dfr.height = height;
                    dfr.resource = (ID3D12Resource*) crDesc->distortionField.resource;
                    dfr.state = GetD3D12State((FfxApiResourceState) crDesc->distortionField.state);
                    dfr.type = FG_ResourceType::Distortion;
                    dfr.validity = FG_ResourceValidity::UntilPresent; // Not sure about this
                    dfr.width = width;
                    dfr.left = left;
                    dfr.top = top;

                    fg->SetResource(&dfr);
                }
            }
            else if (next->type == FFX_API_CONFIGURE_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_REGISTERUIRESOURCE_DX12)
            {
                auto crDesc = (ffxConfigureDescFrameGenerationSwapChainRegisterUiResourceDX12*) next;
                LOG_DEBUG("UiResource found 1: {:X}", (size_t) crDesc->uiResource.resource);

                if (fg->FrameGenerationContext() != nullptr && crDesc->uiResource.resource != nullptr)
                {
                    auto validity = FG_ResourceValidity::UntilPresent;
                    if ((crDesc->flags & FFX_FRAMEGENERATION_UI_COMPOSITION_FLAG_ENABLE_INTERNAL_UI_DOUBLE_BUFFERING) >
                        0)
                    {
                        LOG_WARN("FFX_FRAMEGENERATION_UI_COMPOSITION_FLAG_ENABLE_INTERNAL_UI_DOUBLE_BUFFERING is set!");

                        // Not sure which cmdList to use
                        // validity = FG_ResourceValidity::ValidButMakeCopy;
                    }

                    Dx12Resource ui {};
                    ui.cmdList = nullptr; // Not sure about this
                    ui.height = height;
                    ui.resource = (ID3D12Resource*) crDesc->uiResource.resource;
                    ui.state = GetD3D12State((FfxApiResourceState) crDesc->uiResource.state);
                    ui.type = FG_ResourceType::UIColor;
                    ui.validity = validity; // Not sure about this
                    ui.width = width;
                    ui.left = left;
                    ui.top = top;

                    _uiRes[fg->GetIndex()] = ui;

                    fg->SetResource(&ui);
                }
            }
        }

        if (cDesc->HUDLessColor.resource != nullptr)
        {
            Dx12Resource hudless {};
            hudless.cmdList = nullptr; // Not sure about this
            hudless.height = height;
            hudless.resource = (ID3D12Resource*) cDesc->HUDLessColor.resource;
            hudless.state = GetD3D12State((FfxApiResourceState) cDesc->HUDLessColor.state);
            hudless.type = FG_ResourceType::HudlessColor;
            hudless.validity = FG_ResourceValidity::UntilPresent; // Not sure about this
            hudless.width = width;
            hudless.left = left;
            hudless.top = top;

            fg->SetResource(&hudless);
        }

        if (cDesc->frameGenerationCallback != nullptr && cDesc->frameGenerationEnabled)
        {
            LOG_DEBUG("frameGenerationCallback exist");

            _callbackFrameId = cDesc->frameID;
            _callbackRect = cDesc->generationRect;
            _fgCallback = cDesc->frameGenerationCallback;
            _fgCallbackUserContext = cDesc->frameGenerationCallbackUserContext;
        }

        if (cDesc->presentCallback != nullptr)
        {
            LOG_DEBUG("presentCallback exist");

            _presentCallback = cDesc->presentCallback;
            _presentCallbackUserContext = cDesc->presentCallbackUserContext;
            _callbackFrameId = cDesc->frameID;
        }

        if (cDesc->allowAsyncWorkloads || cDesc->onlyPresentGenerated)
            LOG_DEBUG("allowAsyncWorkloads: {}, onlyPresentGenerated: {}", cDesc->allowAsyncWorkloads,
                      cDesc->onlyPresentGenerated);

        // Not used:
        //   cDesc->allowAsyncWorkloads
        //   cDesc->onlyPresentGenerated
        //   cDesc->frameID
        //   cDesc->frameGenerationCallback
        //   cDesc->frameGenerationCallbackUserContext
        //   cDesc->presentCallback
        //   cDesc->presentCallbackUserContext
        //   cDesc->swapChain

        LOG_DEBUG("FFX_API_CONFIGURE_DESC_TYPE_FRAMEGENERATION done");

        return FFX_API_RETURN_OK;
    }
    else if (desc->type == FFX_API_CONFIGURE_DESC_TYPE_FRAMEGENERATION_REGISTERDISTORTIONRESOURCE)
    {
        auto crDesc = (ffxConfigureDescFrameGenerationRegisterDistortionFieldResource*) desc;
        LOG_DEBUG("DistortionFieldResource found: {:X}", (size_t) crDesc->distortionField.resource);

        if (fg->FrameGenerationContext() != nullptr && crDesc->distortionField.resource != nullptr)
        {
            UINT64 width = 0;
            UINT height = 0;
            UINT left = 0;
            UINT top = 0;

            fg->GetInterpolationRect(width, height);
            fg->GetInterpolationPos(left, top);

            if (width == 0)
            {
                DXGI_SWAP_CHAIN_DESC scDesc {};
                State::Instance().currentFGSwapchain->GetDesc(&scDesc);

                width = scDesc.BufferDesc.Width;
                height = scDesc.BufferDesc.Height;
                top = 0;
                left = 0;
            }

            // Check for FFX_API_CONFIGURE_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_REGISTERUIRESOURCE_DX12
            ffxConfigureDescHeader* next = nullptr;
            next = desc;
            while (next->pNext != nullptr)
            {
                next = next->pNext;

                if (next->type == FFX_API_CONFIGURE_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_REGISTERUIRESOURCE_DX12)
                {
                    auto crDesc = (ffxConfigureDescFrameGenerationSwapChainRegisterUiResourceDX12*) next;
                    LOG_DEBUG("UiResource found 2: {:X}", (size_t) crDesc->uiResource.resource);

                    if (fg->FrameGenerationContext() != nullptr && crDesc->uiResource.resource != nullptr)
                    {
                        auto validity = FG_ResourceValidity::UntilPresent;

                        if ((crDesc->flags &
                             FFX_FRAMEGENERATION_UI_COMPOSITION_FLAG_ENABLE_INTERNAL_UI_DOUBLE_BUFFERING) > 0)
                        {
                            LOG_WARN(
                                "FFX_FRAMEGENERATION_UI_COMPOSITION_FLAG_ENABLE_INTERNAL_UI_DOUBLE_BUFFERING is set!");

                            // Not sure which cmdList to use
                            // validity = FG_ResourceValidity::ValidNow;
                        }

                        Dx12Resource ui {};
                        ui.cmdList = nullptr; // Not sure about this
                        ui.height = height;
                        ui.resource = (ID3D12Resource*) crDesc->uiResource.resource;
                        ui.state = GetD3D12State((FfxApiResourceState) crDesc->uiResource.state);
                        ui.type = FG_ResourceType::UIColor;
                        ui.validity = validity; // Not sure about this
                        ui.width = width;
                        ui.left = left;
                        ui.top = top;

                        _uiRes[fg->GetIndex()] = ui;

                        fg->SetResource(&ui);
                    }
                }
            }

            Dx12Resource dfr {};
            dfr.cmdList = nullptr; // Not sure about this
            dfr.height = height;
            dfr.resource = (ID3D12Resource*) crDesc->distortionField.resource;
            dfr.state = GetD3D12State((FfxApiResourceState) crDesc->distortionField.state);
            dfr.type = FG_ResourceType::Distortion;
            dfr.validity = FG_ResourceValidity::UntilPresent; // Not sure about this
            dfr.width = width;
            dfr.left = left;
            dfr.top = top;

            fg->SetResource(&dfr);
        }

        return FFX_API_RETURN_OK;
    }
    else if (desc->type == FFX_API_CONFIGURE_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_REGISTERUIRESOURCE_DX12)
    {
        auto crDesc = (ffxConfigureDescFrameGenerationSwapChainRegisterUiResourceDX12*) desc;
        LOG_DEBUG("UiResource found 3: {:X}", (size_t) crDesc->uiResource.resource);

        if (fg->FrameGenerationContext() != nullptr && crDesc->uiResource.resource != nullptr)
        {
            UINT64 width = 0;
            UINT height = 0;
            UINT left = 0;
            UINT top = 0;

            fg->GetInterpolationRect(width, height);
            fg->GetInterpolationPos(left, top);

            if (width == 0)
            {
                DXGI_SWAP_CHAIN_DESC scDesc {};
                State::Instance().currentFGSwapchain->GetDesc(&scDesc);

                width = scDesc.BufferDesc.Width;
                height = scDesc.BufferDesc.Height;
                top = 0;
                left = 0;
            }

            // Check for FFX_API_CONFIGURE_DESC_TYPE_FRAMEGENERATION_REGISTERDISTORTIONRESOURCE
            ffxConfigureDescHeader* next = nullptr;
            next = desc;
            while (next->pNext != nullptr)
            {
                next = next->pNext;

                if (next->type == FFX_API_CONFIGURE_DESC_TYPE_FRAMEGENERATION_REGISTERDISTORTIONRESOURCE)
                {
                    auto crDesc = (ffxConfigureDescFrameGenerationRegisterDistortionFieldResource*) next;
                    LOG_DEBUG("DistortionFieldResource found: {:X}", (size_t) crDesc->distortionField.resource);

                    if (fg->FrameGenerationContext() != nullptr && crDesc->distortionField.resource != nullptr)
                    {
                        Dx12Resource dfr {};
                        dfr.cmdList = nullptr; // Not sure about this
                        dfr.height = height;
                        dfr.resource = (ID3D12Resource*) crDesc->distortionField.resource;
                        dfr.state = GetD3D12State((FfxApiResourceState) crDesc->distortionField.state);
                        dfr.type = FG_ResourceType::Distortion;
                        dfr.validity = FG_ResourceValidity::UntilPresent; // Not sure about this
                        dfr.width = width;
                        dfr.left = left;
                        dfr.top = top;

                        fg->SetResource(&dfr);
                    }
                }
            }

            Dx12Resource ui {};
            ui.cmdList = nullptr; // Not sure about this
            ui.height = height;
            ui.resource = (ID3D12Resource*) crDesc->uiResource.resource;
            ui.state = GetD3D12State((FfxApiResourceState) crDesc->uiResource.state);
            ui.type = FG_ResourceType::UIColor;
            ui.validity = FG_ResourceValidity::UntilPresent; // Not sure about this
            ui.width = width;
            ui.left = left;
            ui.top = top;

            _uiRes[fg->GetIndex()] = ui;

            fg->SetResource(&ui);
        }

        return FFX_API_RETURN_OK;
    }
    else if (desc->type == FFX_API_CONFIGURE_DESC_TYPE_FRAMEGENERATION_KEYVALUE)
    {
        auto cDesc = (ffxConfigureDescFrameGenerationKeyValue*) desc;
        LOG_WARN("key: {}, u64: {}, ptr: {}", cDesc->key, cDesc->u64, (size_t) cDesc->ptr);
        return FFX_API_RETURN_OK;
    }
    else if (desc->type == FFX_API_CONFIGURE_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_KEYVALUE_DX12)
    {
        auto cDesc = (ffxConfigureDescFrameGenerationSwapChainKeyValueDX12*) desc;
        LOG_WARN("key: {}, u64: {}, ptr: {}", cDesc->key, cDesc->u64, (size_t) cDesc->ptr);
        return FFX_API_RETURN_OK;
    }

    return rcContinue;
}

ffxReturnCode_t ffxQuery_Dx12FG(ffxContext* context, ffxQueryDescHeader* desc)
{
#ifdef PASSTHRU
    return (ffxReturnCode_t) 0xFFFFFFFF;
#endif

    LOG_DEBUG();

    if (desc->type == FFX_API_QUERY_DESC_TYPE_FRAMEGENERATION_GPU_MEMORY_USAGE)
    {
        auto cDesc = (ffxQueryDescFrameGenerationGetGPUMemoryUsage*) desc;
        cDesc->gpuMemoryUsageFrameGeneration->aliasableUsageInBytes = 32768;
        cDesc->gpuMemoryUsageFrameGeneration->totalUsageInBytes = 32768;

        return FFX_API_RETURN_OK;
    }
    else if (desc->type == FFX_API_QUERY_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_INTERPOLATIONCOMMANDLIST_DX12)
    {
        auto cDesc = (ffxQueryDescFrameGenerationSwapChainInterpolationCommandListDX12*) desc;
        auto fg = State::Instance().currentFG;

        if (fg != nullptr)
        {
            *cDesc->pOutCommandList = fg->GetUICommandList();
            LOG_DEBUG("Returning cmdList: {:X}", (size_t) *cDesc->pOutCommandList);
        }

        return FFX_API_RETURN_OK;
    }
    else if (desc->type == FFX_API_QUERY_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_INTERPOLATIONTEXTURE_DX12)
    {
        auto fg = State::Instance().currentFG;
        if (fg != nullptr)
        {
            auto cDesc = (ffxQueryDescFrameGenerationSwapChainInterpolationTextureDX12*) desc;
            auto fIndex = fg->GetIndex();

            IDXGISwapChain3* sc = (IDXGISwapChain3*) State::Instance().currentFGSwapchain;
            auto scIndex = sc->GetCurrentBackBufferIndex();

            ID3D12Resource* currentBuffer = nullptr;
            auto hr = sc->GetBuffer(scIndex, IID_PPV_ARGS(&currentBuffer));
            if (hr != S_OK)
            {
                LOG_ERROR("sc->GetBuffer error: {:X}", (UINT) hr);
                return {};
            }

            if (currentBuffer == nullptr)
            {
                LOG_ERROR("currentBuffer is nullptr!");
                return {};
            }

            currentBuffer->SetName(std::format(L"currentBuffer[{}]", scIndex).c_str());
            currentBuffer->Release();

            if (CreateBufferResource(_device, currentBuffer, D3D12_RESOURCE_STATE_COMMON, &_interpolation[fIndex]))
                _interpolation[fIndex]->SetName(std::format(L"_interpolation[{}]", fIndex).c_str());

            *cDesc->pOutTexture = ffxApiGetResourceDX12(_interpolation[fIndex], FFX_API_RESOURCE_STATE_COMMON);
        }

        return FFX_API_RETURN_OK;
    }
    else if (desc->type == FFX_API_QUERY_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_GPU_MEMORY_USAGE_DX12)
    {
        auto cDesc = (ffxQueryFrameGenerationSwapChainGetGPUMemoryUsageDX12*) desc;
        cDesc->gpuMemoryUsageFrameGenerationSwapchain->aliasableUsageInBytes = 32768;
        cDesc->gpuMemoryUsageFrameGenerationSwapchain->totalUsageInBytes = 32768;

        return FFX_API_RETURN_OK;
    }

    return rcContinue;
}

ffxReturnCode_t ffxDispatch_Dx12FG(ffxContext* context, ffxDispatchDescHeader* desc)
{
#ifdef PASSTHRU
    return (ffxReturnCode_t) 0xFFFFFFFF;
#endif

    auto fg = State::Instance().currentFG;

    if (fg == nullptr)
    {
        LOG_ERROR("No FG Feature!");
        return FFX_API_RETURN_ERROR_RUNTIME_ERROR;
    }

    if (desc->type == FFX_API_DISPATCH_DESC_TYPE_FRAMEGENERATION)
    {
        auto cdDesc = (ffxDispatchDescFrameGeneration*) desc;

        auto fg = State::Instance().currentFG;
        if (fg != nullptr)
        {
            fg->SetInterpolationPos(cdDesc->generationRect.left, cdDesc->generationRect.top);
            fg->SetInterpolationRect(cdDesc->generationRect.width, cdDesc->generationRect.height);
            fg->SetReset(cdDesc->reset ? 1 : 0);

            if (cdDesc->presentColor.resource != nullptr && fg->GetResource(FG_ResourceType::HudlessColor) == nullptr)
            {
                UINT width = cdDesc->generationRect.width;
                UINT height = cdDesc->generationRect.height;
                UINT left = cdDesc->generationRect.left;
                UINT top = cdDesc->generationRect.top;

                if (width == 0)
                {
                    DXGI_SWAP_CHAIN_DESC scDesc {};
                    State::Instance().currentFGSwapchain->GetDesc(&scDesc);

                    width = scDesc.BufferDesc.Width;
                    height = scDesc.BufferDesc.Height;
                    top = 0;
                    left = 0;
                }

                Dx12Resource hudless {};
                hudless.cmdList = (ID3D12GraphicsCommandList*) cdDesc->commandList;
                hudless.height = height;
                hudless.resource = (ID3D12Resource*) cdDesc->presentColor.resource;
                hudless.state = GetD3D12State((FfxApiResourceState) cdDesc->presentColor.state);
                hudless.type = FG_ResourceType::HudlessColor;
                hudless.validity = FG_ResourceValidity::UntilPresent;
                hudless.width = width;
                hudless.top = top;
                hudless.left = left;
                fg->SetResource(&hudless);
            }
        }

        LOG_DEBUG("FFX_API_DISPATCH_DESC_TYPE_FRAMEGENERATION");
        return FFX_API_RETURN_OK;
    }
    else if (desc->type == FFX_API_DISPATCH_DESC_TYPE_FRAMEGENERATION_PREPARE)
    {
        auto cdDesc = (ffxDispatchDescFrameGenerationPrepare*) desc;
        LOG_DEBUG("DISPATCH_DESC_TYPE_FRAMEGENERATION_PREPARE, frameID: {}", cdDesc->frameID);

        {
            std::lock_guard<std::mutex> lock(_newFrameMutex);
            if (cdDesc->frameID > _currentFrameId)
            {
                _currentFrameId = cdDesc->frameID;
                fg->StartNewFrame();
                _uiRes[fg->GetIndex()] = {};
            }
        }

        auto device = _device == nullptr ? State::Instance().currentD3D12Device : _device;
        fg->EvaluateState(device, _fgConst);

        if (!fg->IsActive() || fg->IsPaused())
            return FFX_API_RETURN_OK;

        //  Camera Data
        bool cameraDataFound = false;
        ffxDispatchDescHeader* next = nullptr;
        next = desc;
        while (next->pNext != nullptr)
        {
            next = next->pNext;

            if (next->type == FFX_API_DISPATCH_DESC_TYPE_FRAMEGENERATION_PREPARE_CAMERAINFO)
            {
                auto cameraDesc = (ffxDispatchDescFrameGenerationPrepareCameraInfo*) next;

                fg->SetCameraData(cameraDesc->cameraPosition, cameraDesc->cameraUp, cameraDesc->cameraRight,
                                  cameraDesc->cameraForward);

                cameraDataFound = true;
                break;
            }
        }

        // Camera Values
        UINT64 dispWidth = 0;
        UINT dispHeight = 0;
        fg->GetInterpolationRect(dispWidth, dispHeight);
        auto aspectRatio = (float) dispWidth / (float) dispHeight;
        fg->SetCameraValues(cdDesc->cameraNear, cdDesc->cameraFar, cdDesc->cameraFovAngleVertical, aspectRatio);

        // Other values
        fg->SetFrameTimeDelta(cdDesc->frameTimeDelta);
        fg->SetJitter(cdDesc->jitterOffset.x, cdDesc->jitterOffset.y);
        fg->SetMVScale(cdDesc->motionVectorScale.x, cdDesc->motionVectorScale.y);
        fg->SetReset(cdDesc->unused_reset ? 1 : 0);

        if (cdDesc->depth.resource != nullptr)
        {
            Dx12Resource depth {};
            depth.cmdList = (ID3D12GraphicsCommandList*) cdDesc->commandList;
            depth.height = cdDesc->renderSize.height; // cdDesc->depth.description.width;
            depth.resource = (ID3D12Resource*) cdDesc->depth.resource;
            depth.state = GetD3D12State((FfxApiResourceState) cdDesc->depth.state);
            depth.type = FG_ResourceType::Depth;
            depth.validity = FG_ResourceValidity::JustTrackCmdlist;
            depth.width = cdDesc->renderSize.width; // cdDesc->depth.description.height;

            fg->SetResource(&depth);
        }

        if (cdDesc->motionVectors.resource != nullptr)
        {
            uint32_t width = 0;
            uint32_t height = 0;

            if (_fgConst.flags & FG_Flags::DisplayResolutionMVs)
            {
                width = _fgConst.displayWidth;
                height = _fgConst.displayHeight;
            }
            else
            {
                width = cdDesc->renderSize.width;
                height = cdDesc->renderSize.height;
            }

            Dx12Resource velocity {};
            velocity.cmdList = (ID3D12GraphicsCommandList*) cdDesc->commandList;
            velocity.height = height; // cdDesc->motionVectors.description.width;
            velocity.resource = (ID3D12Resource*) cdDesc->motionVectors.resource;
            velocity.state = GetD3D12State((FfxApiResourceState) cdDesc->motionVectors.state);
            velocity.type = FG_ResourceType::Velocity;
            velocity.validity = FG_ResourceValidity::JustTrackCmdlist;
            velocity.width = width; // cdDesc->motionVectors.description.height;

            fg->SetResource(&velocity);
        }

        LOG_DEBUG("DISPATCH_DESC_TYPE_FRAMEGENERATION_PREPARE done");
        return FFX_API_RETURN_OK;
    }
    else if (desc->type == FFX_API_DISPATCH_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_WAIT_FOR_PRESENTS_DX12)
    {
        auto cdDesc = (ffxDispatchDescFrameGenerationSwapChainWaitForPresentsDX12*) desc;
        return FFX_API_RETURN_OK;
    }

    return rcContinue;
}

void ffxPresentCallback()
{
#ifdef PASSTHRU
    return;
#endif

    LOG_DEBUG("");

    if (_presentCallback == nullptr && _fgCallback == nullptr)
        return;

    auto fg = State::Instance().currentFG;

    if (fg == nullptr)
        return;

    auto fIndex = fg->GetIndex();
    auto cmdList = fg->GetUICommandList(fIndex);
    ID3D12Resource* currentBuffer = nullptr;

    if (_presentCallback != nullptr)
    {
        ffxCallbackDescFrameGenerationPresent cdfgp {};
        cdfgp.header.type = FFX_API_CALLBACK_DESC_TYPE_FRAMEGENERATION_PRESENT;
        cdfgp.frameID = _callbackFrameId;
        cdfgp.device = _device;
        cdfgp.isGeneratedFrame = false;

        IDXGISwapChain3* sc = (IDXGISwapChain3*) State::Instance().currentFGSwapchain;
        auto scIndex = sc->GetCurrentBackBufferIndex();

        ID3D12Resource* currentBuffer = nullptr;
        auto hr = sc->GetBuffer(scIndex, IID_PPV_ARGS(&currentBuffer));
        if (hr != S_OK)
        {
            LOG_ERROR("sc->GetBuffer error: {:X}", (UINT) hr);
            return;
        }

        if (currentBuffer == nullptr)
        {
            LOG_ERROR("currentBuffer is nullptr!");
            return;
        }

        currentBuffer->SetName(std::format(L"currentBuffer[{}]", scIndex).c_str());

        if (CreateBufferResource(_device, currentBuffer, D3D12_RESOURCE_STATE_COMMON, &_hudless[fIndex]))
            _hudless[fIndex]->SetName(std::format(L"_hudless[{}]", fIndex).c_str());
        else
            return;

        cdfgp.commandList = cmdList;

        ResourceBarrier(cmdList, currentBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_SOURCE);
        ResourceBarrier(cmdList, _hudless[fIndex], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                        D3D12_RESOURCE_STATE_COPY_DEST);

        cmdList->CopyResource(_hudless[fIndex], currentBuffer);

        ResourceBarrier(cmdList, currentBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PRESENT);
        ResourceBarrier(cmdList, _hudless[fIndex], D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);

        cdfgp.outputSwapChainBuffer =
            ffxApiGetResourceDX12(currentBuffer, FFX_API_RESOURCE_STATE_PRESENT, FFX_API_RESOURCE_USAGE_UAV);
        cdfgp.currentBackBuffer =
            ffxApiGetResourceDX12(_hudless[fIndex], FFX_API_RESOURCE_STATE_PRESENT, FFX_API_RESOURCE_USAGE_UAV);

        if (_uiRes[fIndex].resource != nullptr)
            cdfgp.currentUI = ffxApiGetResourceDX12(_uiRes[fIndex].resource, GetFfxApiState(_uiRes[fIndex].state));

        auto result = _presentCallback(&cdfgp, _presentCallbackUserContext);

        ResourceBarrier(cmdList, _hudless[fIndex], D3D12_RESOURCE_STATE_COMMON,
                        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        if (result == FFX_API_RETURN_OK)
        {
            if (fg->GetResource(FG_ResourceType::HudlessColor, fIndex) == nullptr)
            {
                auto hDesc = _hudless[fIndex]->GetDesc();
                Dx12Resource hudless {};
                hudless.cmdList = cmdList;
                hudless.height = hDesc.Height;
                hudless.resource = _hudless[fIndex];
                hudless.state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
                hudless.type = FG_ResourceType::HudlessColor;
                hudless.validity = FG_ResourceValidity::ValidNow;
                hudless.width = hDesc.Width;
                fg->SetResource(&hudless);
            }
        }

        currentBuffer->Release();

        _presentCallback = nullptr;
        _presentCallbackUserContext = nullptr;
    }

    if (_fgCallback != nullptr)
    {
        ffxDispatchDescFrameGeneration ddfg {};
        ddfg.header.type = FFX_API_DISPATCH_DESC_TYPE_FRAMEGENERATION;
        ddfg.frameID = _callbackFrameId;
        ddfg.generationRect = _callbackRect;
        ddfg.numGeneratedFrames = 1;

        if (currentBuffer == nullptr)
        {
            IDXGISwapChain3* sc = (IDXGISwapChain3*) State::Instance().currentFGSwapchain;
            auto scIndex = sc->GetCurrentBackBufferIndex();

            auto hr = sc->GetBuffer(scIndex, IID_PPV_ARGS(&currentBuffer));
            if (hr != S_OK)
            {
                LOG_ERROR("sc->GetBuffer error: {:X}", (UINT) hr);
                return;
            }

            currentBuffer->SetName(std::format(L"currentBuffer[{}]", scIndex).c_str());
            if (currentBuffer == nullptr)
            {
                LOG_ERROR("currentBuffer is nullptr!");
                return;
            }
        }

        if (CreateBufferResource(_device, currentBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &_hudless[fIndex]))
            _hudless[fIndex]->SetName(std::format(L"_hudless[{}]", fIndex).c_str());
        else
            return;

        ddfg.commandList = cmdList;

        ResourceBarrier(cmdList, currentBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_SOURCE);
        ResourceBarrier(cmdList, _hudless[fIndex], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                        D3D12_RESOURCE_STATE_COPY_DEST);

        cmdList->CopyResource(_hudless[fIndex], currentBuffer);

        ResourceBarrier(cmdList, currentBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE,
                        D3D12_RESOURCE_STATE_COPY_SOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        ResourceBarrier(cmdList, _hudless[fIndex], D3D12_RESOURCE_STATE_COPY_DEST,
                        D3D12_RESOURCE_STATE_COPY_SOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        ddfg.outputs[0] = ffxApiGetResourceDX12(currentBuffer, FFX_API_RESOURCE_STATE_GENERIC_READ,
                                                FFX_API_RESOURCE_USAGE_RENDERTARGET);
        ddfg.presentColor = ffxApiGetResourceDX12(_hudless[fIndex], FFX_API_RESOURCE_STATE_GENERIC_READ);
        ddfg.reset = false;

        auto result = _fgCallback(&ddfg, _fgCallbackUserContext);

        ResourceBarrier(cmdList, currentBuffer,
                        D3D12_RESOURCE_STATE_COPY_SOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                        D3D12_RESOURCE_STATE_PRESENT);
        ResourceBarrier(cmdList, _hudless[fIndex],
                        D3D12_RESOURCE_STATE_COPY_SOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        if (result == FFX_API_RETURN_OK)
        {
            if (fg->GetResource(FG_ResourceType::HudlessColor, fIndex) == nullptr)
            {
                auto hDesc = _hudless[fIndex]->GetDesc();
                Dx12Resource hudless {};
                hudless.cmdList = cmdList;
                hudless.height = hDesc.Height;
                hudless.resource = _hudless[fIndex];
                hudless.state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
                hudless.type = FG_ResourceType::HudlessColor;
                hudless.validity = FG_ResourceValidity::ValidNow;
                hudless.width = hDesc.Width;
                fg->SetResource(&hudless);
            }
        }

        currentBuffer->Release();

        _fgCallback = nullptr;
        _fgCallbackUserContext = nullptr;
    }
}
