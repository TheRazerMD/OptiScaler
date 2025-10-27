#include "Hudfix_Dx12.h"

#include <Util.h>
#include <State.h>
#include <Config.h>

#include <framegen/IFGFeature_Dx12.h>

bool Hudfix_Dx12::CreateObjects()
{
    if (_commandQueue != nullptr)
        return false;

    do
    {
        HRESULT result;

        for (size_t i = 0; i < BUFFER_COUNT; i++)
        {
            result = State::Instance().currentD3D12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                                                  IID_PPV_ARGS(&_commandAllocator[i]));
            if (result != S_OK)
            {
                LOG_ERROR("CreateCommandAllocator: {:X}", (unsigned long) result);
                break;
            }
            _commandAllocator[i]->SetName(L"Hudfix CommandAllocator");

            result = State::Instance().currentD3D12Device->CreateCommandList(
                0, D3D12_COMMAND_LIST_TYPE_DIRECT, _commandAllocator[i], NULL, IID_PPV_ARGS(&_commandList[i]));
            if (result != S_OK)
            {
                LOG_ERROR("CreateCommandList: {:X}", (unsigned long) result);
                break;
            }

            _commandList[i]->SetName(L"Hudfix CommandList");

            result = _commandList[i]->Close();
            if (result != S_OK)
            {
                LOG_ERROR("_hudlessCommandList->Close: {:X}", (unsigned long) result);
                break;
            }

            result =
                State::Instance().currentD3D12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence[i]));
            if (result != S_OK)
            {
                LOG_ERROR("CreateFence: {0:X}", (unsigned long) result);
                break;
            }
        }

        // Create a command queue for frame generation
        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queueDesc.NodeMask = 0;
        queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_HIGH;

        HRESULT hr = State::Instance().currentD3D12Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&_commandQueue));
        if (hr != S_OK)
        {
            LOG_ERROR("CreateCommandQueue: {:X}", (unsigned long) hr);
            break;
        }

        _commandQueue->SetName(L"Hudfix CommandQueue");

        return true;

    } while (false);

    return false;
}

bool Hudfix_Dx12::CreateBufferResource(ID3D12Device* InDevice, ResourceInfo* InSource, D3D12_RESOURCE_STATES InState,
                                       ID3D12Resource** OutResource)
{
    if (InDevice == nullptr || InSource == nullptr)
        return false;

    if (*OutResource != nullptr)
    {
        auto bufDesc = (*OutResource)->GetDesc();

        if (bufDesc.Width != (UINT64) (InSource->width) || bufDesc.Height != (UINT) (InSource->height) ||
            bufDesc.Format != InSource->format)
        {
            (*OutResource)->Release();
            (*OutResource) = nullptr;
            LOG_WARN("Release {}x{}, new one: {}x{}", bufDesc.Width, bufDesc.Height, InSource->width, InSource->height);
        }
        else
        {
            return true;
        }
    }

    D3D12_HEAP_PROPERTIES heapProperties;
    D3D12_HEAP_FLAGS heapFlags;
    HRESULT hr = InSource->buffer->GetHeapProperties(&heapProperties, &heapFlags);

    if (hr != S_OK)
    {
        LOG_ERROR("GetHeapProperties result: {:X}", (UINT64) hr);
        return false;
    }

    D3D12_RESOURCE_DESC texDesc = InSource->buffer->GetDesc();
    texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    hr = InDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &texDesc, InState, nullptr,
                                           IID_PPV_ARGS(OutResource));

    if (hr != S_OK)
    {
        LOG_ERROR("CreateCommittedResource result: {:X}", (UINT64) hr);
        return false;
    }

    LOG_DEBUG("Created new one: {}x{}", texDesc.Width, texDesc.Height);
    return true;
}

