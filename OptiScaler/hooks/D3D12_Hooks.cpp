#include "D3D12_Hooks.h"

#include <Util.h>
#include <Config.h>

#include <resource_tracking/ResTrack_Dx12.h>

#include <proxies/D3D12_Proxy.h>
#include <proxies/IGDExt_Proxy.h>

#include <detours/detours.h>

#include <d3d12.h>
#include <dxgi1_6.h>

#pragma intrinsic(_ReturnAddress)

typedef void (*PFN_CreateSampler)(ID3D12Device* device, const D3D12_SAMPLER_DESC* pDesc,
                                  D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor);

typedef HRESULT (*PFN_CheckFeatureSupport)(ID3D12Device* device, D3D12_FEATURE Feature, void* pFeatureSupportData,
                                           UINT FeatureSupportDataSize);

typedef HRESULT (*PFN_CreateCommittedResource)(ID3D12Device* device, const D3D12_HEAP_PROPERTIES* pHeapProperties,
                                               D3D12_HEAP_FLAGS HeapFlags, D3D12_RESOURCE_DESC* pDesc,
                                               D3D12_RESOURCE_STATES InitialResourceState,
                                               const D3D12_CLEAR_VALUE* pOptimizedClearValue, REFIID riidResource,
                                               void** ppvResource);

typedef HRESULT (*PFN_CreatePlacedResource)(ID3D12Device* device, ID3D12Heap* pHeap, UINT64 HeapOffset,
                                            D3D12_RESOURCE_DESC* pDesc, D3D12_RESOURCE_STATES InitialState,
                                            const D3D12_CLEAR_VALUE* pOptimizedClearValue, REFIID riid,
                                            void** ppvResource);

typedef D3D12_RESOURCE_ALLOCATION_INFO (*PFN_GetResourceAllocationInfo)(ID3D12Device* device, UINT visibleMask,
                                                                        UINT numResourceDescs,
                                                                        D3D12_RESOURCE_DESC* pResourceDescs);

typedef ULONG (*PFN_Release)(IUnknown* This);

static PFN_CreateSampler o_CreateSampler = nullptr;
static PFN_CheckFeatureSupport o_CheckFeatureSupport = nullptr;
static PFN_CreateCommittedResource o_CreateCommittedResource = nullptr;
static PFN_CreatePlacedResource o_CreatePlacedResource = nullptr;
static PFN_GetResourceAllocationInfo o_GetResourceAllocationInfo = nullptr;

static D3d12Proxy::PFN_D3D12CreateDevice o_D3D12CreateDevice = nullptr;
static D3d12Proxy::PFN_D3D12SerializeRootSignature o_D3D12SerializeRootSignature = nullptr;
static D3d12Proxy::PFN_D3D12SerializeVersionedRootSignature o_D3D12SerializeVersionedRootSignature = nullptr;
static PFN_Release o_D3D12DeviceRelease = nullptr;

static bool _d3d12Captured = false;
static LUID _lastAdapterLuid = {};

// Intel Atomic Extension
struct UE_D3D12_RESOURCE_DESC
{
    D3D12_RESOURCE_DIMENSION Dimension;
    UINT64 Alignment;
    UINT64 Width;
    UINT Height;
    UINT16 DepthOrArraySize;
    UINT16 MipLevels;
    DXGI_FORMAT Format;
    DXGI_SAMPLE_DESC SampleDesc;
    D3D12_TEXTURE_LAYOUT Layout;
    D3D12_RESOURCE_FLAGS Flags;

    // UE Part
    uint8_t PixelFormat { 0 };
    uint8_t UAVPixelFormat { 0 };
    bool bRequires64BitAtomicSupport : 1 = false;
    bool bReservedResource : 1 = false;
    bool bBackBuffer : 1 = false;
    bool bExternal : 1 = false;
};

static ID3D12Device* _intelD3D12Device = nullptr;
static ULONG _intelD3D12DeviceRefTarget = 0;
static bool _skipCommitedResource = false;
static bool _skipGetResourceAllocationInfo = false;

#ifdef ENABLE_DEBUG_LAYER_DX12
static ID3D12Debug3* debugController = nullptr;
static ID3D12InfoQueue* infoQueue = nullptr;
static ID3D12InfoQueue1* infoQueue1 = nullptr;

