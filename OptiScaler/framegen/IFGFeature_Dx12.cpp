#include "IFGFeature_Dx12.h"

#include <State.h>
#include <Config.h>

#include <magic_enum.hpp>

bool IFGFeature_Dx12::GetResourceCopy(FG_ResourceType type, D3D12_RESOURCE_STATES bufferState, ID3D12Resource* output)
{
    if (!InitCopyCmdList())
        return false;

    auto resource = GetResource(type);

    if (resource == nullptr || (resource->copy == nullptr && resource->validity == FG_ResourceValidity::ValidNow))
    {
        LOG_WARN("No resource copy of type {} to use", magic_enum::enum_name(type));
        return false;
    }

    auto fIndex = GetIndex();

    if (!_uiCommandListResetted[fIndex])
    {
        auto result = _copyCommandAllocator[fIndex]->Reset();
        if (result != S_OK)
            return false;

        result = _copyCommandList[fIndex]->Reset(_copyCommandAllocator[fIndex], nullptr);
        if (result != S_OK)
            return false;
    }

    _copyCommandList[fIndex]->CopyResource(output, resource->GetResource());

    return true;
}

ID3D12CommandQueue* IFGFeature_Dx12::GetCommandQueue() { return _gameCommandQueue; }

ID3D12GraphicsCommandList* IFGFeature_Dx12::GetUICommandList(int index)
{
    if (index < 0)
        index = GetIndex();

    LOG_DEBUG("index: {}", index);

    if (_uiCommandAllocator[0] == nullptr)
    {
        if (_device != nullptr)
            CreateObjects(_device);
        else if (State::Instance().currentD3D12Device != nullptr)
            CreateObjects(State::Instance().currentD3D12Device);
        else
            return nullptr;
    }

    for (size_t i = 0; i < BUFFER_COUNT; i++)
    {
        if (i != index && _uiCommandListResetted[i])
        {
            LOG_DEBUG("Executing _uiCommandList[{}]: {:X}", i, (size_t) _uiCommandList[i]);
            auto closeResult = _uiCommandList[i]->Close();

            if (closeResult != S_OK)
                LOG_ERROR("_uiCommandList[{}]->Close() error: {:X}", i, (UINT) closeResult);

            _uiCommandListResetted[i] = false;
        }
    }

    if (!_uiCommandListResetted[index])
    {
        auto result = _uiCommandAllocator[index]->Reset();

        if (result == S_OK)
        {
            result = _uiCommandList[index]->Reset(_uiCommandAllocator[index], nullptr);

            if (result == S_OK)
                _uiCommandListResetted[index] = true;
            else
                LOG_ERROR("_uiCommandList[{}]->Reset() error: {:X}", index, (UINT) result);
        }
        else
        {
            LOG_ERROR("_uiCommandAllocator[{}]->Reset() error: {:X}", index, (UINT) result);
        }
    }

    return _uiCommandList[index];
}

Dx12Resource* IFGFeature_Dx12::GetResource(FG_ResourceType type, int index)
{
    std::lock_guard<std::mutex> lock(_frMutex);

    if (_resourceFrame[type] != _frameCount)
        return nullptr;

    if (index < 0)
        index = GetIndex();

    auto& currentIndex = _frameResources[index];
    if (auto it = currentIndex.find(type); it != currentIndex.end())
        return &it->second;

    return nullptr;
}

void IFGFeature_Dx12::NewFrame()
{
    if (_waitingNewFrameData)
    {
        LOG_DEBUG("Re-activating FG");
        UpdateTarget();
        Activate();
        _waitingNewFrameData = false;
    }

    auto fIndex = GetIndex();

    std::lock_guard<std::mutex> lock(_frMutex);

    LOG_DEBUG("_frameCount: {}, fIndex: {}", _frameCount, fIndex);

    _frameResources[fIndex].clear();
    _uiCommandListResetted[fIndex] = false;
    _lastFGFramePresentId = _fgFramePresentId;
}