bool Hudfix_Dx12::CreateBufferResourceWithSize(ID3D12Device* InDevice, ResourceInfo* InSource,
                                               D3D12_RESOURCE_STATES InState, ID3D12Resource** OutResource,
                                               UINT InWidth, UINT InHeight)
{
    if (InDevice == nullptr || InSource == nullptr)
        return false;

    if (*OutResource != nullptr)
    {
        auto bufDesc = (*OutResource)->GetDesc();

        if (bufDesc.Width != (UINT64) InWidth || bufDesc.Height != InHeight || bufDesc.Format != InSource->format)
        {
            (*OutResource)->Release();
            (*OutResource) = nullptr;
            LOG_WARN("Release {}x{}, new one: {}x{}", bufDesc.Width, bufDesc.Height, InWidth, InHeight);
        }
        else
        {
            return true;
        }
    }

    D3D12_HEAP_PROPERTIES heapProperties;
    D3D12_HEAP_FLAGS heapFlags;
    HRESULT hr = InSource->buffer->GetHeapProperties(&heapProperties, &heapFlags);

    if (hr != S_OK)
    {
        LOG_ERROR("GetHeapProperties result: {:X}", (UINT64) hr);
        return false;
    }

    D3D12_RESOURCE_DESC texDesc = InSource->buffer->GetDesc();
    texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    texDesc.Width = InWidth;
    texDesc.Height = InHeight;

    hr = InDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &texDesc, InState, nullptr,
                                           IID_PPV_ARGS(OutResource));

    if (hr != S_OK)
    {
        LOG_ERROR("CreateCommittedResource result: {:X}", (UINT64) hr);
        return false;
    }

    LOG_DEBUG("Created new one: {}x{}", InWidth, InHeight);
    return true;
}