static void CALLBACK D3D12DebugCallback(D3D12_MESSAGE_CATEGORY Category, D3D12_MESSAGE_SEVERITY Severity,
                                        D3D12_MESSAGE_ID ID, LPCSTR pDescription, void* pContext)
{
    LOG_DEBUG("Category: {}, Severity: {}, ID: {}, Message: {}", (UINT) Category, (UINT) Severity, (UINT) ID,
              pDescription);
}
#endif

static void HookToDevice(ID3D12Device* InDevice);
static void UnhookDevice();

static inline D3D12_FILTER UpgradeToAF(D3D12_FILTER f)
{
    // Skip point filter
    const auto minF = D3D12_DECODE_MIN_FILTER(f);
    const auto magF = D3D12_DECODE_MAG_FILTER(f);
    const auto mipF = D3D12_DECODE_MIP_FILTER(f);
    if (Config::Instance()->AnisotropySkipPointFilter.value_or_default() &&
        ((mipF == D3D12_FILTER_TYPE_POINT) || (minF == D3D12_FILTER_TYPE_POINT && magF == D3D12_FILTER_TYPE_POINT)))
    {
        return f;
    }

    const auto reduction = D3D12_DECODE_FILTER_REDUCTION(f);

    if (reduction == D3D12_FILTER_REDUCTION_TYPE_COMPARISON)
    {
        if (Config::Instance()->AnisotropyModifyComp.value_or_default())
            return D3D12_ENCODE_ANISOTROPIC_FILTER(D3D12_FILTER_REDUCTION_TYPE_COMPARISON);

        return f;
    }

    if (reduction == D3D12_FILTER_REDUCTION_TYPE_MINIMUM)
    {
        if (Config::Instance()->AnisotropyModifyMinMax.value_or_default())
            return D3D12_ENCODE_ANISOTROPIC_FILTER(D3D12_FILTER_REDUCTION_TYPE_MINIMUM);

        return f;
    }

    if (reduction == D3D12_FILTER_REDUCTION_TYPE_MAXIMUM)
    {
        if (Config::Instance()->AnisotropyModifyMinMax.value_or_default())
            return D3D12_ENCODE_ANISOTROPIC_FILTER(D3D12_FILTER_REDUCTION_TYPE_MAXIMUM);

        return f;
    }

    return D3D12_ENCODE_ANISOTROPIC_FILTER(D3D12_FILTER_REDUCTION_TYPE_STANDARD);
}

static HRESULT hkD3D12CreateDevice(IDXGIAdapter* pAdapter, D3D_FEATURE_LEVEL MinimumFeatureLevel, REFIID riid,
                                   void** ppDevice)
{
    LOG_DEBUG("Adapter: {:X}, Level: {:X}, Caller: {}", (size_t) pAdapter, (UINT) MinimumFeatureLevel,
              Util::WhoIsTheCaller(_ReturnAddress()));

#ifdef ENABLE_DEBUG_LAYER_DX12
    LOG_WARN("Debug layers active!");
    if (debugController == nullptr && D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)) == S_OK)
    {
        debugController->EnableDebugLayer();

#ifdef ENABLE_GPU_VALIDATION
        LOG_WARN("GPU Based Validation active!");
        debugController->SetEnableGPUBasedValidation(TRUE);
#endif

        debugController->Release();
    }