void IFGFeature_Dx12::FlipResource(Dx12Resource* resource)
{
    auto type = resource->type;

    if (type != FG_ResourceType::Depth && type != FG_ResourceType::Velocity)
        return;

    auto fIndex = GetIndex();
    ID3D12Resource* flipOutput = nullptr;
    std::unique_ptr<RF_Dx12>* flip = nullptr;

    flipOutput = _resourceCopy[fIndex][type];

    if (!CreateBufferResource(_device, resource->resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, &flipOutput, true,
                              resource->type == FG_ResourceType::Depth))
    {
        LOG_ERROR("{}, CreateBufferResource for flip is failed!", magic_enum::enum_name(type));
        return;
    }

    _resourceCopy[fIndex][type] = flipOutput;

    if (type == FG_ResourceType::Depth)
    {
        if (_depthFlip.get() == nullptr)
        {
            _depthFlip = std::make_unique<RF_Dx12>("DepthFlip", _device);
            return;
        }

        flip = &_depthFlip;
    }
    else
    {
        if (_mvFlip.get() == nullptr)
        {
            _mvFlip = std::make_unique<RF_Dx12>("VelocityFlip", _device);
            return;
        }

        flip = &_mvFlip;
    }

    if (flip->get()->IsInit())
    {
        auto cmdList = (resource->cmdList != nullptr) ? resource->cmdList : GetUICommandList(fIndex);
        auto result = flip->get()->Dispatch(_device, (ID3D12GraphicsCommandList*) cmdList, resource->resource,
                                            flipOutput, resource->width, resource->height, true);

        if (result)
        {
            LOG_TRACE("Setting {} from flip, index: {}", magic_enum::enum_name(type), fIndex);
            resource->copy = flipOutput;
            resource->state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        }
    }
}

bool IFGFeature_Dx12::CreateBufferResourceWithSize(ID3D12Device* device, ID3D12Resource* source,
                                                   D3D12_RESOURCE_STATES state, ID3D12Resource** target, UINT width,
                                                   UINT height, bool UAV, bool depth)
{
    if (device == nullptr || source == nullptr)
        return false;

    auto inDesc = source->GetDesc();

    if (UAV)
        inDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    if (depth)
        inDesc.Format = DXGI_FORMAT_R32_FLOAT;

    if (*target != nullptr)
    {
        auto bufDesc = (*target)->GetDesc();

        if (bufDesc.Width != width || bufDesc.Height != height || bufDesc.Format != inDesc.Format ||
            bufDesc.Flags != inDesc.Flags)
        {
            (*target)->Release();
            (*target) = nullptr;
        }
        else
        {
            return true;
        }
    }

    D3D12_HEAP_PROPERTIES heapProperties;
    D3D12_HEAP_FLAGS heapFlags;
    HRESULT hr = source->GetHeapProperties(&heapProperties, &heapFlags);

    if (hr != S_OK)
    {
        LOG_ERROR("GetHeapProperties result: {:X}", (UINT64) hr);
        return false;
    }

    inDesc.Width = width;
    inDesc.Height = height;

    hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &inDesc, state, nullptr,
                                         IID_PPV_ARGS(target));

    if (hr != S_OK)
    {
        LOG_ERROR("CreateCommittedResource result: {:X}", (UINT64) hr);
        return false;
    }

    LOG_DEBUG("Created new one: {}x{}", inDesc.Width, inDesc.Height);

    return true;
}