void Hudfix_Dx12::ResourceBarrier(ID3D12GraphicsCommandList* InCommandList, ID3D12Resource* InResource,
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

bool Hudfix_Dx12::CheckCapture()
{
    auto fIndex = GetIndex();

    // early exit
    if (_captureCounter[fIndex] > 999)
    {
        LOG_DEBUG("_captureCounter[{}] > 999", fIndex);
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(_counterMutex);
        _captureCounter[fIndex]++;

        LOG_TRACE("frameCounter: {}, _captureCounter: {}, Limit: {}", State::Instance().currentFeature->FrameCount(),
                  _captureCounter[fIndex], Config::Instance()->FGHUDLimit.value_or_default());

        if (_captureCounter[fIndex] > 999 ||
            _captureCounter[fIndex] != Config::Instance()->FGHUDLimit.value_or_default())
            return false;
    }

    return true;
}

inline static std::string GetSourceString(UINT source)
{
    switch (source)
    {
    case CaptureInfo::CreateRTV:
        return "RTV";
    case CaptureInfo::CreateSRV:
        return "SRV";
    case CaptureInfo::CreateUAV:
        return "UAV";
    case CaptureInfo::OMSetRTV:
        return "OM";
    case CaptureInfo::Upscaler:
        return "Ups";
    case CaptureInfo::SetCR:
        return "SCR";
    case CaptureInfo::SetGR:
        return "SGR";
    default:
        return std::format("{}", source);
    }
}

inline static std::string GetDispatchString(UINT source)
{
    switch (source)
    {
    case CaptureInfo::DrawInstanced:
        return "DI";
    case CaptureInfo::DrawIndexedInstanced:
        return "DII";
    case CaptureInfo::Dispatch:
        return "Disp";
    default:
        return std::format("{}", source);
    }
}

bool Hudfix_Dx12::CheckResource(ResourceInfo* resource)
{
    if (resource == nullptr || resource->buffer == nullptr || State::Instance().isShuttingDown)
        return false;

    if (State::Instance().FGonlyUseCapturedResources)
    {
        auto result = _captureList.find(resource->buffer) != _captureList.end();
        return true;
    }

    auto& s = State::Instance();

    // There are all these chacks because looks like ResTracker is still missing some resources
    // Need check more docs about D3D12 resource/heap usage

    // Compare aganist stored info first
    if (resource->width == 0 || resource->height == 0 || resource->buffer == nullptr)
        return false;

    // Get resource info
    auto resDesc = resource->buffer->GetDesc();

    // dimensions not match
    if (resDesc.Height != s.currentSwapchainDesc.BufferDesc.Height ||
        resDesc.Width != s.currentSwapchainDesc.BufferDesc.Width)
    {
        // Extended size check
        if (!(Config::Instance()->FGRelaxedResolutionCheck.value_or_default() &&
              resDesc.Height >= s.currentSwapchainDesc.BufferDesc.Height - 32 &&
              resDesc.Height <= s.currentSwapchainDesc.BufferDesc.Height + 32 &&
              resDesc.Width >= s.currentSwapchainDesc.BufferDesc.Width - 32 &&
              resDesc.Width <= s.currentSwapchainDesc.BufferDesc.Width + 32))
        {
            return false;
        }

        resource->extended = true;
    }

    // check for resource flags
    if ((resDesc.Flags & (D3D12_RESOURCE_FLAG_RAYTRACING_ACCELERATION_STRUCTURE |
                          D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | D3D12_RESOURCE_FLAG_VIDEO_DECODE_REFERENCE_ONLY |
                          D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE | D3D12_RESOURCE_FLAG_VIDEO_ENCODE_REFERENCE_ONLY)) >
        0)
    {
        return false;
    }

    std::string caller;
    auto source = resource->captureInfo & 0xFF;
    auto dispatcher = resource->captureInfo & 0xFF00;

    // format match
    if (resDesc.Format == s.currentSwapchainDesc.BufferDesc.Format)
    {
        LOG_DEBUG("{}->{} Width: {}/{}, Height: {}/{}, Format: {}/{}, Resource: {:X}, convertFormat: {} -> TRUE",
                  GetSourceString(source), GetDispatchString(dispatcher), resDesc.Width,
                  s.currentSwapchainDesc.BufferDesc.Width, resDesc.Height, s.currentSwapchainDesc.BufferDesc.Height,
                  (UINT) resDesc.Format, (UINT) s.currentSwapchainDesc.BufferDesc.Format, (size_t) resource->buffer,
                  Config::Instance()->FGHUDFixExtended.value_or_default());

        return true;
    }

    // extended not active
    if (!Config::Instance()->FGHUDFixExtended.value_or_default())
    {
        return false;
    }

    // resource and target formats are supported by converter
    if ((resDesc.Format == DXGI_FORMAT_R10G10B10A2_UNORM || resDesc.Format == DXGI_FORMAT_R10G10B10A2_TYPELESS ||
         resDesc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT || resDesc.Format == DXGI_FORMAT_R16G16B16A16_TYPELESS ||
         resDesc.Format == DXGI_FORMAT_R11G11B10_FLOAT || resDesc.Format == DXGI_FORMAT_R32G32B32A32_FLOAT ||
         resDesc.Format == DXGI_FORMAT_R32G32B32A32_TYPELESS || resDesc.Format == DXGI_FORMAT_R32G32B32_FLOAT ||
         resDesc.Format == DXGI_FORMAT_R32G32B32_TYPELESS || resDesc.Format == DXGI_FORMAT_R8G8B8A8_TYPELESS ||
         resDesc.Format == DXGI_FORMAT_R8G8B8A8_UNORM || resDesc.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB ||
         resDesc.Format == DXGI_FORMAT_B8G8R8A8_TYPELESS || resDesc.Format == DXGI_FORMAT_B8G8R8A8_UNORM ||
         resDesc.Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) &&
        (s.currentSwapchainDesc.BufferDesc.Format == DXGI_FORMAT_R10G10B10A2_UNORM ||
         s.currentSwapchainDesc.BufferDesc.Format == DXGI_FORMAT_R10G10B10A2_TYPELESS ||
         s.currentSwapchainDesc.BufferDesc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT ||
         s.currentSwapchainDesc.BufferDesc.Format == DXGI_FORMAT_R16G16B16A16_TYPELESS ||
         s.currentSwapchainDesc.BufferDesc.Format == DXGI_FORMAT_R11G11B10_FLOAT ||
         s.currentSwapchainDesc.BufferDesc.Format == DXGI_FORMAT_R32G32B32A32_FLOAT ||
         s.currentSwapchainDesc.BufferDesc.Format == DXGI_FORMAT_R32G32B32A32_TYPELESS ||
         s.currentSwapchainDesc.BufferDesc.Format == DXGI_FORMAT_R32G32B32_FLOAT ||
         s.currentSwapchainDesc.BufferDesc.Format == DXGI_FORMAT_R32G32B32_TYPELESS ||
         s.currentSwapchainDesc.BufferDesc.Format == DXGI_FORMAT_R8G8B8A8_TYPELESS ||
         s.currentSwapchainDesc.BufferDesc.Format == DXGI_FORMAT_R8G8B8A8_UNORM ||
         s.currentSwapchainDesc.BufferDesc.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB ||
         s.currentSwapchainDesc.BufferDesc.Format == DXGI_FORMAT_B8G8R8A8_TYPELESS ||
         s.currentSwapchainDesc.BufferDesc.Format == DXGI_FORMAT_B8G8R8A8_UNORM ||
         s.currentSwapchainDesc.BufferDesc.Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB))
    {
        LOG_DEBUG("{}->{} Width: {}/{}, Height: {}/{}, Format: {}/{}, Resource: {:X}, convertFormat: {} -> TRUE",
                  GetSourceString(source), GetDispatchString(dispatcher), resDesc.Width,
                  s.currentSwapchainDesc.BufferDesc.Width, resDesc.Height, s.currentSwapchainDesc.BufferDesc.Height,
                  (UINT) resDesc.Format, (UINT) s.currentSwapchainDesc.BufferDesc.Format, (size_t) resource->buffer,
                  Config::Instance()->FGHUDFixExtended.value_or_default());

        return true;
    }

    return false;
}

int Hudfix_Dx12::GetIndex() { return _upscaleCounter % BUFFER_COUNT; }

void Hudfix_Dx12::HudlessFound(ID3D12GraphicsCommandList* cmdList)
{
    LOG_DEBUG("_upscaleCounter: {}, _fgCounter: {}", _upscaleCounter, _fgCounter);

    std::lock_guard<std::mutex> lock(_counterMutex);

    auto index = GetIndex();
    if (_captureCounter[index] > 1000)
        return;

    // Set it above 1000 to prvent capture
    _captureCounter[index] = 9999;

    // auto fg = State::Instance().currentFG;
    // if (fg != nullptr)
    //     fg->Dispatch();

    // Increase counter
    _fgCounter = _upscaleCounter;

    _skipHudlessChecks = false;
}

bool Hudfix_Dx12::CheckForRealObject(std::string functionName, IUnknown* pObject, IUnknown** ppRealObject)
{
    // return false;

    if (streamlineRiid.Data1 == 0)
    {
        auto iidResult = IIDFromString(L"{ADEC44E2-61F0-45C3-AD9F-1B37379284FF}", &streamlineRiid);

        if (iidResult != S_OK)
            return false;
    }

    auto qResult = pObject->QueryInterface(streamlineRiid, (void**) ppRealObject);

    if (qResult == S_OK && *ppRealObject != nullptr)
    {
        LOG_INFO("{} Streamline proxy found!", functionName);
        (*ppRealObject)->Release();
        return true;
    }

    return false;
}

void Hudfix_Dx12::UpscaleStart()
{
    if (State::Instance().FGresetCapturedResources)
    {
        std::lock_guard<std::mutex> lock(_captureMutex);
        _captureList.clear();
        LOG_DEBUG("FGResetCapturedResources");
        State::Instance().FGresetCapturedResources = false;
        State::Instance().FGcapturedResourceCount = 0;
        State::Instance().FGresetCapturedResources = false;
    }

    if (State::Instance().ClearCapturedHudlesses)
    {
        State::Instance().ClearCapturedHudlesses = false;
        State::Instance().CapturedHudlesses.clear();
    }
}

void Hudfix_Dx12::UpscaleEnd(UINT64 frameId, double lastFGFrameTime)
{

    // Update counter after upscaling so _upscaleCounter == _fgCounter check at IsResourceCheckActive will work
    _upscaleCounter++; // = frameId;
    _frameTime = lastFGFrameTime;

    // Get new index and clear resources
    auto index = GetIndex();
    _captureCounter[index] = 0;
}

void Hudfix_Dx12::PresentStart()
{
    _fgCounter = _upscaleCounter;
    return;
}

void Hudfix_Dx12::PresentEnd() { LOG_DEBUG(""); }

UINT64 Hudfix_Dx12::ActiveUpscaleFrame() { return _upscaleCounter; }

UINT64 Hudfix_Dx12::ActivePresentFrame() { return _fgCounter; }

bool Hudfix_Dx12::IsResourceCheckActive()
{
    if (State::Instance().isShuttingDown)
    {
        // LOG_TRACK("State::Instance().isShuttingDown");
        return false;
    }

    if (_upscaleCounter <= _fgCounter)
    {
        // LOG_TRACK("_upscaleCounter <= _fgCounter: {} <= {}", _upscaleCounter, _fgCounter);
        return false;
    }

    if (!Config::Instance()->FGEnabled.value_or_default() || !Config::Instance()->FGHUDFix.value_or_default())
    {
        // LOG_TRACK(
        //     "!Config::Instance()->FGEnabled.value_or_default() || !Config::Instance()->FGHUDFix.value_or_default()");
        return false;
    }

    if (State::Instance().currentFeature == nullptr || State::Instance().currentFG == nullptr)
    {
        // LOG_TRACK("State::Instance().currentFeature == nullptr || State::Instance().currentFG == nullptr");
        return false;
    }

    if (!State::Instance().currentFG->IsActive() || State::Instance().FGchanged)
    {
        // LOG_TRACK("!State::Instance().currentFG->IsActive() || State::Instance().FGchanged");
        return false;
    }

    auto fg = reinterpret_cast<IFGFeature_Dx12*>(State::Instance().currentFG);
    if (fg == nullptr)
    {
        // LOG_TRACK("fg == nullptr");
        return false;
    }

    return true;
}

bool Hudfix_Dx12::SkipHudlessChecks() { return _skipHudlessChecks; }

bool Hudfix_Dx12::CheckForHudless(ID3D12GraphicsCommandList* cmdList, ResourceInfo* resource,
                                  D3D12_RESOURCE_STATES state, bool ignoreBlocked)
{
    auto& s = State::Instance();

    if (s.currentFG == nullptr)
        return false;

    if (!IsResourceCheckActive())
        return false;

    do
    {
        if (!CheckResource(resource))
            break;

        CapturedHudlessInfo* capturedHudlessInfo = &s.CapturedHudlesses[resource->buffer];
        if (capturedHudlessInfo != nullptr && !capturedHudlessInfo->enabled)
        {
            LOG_DEBUG("Skipping {:X}, disabled from captured hudless list!", (size_t) resource->buffer);
            break;
        }

        DXGI_SWAP_CHAIN_DESC scDesc {};
        if (s.currentSwapchain->GetDesc(&scDesc) != S_OK)
        {
            LOG_WARN("Can't get swapchain desc!");
            break;
        }

        // Prevent double capture
        LOG_DEBUG("Waiting _checkMutex");
        std::lock_guard<std::mutex> lock(_checkMutex);

        if (!ignoreBlocked && Config::Instance()->FGResourceBlocking.value_or_default())
        {
            if (_hudlessList.contains(resource->buffer))
            {
                auto info = &_hudlessList[resource->buffer];

                // if game starts reusing the ignored resource & it's not banned
                if (info->ignore && !info->dontReuse)
                {
                    // check resource once per frame
                    if (info->lastTriedFrame != _upscaleCounter)
                    {
                        // start retry period
                        if (info->retryStartFrame == 0)
                        {
                            LOG_WARN("Retry for {:X} as hudless, current frame: {}", (size_t) resource->buffer,
                                     _upscaleCounter);
                            info->retryStartFrame = _upscaleCounter;
                            info->lastTriedFrame = _upscaleCounter;
                            info->retryCount = 0;
                            break;
                        }

                        info->retryCount++;
                        info->lastTriedFrame = _upscaleCounter;

                        // If still in retry period (70 frames)
                        if ((_upscaleCounter - info->retryStartFrame) < 69)
                        {
                            // and used at least 20 times (around every 3rd frame)
                            // try reusing the resource
                            if (info->retryCount > 19)
                            {
                                LOG_WARN("Reusing {:X} as hudless, retry start frame: {}, current frame: {}, reuse "
                                         "count: {}",
                                         (size_t) resource->buffer, info->retryStartFrame, _upscaleCounter,
                                         info->retryCount);

                                info->lastUsedFrame = _upscaleCounter;
                                info->retryStartFrame = 0;
                                info->useCount = 0;
                                info->retryCount = 0;
                                info->ignore = false;
                                info->reuseCount++;
                            }
                        }
                        else
                        {
                            // Retry period ended without success, reset values

                            LOG_WARN("Retry failed for {:X} as hudless, current frame: {}", (size_t) resource->buffer,
                                     _upscaleCounter);

                            info->useCount = 0;
                            info->retryCount = 0;
                            info->retryStartFrame = 0;
                        }
                    }
                }

                // directly ignore
                if (info->ignore)
                    break;

                // if buffer is not used in last 5 frames stop using it
                if ((_upscaleCounter - info->lastUsedFrame) > 6 && info->useCount < 100)
                {
                    LOG_WARN("Blocked {:X} as hudless, last used frame: {}, current frame: {}, use count: {}",
                             (size_t) resource->buffer, info->lastUsedFrame, _upscaleCounter, info->useCount);

                    info->ignore = true;
                    info->retryCount = 0;
                    info->lastTriedFrame = 0;
                    info->retryStartFrame = 0;
                    info->lastUsedFrame = _upscaleCounter;

                    // don't reuse more than 2 times
                    if (info->reuseCount > 1)
                        info->dontReuse = true;

                    break;
                }

                // update the info
                info->lastUsedFrame = _upscaleCounter;
                info->useCount++;
            }
            else
            {
                _hudlessList[resource->buffer] = { _upscaleCounter, 0, 0, 0, 0, 1, false, false };
            }
        }

        if (!CheckCapture())
            break;

        auto fIndex = GetIndex();

        LOG_TRACE("Capture resource: {:X}, index: {}", (size_t) resource->buffer, fIndex);

        if (_commandQueue == nullptr && !CreateObjects())
        {
            LOG_WARN("Can't create command queue!");
            _captureCounter[fIndex]--;
            return false;
        }

        // Make a copy of resource to capture current state
        if (!resource->extended)
        {
            if (CreateBufferResource(s.currentD3D12Device, resource, D3D12_RESOURCE_STATE_COPY_DEST,
                                     &_captureBuffer[fIndex]))
            {
                LOG_DEBUG("Create a copy of resource: {:X}", (size_t) resource->buffer);

                // Using state D3D12_RESOURCE_STATE_VIDEO_ENCODE_WRITE as skip flag
                if (state != D3D12_RESOURCE_STATE_VIDEO_ENCODE_WRITE)
                    ResourceBarrier(cmdList, resource->buffer, resource->state, D3D12_RESOURCE_STATE_COPY_SOURCE);

                cmdList->CopyResource(_captureBuffer[fIndex], resource->buffer);

                // Using state D3D12_RESOURCE_STATE_VIDEO_ENCODE_WRITE as skip flag
                if (state != D3D12_RESOURCE_STATE_VIDEO_ENCODE_WRITE)
                    ResourceBarrier(cmdList, resource->buffer, D3D12_RESOURCE_STATE_COPY_SOURCE, resource->state);

                LOG_DEBUG("Copy created");
            }
            else
            {
                LOG_WARN("Can't create _captureBuffer!");
                _captureCounter[fIndex]--;
                break;
            }
        }
        else
        {
            if (CreateBufferResourceWithSize(s.currentD3D12Device, resource, D3D12_RESOURCE_STATE_COPY_DEST,
                                             &_captureBuffer[fIndex], scDesc.BufferDesc.Width,
                                             scDesc.BufferDesc.Height))
            {
                LOG_DEBUG("Create a copy of resource: {:X}", (size_t) resource->buffer);

                // Using state D3D12_RESOURCE_STATE_VIDEO_ENCODE_WRITE as skip flag
                if (state != D3D12_RESOURCE_STATE_VIDEO_ENCODE_WRITE)
                    ResourceBarrier(cmdList, resource->buffer, state, D3D12_RESOURCE_STATE_COPY_SOURCE);

                D3D12_TEXTURE_COPY_LOCATION srcLocation;
                ZeroMemory(&srcLocation, sizeof(srcLocation));
                srcLocation.pResource = resource->buffer;
                srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                srcLocation.SubresourceIndex = 0; // copy from mip 0, array slice 0

                D3D12_TEXTURE_COPY_LOCATION dstLocation;
                ZeroMemory(&dstLocation, sizeof(dstLocation));
                dstLocation.pResource = _captureBuffer[fIndex];
                dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                dstLocation.SubresourceIndex = 0; // paste into mip 0, array slice 0

                D3D12_BOX srcBox;
                srcBox.left = 0;
                srcBox.top = 0;
                srcBox.front = 0;
                srcBox.back = 1;

                if (scDesc.BufferDesc.Width > resource->width || scDesc.BufferDesc.Height > resource->height)
                {
                    srcBox.right = static_cast<UINT>(resource->width);
                    srcBox.bottom = resource->height;
                    UINT top = (scDesc.BufferDesc.Height - resource->height) / 2;
                    UINT left = static_cast<UINT>((scDesc.BufferDesc.Width - resource->width) / 2);

                    cmdList->CopyTextureRegion(&dstLocation, left, top, 0, &srcLocation, &srcBox);
                }
                else
                {
                    srcBox.right = scDesc.BufferDesc.Width;
                    srcBox.bottom = scDesc.BufferDesc.Height;

                    cmdList->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, &srcBox);
                }

                // Using state D3D12_RESOURCE_STATE_VIDEO_ENCODE_WRITE as skip flag
                if (state != D3D12_RESOURCE_STATE_VIDEO_ENCODE_WRITE)
                    ResourceBarrier(cmdList, resource->buffer, D3D12_RESOURCE_STATE_COPY_SOURCE, state);

                LOG_DEBUG("Copy created");
            }
            else
            {
                LOG_WARN("Can't create _captureBuffer!");
                _captureCounter[fIndex]--;
                break;
            }
        }

        auto fg = reinterpret_cast<IFGFeature_Dx12*>(s.currentFG);
        UINT64 interpolationRectWidth = 0;
        UINT interpolationRectHeight = 0;
        if (fg != nullptr)
            fg->GetInterpolationRect(interpolationRectWidth, interpolationRectHeight);

        uint32_t left = 0;
        uint32_t top = 0;

        // use swapchain buffer info
        LOG_DEBUG("SwapChain Res: {}x{}, Upscaler Display Res: {}x{}", s.currentSwapchainDesc.BufferDesc.Width,
                  s.currentSwapchainDesc.BufferDesc.Height, interpolationRectWidth, interpolationRectHeight);

        auto calculatedLeft = ((int) s.currentSwapchainDesc.BufferDesc.Width - (int) interpolationRectWidth) / 2;
        if (calculatedLeft > 0)
            left = Config::Instance()->FGRectLeft.value_or(calculatedLeft);

        auto calculatedTop = ((int) s.currentSwapchainDesc.BufferDesc.Height - (int) interpolationRectHeight) / 2;
        if (calculatedTop > 0)
            top = Config::Instance()->FGRectTop.value_or(calculatedTop);

        // needs conversion?
        if (resource->format != scDesc.BufferDesc.Format)
        {
            if (_formatTransfer[fIndex] == nullptr ||
                !_formatTransfer[fIndex]->IsFormatCompatible(scDesc.BufferDesc.Format))
            {
                LOG_DEBUG("Format change, recreate the FormatTransfer");

                if (_formatTransfer[fIndex] != nullptr)
                    delete _formatTransfer[fIndex];

                _formatTransfer[fIndex] = nullptr;
                s.skipHeapCapture = true;
                _formatTransfer[fIndex] = new FT_Dx12("FormatTransfer", s.currentD3D12Device, scDesc.BufferDesc.Format);
                s.skipHeapCapture = false;
            }

            if (_formatTransfer[fIndex] != nullptr &&
                _formatTransfer[fIndex]->CreateBufferResource(s.currentD3D12Device, resource->buffer,
                                                              D3D12_RESOURCE_STATE_UNORDERED_ACCESS))
            {
                // This will prevent resource tracker to check these operations
                // Will reset after FG dispatch
                _skipHudlessChecks = true;

                ResourceBarrier(cmdList, _captureBuffer[fIndex], D3D12_RESOURCE_STATE_COPY_DEST,
                                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                _formatTransfer[fIndex]->Dispatch(s.currentD3D12Device, cmdList, _captureBuffer[fIndex],
                                                  _formatTransfer[fIndex]->Buffer());
                ResourceBarrier(cmdList, _captureBuffer[fIndex], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                                D3D12_RESOURCE_STATE_COPY_DEST);

                LOG_TRACE("Using _formatTransfer->Buffer()");

                if (fg != nullptr)
                {
                    Dx12Resource setResource {};
                    setResource.type = FG_ResourceType::HudlessColor;
                    setResource.cmdList = cmdList;
                    setResource.resource = _formatTransfer[fIndex]->Buffer();
                    setResource.left = left;
                    setResource.top = top;
                    setResource.width = scDesc.BufferDesc.Width;
                    setResource.height = scDesc.BufferDesc.Height;
                    setResource.state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
                    setResource.validity = FG_ResourceValidity::JustTrackCmdlist;

                    fg->SetResource(&setResource);
                }
            }
            else
            {
                LOG_WARN("_formatTransfer is null or can't create _formatTransfer buffer!");
                _captureCounter[fIndex]--;
                break;
            }
        }
        else
        {
            // This will prevent resource tracker to check these operations
            // Will reset after FG dispatch
            _skipHudlessChecks = true;
            LOG_DEBUG("Using _captureBuffer");

            if (fg != nullptr)
            {
                Dx12Resource setResource {};
                setResource.type = FG_ResourceType::HudlessColor;
                setResource.cmdList = cmdList;
                setResource.resource = _captureBuffer[fIndex];
                setResource.left = left;
                setResource.top = top;
                setResource.width = scDesc.BufferDesc.Width;
                setResource.height = scDesc.BufferDesc.Height;
                setResource.state = D3D12_RESOURCE_STATE_COPY_DEST;
                setResource.validity = FG_ResourceValidity::JustTrackCmdlist;

                fg->SetResource(&setResource);
            }
        }

        if (s.FGcaptureResources)
        {
            std::lock_guard<std::mutex> lock(_captureMutex);
            _captureList.insert(resource->buffer);
            s.FGcapturedResourceCount = _captureList.size();
        }

        LOG_DEBUG("Calling FG with hudless");

        // This will prevent resource tracker to check these operations
        // Will reset after FG dispatch
        _skipHudlessChecks = true;
        HudlessFound(cmdList);

        if (capturedHudlessInfo != nullptr)
            capturedHudlessInfo->usageCount++;
        else
        {
            s.CapturedHudlesses[resource->buffer] = {};
            capturedHudlessInfo = &s.CapturedHudlesses[resource->buffer];
        }

        capturedHudlessInfo->captureInfo = resource->captureInfo;

        return true;

    } while (false);

    return false;
}

void Hudfix_Dx12::ResetCounters()
{
    _fgCounter = 0;
    _upscaleCounter = 0;

    _lastDiffTime = 0.0;
    _upscaleEndTime = 0.0;
    _targetTime = 0.0;
    _frameTime = 0.0;

    _hudlessList.clear();

    _captureCounter[0] = 0;
    _captureCounter[1] = 0;
    _captureCounter[2] = 0;
    _captureCounter[3] = 0;

    LOG_DEBUG("_hudlessList: {}", _hudlessList.size());
}