#endif

    DXGI_ADAPTER_DESC desc {};
    std::wstring szName;
    if (pAdapter != nullptr && MinimumFeatureLevel != D3D_FEATURE_LEVEL_1_0_CORE)
    {
        ScopedSkipSpoofing skipSpoofing {};

        if (pAdapter->GetDesc(&desc) == S_OK)
        {
            szName = desc.Description;
            LOG_INFO("Adapter Desc: {}", wstring_to_string(szName));
        }
    }

    auto minLevel = MinimumFeatureLevel;
    if (Config::Instance()->SpoofFeatureLevel.value_or_default() && MinimumFeatureLevel != D3D_FEATURE_LEVEL_1_0_CORE)
    {
        LOG_INFO("Forcing feature level 0xb000 for new device");
        minLevel = D3D_FEATURE_LEVEL_11_0;
    }

    if (ppDevice == nullptr)
    {
        LOG_ERROR("ppDevice is nullptr");
        return o_D3D12CreateDevice(pAdapter, minLevel, riid, ppDevice);
    }

    HRESULT result;
    if (desc.VendorId == VendorId::Intel)
    {
        ScopedSkipSpoofing skipSpoofing {};
        result = o_D3D12CreateDevice(pAdapter, minLevel, riid, ppDevice);
    }
    else
    {
        result = o_D3D12CreateDevice(pAdapter, minLevel, riid, ppDevice);
    }

    LOG_DEBUG("o_D3D12CreateDevice result: {:X}", (UINT) result);

    if (result == S_OK && ppDevice != nullptr && MinimumFeatureLevel != D3D_FEATURE_LEVEL_1_0_CORE)
    {
        LOG_DEBUG("Device captured: {0:X}", (size_t) *ppDevice);
        State::Instance().currentD3D12Device = (ID3D12Device*) *ppDevice;

        if (szName.size() > 0)
            State::Instance().DeviceAdapterNames[*ppDevice] = wstring_to_string(szName);

        if (desc.VendorId == VendorId::Intel && Config::Instance()->UESpoofIntelAtomics64.value_or_default())
        {
            IGDExtProxy::EnableAtomicSupport(State::Instance().currentD3D12Device);
            _intelD3D12Device = State::Instance().currentD3D12Device;
            _intelD3D12DeviceRefTarget = _intelD3D12Device->AddRef();

            if (o_D3D12DeviceRelease == nullptr)
                _intelD3D12Device->Release();
            else
                o_D3D12DeviceRelease(_intelD3D12Device);
        }

        // For WuWa, might cause issues with other games
        // if (_lastAdapterLuid.HighPart != desc.AdapterLuid.HighPart ||
        //    _lastAdapterLuid.LowPart != desc.AdapterLuid.LowPart)
        // {
        //    ResTrack_Dx12::ReleaseHooks();
        // }

        // _lastAdapterLuid = desc.AdapterLuid;

        HookToDevice(State::Instance().currentD3D12Device);
        _d3d12Captured = true;

        State::Instance().d3d12Devices.push_back((ID3D12Device*) *ppDevice);

#ifdef ENABLE_DEBUG_LAYER_DX12
        if (infoQueue != nullptr)
            infoQueue->Release();

        if (infoQueue1 != nullptr)
            infoQueue1->Release();

        if (State::Instance().currentD3D12Device->QueryInterface(IID_PPV_ARGS(&infoQueue)) == S_OK)
        {
            LOG_DEBUG("infoQueue accuired");

            infoQueue->ClearRetrievalFilter();
            infoQueue->SetMuteDebugOutput(false);

            HRESULT res;
            res = infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
            // res = infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
            // res = infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

            if (infoQueue->QueryInterface(IID_PPV_ARGS(&infoQueue1)) == S_OK && infoQueue1 != nullptr)
            {
                LOG_DEBUG("infoQueue1 accuired, registering MessageCallback");
                res = infoQueue1->RegisterMessageCallback(D3D12DebugCallback, D3D12_MESSAGE_CALLBACK_IGNORE_FILTERS,
                                                          NULL, NULL);
            }
        }
#endif
    }

    LOG_DEBUG("final result: {:X}", (UINT) result);
    return result;
}