bool IFGFeature_Dx12::InitCopyCmdList()
{
    if (_copyCommandList[0] != nullptr && _copyCommandAllocator[0] != nullptr)
        return true;

    if (_device == nullptr)
        return false;

    if (_copyCommandList[0] == nullptr || _copyCommandAllocator[0] == nullptr)
        DestroyCopyCmdList();

    ID3D12CommandAllocator* allocator = nullptr;
    ID3D12GraphicsCommandList* cmdList = nullptr;

    for (size_t i = 0; i < BUFFER_COUNT; i++)
    {
        auto result =
            _device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_copyCommandAllocator[i]));
        if (result != S_OK)
        {
            LOG_ERROR("_copyCommandAllocator: {:X}", (unsigned long) result);
            return false;
        }

        _copyCommandAllocator[i]->SetName(L"_copyCommandAllocator");
        if (CheckForRealObject(__FUNCTION__, _copyCommandAllocator[i], (IUnknown**) &allocator))
            _copyCommandAllocator[i] = allocator;

        result = _device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _copyCommandAllocator[i], NULL,
                                            IID_PPV_ARGS(&_copyCommandList[i]));
        if (result != S_OK)
        {
            LOG_ERROR("_copyCommandAllocator: {:X}", (unsigned long) result);
            return false;
        }
        _copyCommandList[i]->SetName(L"_copyCommandList");
        if (CheckForRealObject(__FUNCTION__, _copyCommandList[i], (IUnknown**) &cmdList))
            _copyCommandList[i] = cmdList;

        result = _copyCommandList[i]->Close();
        if (result != S_OK)
        {
            LOG_ERROR("_copyCommandList->Close: {:X}", (unsigned long) result);
            return false;
        }
    }

    return true;
}

void IFGFeature_Dx12::DestroyCopyCmdList()
{
    for (size_t i = 0; i < BUFFER_COUNT; i++)
    {
        if (_copyCommandAllocator[i] != nullptr)
        {
            _copyCommandAllocator[i]->Release();
            _copyCommandAllocator[i] = nullptr;
        }

        if (_copyCommandList[i] != nullptr)
        {
            _copyCommandList[i]->Release();
            _copyCommandList[i] = nullptr;
        }
    }
}

bool IFGFeature_Dx12::CreateBufferResource(ID3D12Device* device, ID3D12Resource* source,
                                           D3D12_RESOURCE_STATES initialState, ID3D12Resource** target, bool UAV,
                                           bool depth)
{
    if (device == nullptr || source == nullptr)
        return false;

    auto inDesc = source->GetDesc();

    if (UAV)
        inDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    if (depth)
        inDesc.Format = DXGI_FORMAT_R32_FLOAT;

    if (*target != nullptr)
    {
        //(*target)->Release();
        //(*target) = nullptr;

        auto bufDesc = (*target)->GetDesc();

        if (bufDesc.Width != inDesc.Width || bufDesc.Height != inDesc.Height || bufDesc.Format != inDesc.Format ||
            bufDesc.Flags != inDesc.Flags)
        {
            (*target)->Release();
            (*target) = nullptr;
        }
        else
        {
            return true;
        }
    }

    D3D12_HEAP_PROPERTIES heapProperties;
    D3D12_HEAP_FLAGS heapFlags;
    auto hr = source->GetHeapProperties(&heapProperties, &heapFlags);

    hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &inDesc, initialState, nullptr,
                                         IID_PPV_ARGS(target));

    if (hr != S_OK)
    {
        LOG_ERROR("CreateCommittedResource result: {:X}", (UINT64) hr);
        return false;
    }

    LOG_DEBUG("Created new one: {}x{}", inDesc.Width, inDesc.Height);

    return true;
}

void IFGFeature_Dx12::ResourceBarrier(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* resource,
                                      D3D12_RESOURCE_STATES beforeState, D3D12_RESOURCE_STATES afterState)
{
    if (beforeState == afterState)
        return;

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = beforeState;
    barrier.Transition.StateAfter = afterState;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmdList->ResourceBarrier(1, &barrier);
}

bool IFGFeature_Dx12::CopyResource(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* source, ID3D12Resource** target,
                                   D3D12_RESOURCE_STATES sourceState)
{
    auto result = true;

    ResourceBarrier(cmdList, source, sourceState, D3D12_RESOURCE_STATE_COPY_SOURCE);

    if (CreateBufferResource(_device, source, D3D12_RESOURCE_STATE_COPY_DEST, target))
        cmdList->CopyResource(*target, source);
    else
        result = false;

    ResourceBarrier(cmdList, source, D3D12_RESOURCE_STATE_COPY_SOURCE, sourceState);

    return result;
}