static HRESULT hkD3D12SerializeRootSignature(D3d12Proxy::D3D12_ROOT_SIGNATURE_DESC_L* pRootSignature,
                                             D3D_ROOT_SIGNATURE_VERSION Version, ID3DBlob** ppBlob,
                                             ID3DBlob** ppErrorBlob)
{
    if (pRootSignature != nullptr)
    {
        for (size_t i = 0; i < pRootSignature->NumStaticSamplers; i++)
        {
            auto samplerDesc = &pRootSignature->pStaticSamplers[i];

            if (Config::Instance()->MipmapBiasOverride.has_value())
            {
                if ((samplerDesc->MipLODBias < 0.0f && samplerDesc->MinLOD != samplerDesc->MaxLOD) ||
                    Config::Instance()->MipmapBiasOverrideAll.value_or_default())
                {
                    if (Config::Instance()->MipmapBiasOverride.has_value())
                    {
                        LOG_DEBUG("Overriding mipmap bias {0} -> {1}", samplerDesc->MipLODBias,
                                  Config::Instance()->MipmapBiasOverride.value());

                        if (Config::Instance()->MipmapBiasFixedOverride.value_or_default())
                            samplerDesc->MipLODBias = Config::Instance()->MipmapBiasOverride.value();
                        else if (Config::Instance()->MipmapBiasScaleOverride.value_or_default())
                            samplerDesc->MipLODBias =
                                samplerDesc->MipLODBias * Config::Instance()->MipmapBiasOverride.value();
                        else
                            samplerDesc->MipLODBias =
                                samplerDesc->MipLODBias + Config::Instance()->MipmapBiasOverride.value();
                    }

                    if (State::Instance().lastMipBiasMax < samplerDesc->MipLODBias)
                        State::Instance().lastMipBiasMax = samplerDesc->MipLODBias;

                    if (State::Instance().lastMipBias > samplerDesc->MipLODBias)
                        State::Instance().lastMipBias = samplerDesc->MipLODBias;
                }
            }

            if (Config::Instance()->AnisotropyOverride.has_value())
            {
                LOG_DEBUG("Overriding {2:X} to anisotropic filtering {0} -> {1}", samplerDesc->MaxAnisotropy,
                          Config::Instance()->AnisotropyOverride.value(), (UINT) samplerDesc->Filter);

                samplerDesc->Filter = UpgradeToAF(samplerDesc->Filter);
                samplerDesc->MaxAnisotropy = Config::Instance()->AnisotropyOverride.value();
            }
        }
    }

    return o_D3D12SerializeRootSignature(pRootSignature, Version, ppBlob, ppErrorBlob);
}

static HRESULT hkD3D12SerializeVersionedRootSignature(D3d12Proxy::D3D12_VERSIONED_ROOT_SIGNATURE_DESC_L* pRootSignature,
                                                      ID3DBlob** ppBlob, ID3DBlob** ppErrorBlob)
{
    if (pRootSignature != nullptr)
    {
        for (size_t i = 0; i < pRootSignature->Desc_1_0.NumStaticSamplers; i++)
        {
            auto samplerDesc = &pRootSignature->Desc_1_0.pStaticSamplers[i];

            if (Config::Instance()->MipmapBiasOverride.has_value())
            {
                if ((samplerDesc->MipLODBias < 0.0f && samplerDesc->MinLOD != samplerDesc->MaxLOD) ||
                    Config::Instance()->MipmapBiasOverrideAll.value_or_default())
                {
                    if (Config::Instance()->MipmapBiasOverride.has_value())
                    {
                        LOG_DEBUG("Overriding mipmap bias {0} -> {1}", samplerDesc->MipLODBias,
                                  Config::Instance()->MipmapBiasOverride.value());

                        if (Config::Instance()->MipmapBiasFixedOverride.value_or_default())
                            samplerDesc->MipLODBias = Config::Instance()->MipmapBiasOverride.value();
                        else if (Config::Instance()->MipmapBiasScaleOverride.value_or_default())
                            samplerDesc->MipLODBias =
                                samplerDesc->MipLODBias * Config::Instance()->MipmapBiasOverride.value();
                        else
                            samplerDesc->MipLODBias =
                                samplerDesc->MipLODBias + Config::Instance()->MipmapBiasOverride.value();
                    }

                    if (State::Instance().lastMipBiasMax < samplerDesc->MipLODBias)
                        State::Instance().lastMipBiasMax = samplerDesc->MipLODBias;

                    if (State::Instance().lastMipBias > samplerDesc->MipLODBias)
                        State::Instance().lastMipBias = samplerDesc->MipLODBias;
                }
            }

            if (Config::Instance()->AnisotropyOverride.has_value())
            {
                LOG_DEBUG("Overriding {2:X} to anisotropic filtering {0} -> {1}", samplerDesc->MaxAnisotropy,
                          Config::Instance()->AnisotropyOverride.value(), (UINT) samplerDesc->Filter);

                samplerDesc->Filter = UpgradeToAF(samplerDesc->Filter);
                samplerDesc->MaxAnisotropy = Config::Instance()->AnisotropyOverride.value();
            }
        }
    }

    return o_D3D12SerializeVersionedRootSignature(pRootSignature, ppBlob, ppErrorBlob);
}

static ULONG hkD3D12DeviceRelease(IUnknown* device)
{
    if (Config::Instance()->UESpoofIntelAtomics64.value_or_default() && device == _intelD3D12Device)
    {
        auto refCount = device->AddRef();

        if (refCount == _intelD3D12DeviceRefTarget)
        {
            LOG_INFO("Destroying IGDExt context!");
            _intelD3D12Device = nullptr;
            IGDExtProxy::DestroyContext();
        }

        o_D3D12DeviceRelease(device);
    }

    auto result = o_D3D12DeviceRelease(device);

    return result;
}

static HRESULT hkCheckFeatureSupport(ID3D12Device* device, D3D12_FEATURE Feature, void* pFeatureSupportData,
                                     UINT FeatureSupportDataSize)
{
    auto result = o_CheckFeatureSupport(device, Feature, pFeatureSupportData, FeatureSupportDataSize);

    if (Config::Instance()->UESpoofIntelAtomics64.value_or_default() && Feature == D3D12_FEATURE_D3D12_OPTIONS9)
    {
        auto featureSupport = (D3D12_FEATURE_DATA_D3D12_OPTIONS9*) pFeatureSupportData;
        LOG_INFO("Spoofing AtomicInt64OnTypedResourceSupported");
        featureSupport->AtomicInt64OnTypedResourceSupported = true;
    }

    return result;
}

static HRESULT hkCreateCommittedResource(ID3D12Device* device, const D3D12_HEAP_PROPERTIES* pHeapProperties,
                                         D3D12_HEAP_FLAGS HeapFlags, D3D12_RESOURCE_DESC* pDesc,
                                         D3D12_RESOURCE_STATES InitialResourceState,
                                         const D3D12_CLEAR_VALUE* pOptimizedClearValue, REFIID riidResource,
                                         void** ppvResource)
{
    if (!_skipCommitedResource)
    {
        auto ueDesc = reinterpret_cast<UE_D3D12_RESOURCE_DESC*>(pDesc);

        if (Config::Instance()->UESpoofIntelAtomics64.value_or_default() && ueDesc != nullptr &&
            ueDesc->bRequires64BitAtomicSupport)
        {
            _skipCommitedResource = true;
            auto result = IGDExtProxy::CreateCommitedResource(pHeapProperties, HeapFlags, pDesc, InitialResourceState,
                                                              pOptimizedClearValue, riidResource, ppvResource);
            _skipCommitedResource = false;

            return result;
        }
    }

    return o_CreateCommittedResource(device, pHeapProperties, HeapFlags, pDesc, InitialResourceState,
                                     pOptimizedClearValue, riidResource, ppvResource);
}

static bool skipPlacedResource = false;

static HRESULT hkCreatePlacedResource(ID3D12Device* device, ID3D12Heap* pHeap, UINT64 HeapOffset,
                                      D3D12_RESOURCE_DESC* pDesc, D3D12_RESOURCE_STATES InitialState,
                                      const D3D12_CLEAR_VALUE* pOptimizedClearValue, REFIID riid, void** ppvResource)
{
    if (!skipPlacedResource)
    {
        auto ueDesc = reinterpret_cast<UE_D3D12_RESOURCE_DESC*>(pDesc);

        if (Config::Instance()->UESpoofIntelAtomics64.value_or_default() && ueDesc != nullptr &&
            ueDesc->bRequires64BitAtomicSupport)
        {
            skipPlacedResource = true;
            auto result = IGDExtProxy::CreatePlacedResource(pHeap, HeapOffset, pDesc, InitialState,
                                                            pOptimizedClearValue, riid, ppvResource);
            skipPlacedResource = false;

            return result;
        }
    }

    return o_CreatePlacedResource(device, pHeap, HeapOffset, pDesc, InitialState, pOptimizedClearValue, riid,
                                  ppvResource);
}

static D3D12_RESOURCE_ALLOCATION_INFO hkGetResourceAllocationInfo(ID3D12Device* device, UINT visibleMask,
                                                                  UINT numResourceDescs,
                                                                  D3D12_RESOURCE_DESC* pResourceDescs)
{
    if (State::Instance().currentD3D12Device != device)
        device = State::Instance().currentD3D12Device;

    if (!_skipGetResourceAllocationInfo)
    {
        auto ueDesc = reinterpret_cast<UE_D3D12_RESOURCE_DESC*>(pResourceDescs);

        if (Config::Instance()->UESpoofIntelAtomics64.value_or_default() && ueDesc != nullptr &&
            ueDesc->bRequires64BitAtomicSupport)
        {
            _skipGetResourceAllocationInfo = true;
            auto result = IGDExtProxy::GetResourceAllocationInfo(visibleMask, numResourceDescs, pResourceDescs);
            _skipGetResourceAllocationInfo = false;

            return result;
        }
    }

    return o_GetResourceAllocationInfo(device, visibleMask, numResourceDescs, pResourceDescs);
}

static void hkCreateSampler(ID3D12Device* device, const D3D12_SAMPLER_DESC* pDesc,
                            D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
    if (pDesc == nullptr || device == nullptr)
        return;

    D3D12_SAMPLER_DESC newDesc = *pDesc;

    if (Config::Instance()->AnisotropyOverride.has_value())
    {
        LOG_DEBUG("Overriding {2:X} to anisotropic filtering {0} -> {1}", pDesc->MaxAnisotropy,
                  Config::Instance()->AnisotropyOverride.value(), (UINT) newDesc.Filter);

        newDesc.Filter = UpgradeToAF(pDesc->Filter);
        newDesc.MaxAnisotropy = Config::Instance()->AnisotropyOverride.value();
    }
    else
    {
        newDesc.Filter = pDesc->Filter;
        newDesc.MaxAnisotropy = pDesc->MaxAnisotropy;
    }

    if ((newDesc.MipLODBias < 0.0f && newDesc.MinLOD != newDesc.MaxLOD) ||
        Config::Instance()->MipmapBiasOverrideAll.value_or_default())
    {
        if (Config::Instance()->MipmapBiasOverride.has_value())
        {
            LOG_DEBUG("Overriding mipmap bias {0} -> {1}", pDesc->MipLODBias,
                      Config::Instance()->MipmapBiasOverride.value());

            if (Config::Instance()->MipmapBiasFixedOverride.value_or_default())
                newDesc.MipLODBias = Config::Instance()->MipmapBiasOverride.value();
            else if (Config::Instance()->MipmapBiasScaleOverride.value_or_default())
                newDesc.MipLODBias = newDesc.MipLODBias * Config::Instance()->MipmapBiasOverride.value();
            else
                newDesc.MipLODBias = newDesc.MipLODBias + Config::Instance()->MipmapBiasOverride.value();
        }

        if (State::Instance().lastMipBiasMax < newDesc.MipLODBias)
            State::Instance().lastMipBiasMax = newDesc.MipLODBias;

        if (State::Instance().lastMipBias > newDesc.MipLODBias)
            State::Instance().lastMipBias = newDesc.MipLODBias;
    }

    return o_CreateSampler(device, &newDesc, DestDescriptor);
}

static void HookToDevice(ID3D12Device* InDevice)
{
    if (o_CreateSampler != nullptr || InDevice == nullptr)
        return;

    LOG_DEBUG("Dx12");

    // Get the vtable pointer
    PVOID* pVTable = *(PVOID**) InDevice;

    ID3D12Device* realDevice = nullptr;
    if (Util::CheckForRealObject(__FUNCTION__, InDevice, (IUnknown**) &realDevice))
        pVTable = *(PVOID**) realDevice;

    // hudless
    o_D3D12DeviceRelease = (PFN_Release) pVTable[2];
    o_CreateSampler = (PFN_CreateSampler) pVTable[22];
    o_CheckFeatureSupport = (PFN_CheckFeatureSupport) pVTable[13];
    o_GetResourceAllocationInfo = (PFN_GetResourceAllocationInfo) pVTable[25];
    o_CreateCommittedResource = (PFN_CreateCommittedResource) pVTable[27];
    o_CreatePlacedResource = (PFN_CreatePlacedResource) pVTable[29];

    // Apply the detour
    if (o_CreateSampler != nullptr)
    {
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());

        if (o_CreateSampler != nullptr)
            DetourAttach(&(PVOID&) o_CreateSampler, hkCreateSampler);

        if (Config::Instance()->UESpoofIntelAtomics64.value_or_default())
        {
            if (o_CheckFeatureSupport != nullptr)
                DetourAttach(&(PVOID&) o_CheckFeatureSupport, hkCheckFeatureSupport);

            if (o_CreateCommittedResource != nullptr)
                DetourAttach(&(PVOID&) o_CreateCommittedResource, hkCreateCommittedResource);

            if (o_CreatePlacedResource != nullptr)
                DetourAttach(&(PVOID&) o_CreatePlacedResource, hkCreatePlacedResource);

            if (o_D3D12DeviceRelease != nullptr)
                DetourAttach(&(PVOID&) o_D3D12DeviceRelease, hkD3D12DeviceRelease);

            // This does not work but luckily
            // UE works without Intel Extension for it
            // if (o_GetResourceAllocationInfo != nullptr)
            //    DetourAttach(&(PVOID&) o_GetResourceAllocationInfo, hkGetResourceAllocationInfo);
        }

        DetourTransactionCommit();
    }

    if (State::Instance().activeFgInput == FGInput::Upscaler)
        ResTrack_Dx12::HookDevice(InDevice);
}

static void UnhookDevice()
{
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    if (o_CreateSampler != nullptr)
        DetourDetach(&(PVOID&) o_CreateSampler, hkCreateSampler);

    if (o_CheckFeatureSupport != nullptr)
        DetourDetach(&(PVOID&) o_CheckFeatureSupport, hkCheckFeatureSupport);

    if (o_CreateCommittedResource != nullptr)
        DetourDetach(&(PVOID&) o_CreateCommittedResource, hkCreateCommittedResource);

    if (o_CreatePlacedResource != nullptr)
        DetourDetach(&(PVOID&) o_CreatePlacedResource, hkCreatePlacedResource);

    if (o_D3D12DeviceRelease != nullptr)
        DetourDetach(&(PVOID&) o_D3D12DeviceRelease, hkD3D12DeviceRelease);

    if (o_GetResourceAllocationInfo != nullptr)
        DetourDetach(&(PVOID&) o_GetResourceAllocationInfo, hkGetResourceAllocationInfo);

    o_CreateSampler = nullptr;
    o_CheckFeatureSupport = nullptr;
    o_CreateCommittedResource = nullptr;
    o_CreatePlacedResource = nullptr;
    o_D3D12DeviceRelease = nullptr;
    o_GetResourceAllocationInfo = nullptr;

    DetourTransactionCommit();
}

void D3D12Hooks::Hook()
{
    if (o_D3D12CreateDevice != nullptr)
        return;

    LOG_DEBUG("");

    o_D3D12CreateDevice = D3d12Proxy::Hook_D3D12CreateDevice(hkD3D12CreateDevice);
    o_D3D12SerializeRootSignature = D3d12Proxy::Hook_D3D12SerializeRootSignature(hkD3D12SerializeRootSignature);
    o_D3D12SerializeVersionedRootSignature =
        D3d12Proxy::Hook_D3D12SerializeVersionedRootSignature(hkD3D12SerializeVersionedRootSignature);
}

void D3D12Hooks::Unhook()
{
    if (o_D3D12CreateDevice == nullptr)
        return;

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    if (o_CreateSampler != nullptr)
    {
        DetourDetach(&(PVOID&) o_CreateSampler, hkCreateSampler);
        o_CreateSampler = nullptr;
    }

    if (o_CheckFeatureSupport != nullptr)
    {
        DetourDetach(&(PVOID&) o_CheckFeatureSupport, hkCheckFeatureSupport);
        o_CheckFeatureSupport = nullptr;
    }

    if (o_CreateCommittedResource != nullptr)
    {
        DetourDetach(&(PVOID&) o_CreateCommittedResource, hkCreateCommittedResource);
        o_CreateCommittedResource = nullptr;
    }

    if (o_CreatePlacedResource != nullptr)
    {
        DetourDetach(&(PVOID&) o_CreatePlacedResource, hkCreatePlacedResource);
        o_CreatePlacedResource = nullptr;
    }

    if (o_GetResourceAllocationInfo != nullptr)
    {
        DetourDetach(&(PVOID&) o_GetResourceAllocationInfo, hkGetResourceAllocationInfo);
        o_GetResourceAllocationInfo = nullptr;
    }

    if (o_D3D12DeviceRelease != nullptr)
    {
        DetourDetach(&(PVOID&) o_D3D12DeviceRelease, hkD3D12DeviceRelease);
        o_D3D12DeviceRelease = nullptr;
    }

    DetourTransactionCommit();
}
