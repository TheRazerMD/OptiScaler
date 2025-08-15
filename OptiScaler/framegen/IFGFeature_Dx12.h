#pragma once
#include <pch.h>
#include "IFGFeature.h"

#include <upscalers/IFeature.h>

#include <shaders/resource_flip/RF_Dx12.h>
#include <shaders/hudless_compare/HC_Dx12.h>

#include <dxgi1_6.h>
#include <d3d12.h>

typedef struct Dx12Resource
{
    FG_ResourceType type;
    ID3D12Resource* resource = nullptr;
    UINT top = 0;
    UINT left = 0;
    UINT width = 0;
    UINT height = 0;
    ID3D12GraphicsCommandList* cmdList = nullptr;
    D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
    FG_ResourceValidity validity = FG_ResourceValidity::ValidNow;

    // TODO: make private?
    ID3D12Resource* copy = nullptr;
    UINT64 fenceValue = 0;
    bool waitingExecution = false;

    ID3D12Resource* GetResource() { return (copy == nullptr) ? resource : copy; }
};

class IFGFeature_Dx12 : public virtual IFGFeature
{
  private:
    ID3D12GraphicsCommandList* _copyCommandList {};
    ID3D12CommandAllocator* _copyCommandAllocator {};

    bool InitCopyCmdList();
    void DestroyCopyCmdList();

  protected:
    ID3D12Device* _device = nullptr;
    IDXGISwapChain* _swapChain = nullptr;
    ID3D12CommandQueue* _gameCommandQueue = nullptr;

    HWND _hwnd = NULL;

    std::map<FG_ResourceType, Dx12Resource> _frameResources[BUFFER_COUNT] {};
    std::map<FG_ResourceType, ID3D12Resource*> _resourceCopy[BUFFER_COUNT] {};

    std::unique_ptr<RF_Dx12> _mvFlip;
    std::unique_ptr<RF_Dx12> _depthFlip;
    std::unique_ptr<HC_Dx12> _hudlessCompare;

    bool CreateBufferResource(ID3D12Device* InDevice, ID3D12Resource* InSource, D3D12_RESOURCE_STATES InState,
                              ID3D12Resource** OutResource, bool UAV = false, bool depth = false);
    bool CreateBufferResourceWithSize(ID3D12Device* device, ID3D12Resource* source, D3D12_RESOURCE_STATES state,
                                      ID3D12Resource** target, UINT width, UINT height, bool UAV, bool depth);
    void ResourceBarrier(ID3D12GraphicsCommandList* InCommandList, ID3D12Resource* InResource,
                         D3D12_RESOURCE_STATES InBeforeState, D3D12_RESOURCE_STATES InAfterState);
    bool CopyResource(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* source, ID3D12Resource** target,
                      D3D12_RESOURCE_STATES sourceState);

    void NewFrame() override final;
    void FlipResource(Dx12Resource* resource);

  public:
    virtual bool CreateSwapchain(IDXGIFactory* factory, ID3D12CommandQueue* cmdQueue, DXGI_SWAP_CHAIN_DESC* desc,
                                 IDXGISwapChain** swapChain) = 0;
    virtual bool CreateSwapchain1(IDXGIFactory* factory, ID3D12CommandQueue* cmdQueue, HWND hwnd,
                                  DXGI_SWAP_CHAIN_DESC1* desc, DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pFullscreenDesc,
                                  IDXGISwapChain1** swapChain) = 0;
    virtual bool ReleaseSwapchain(HWND hwnd) = 0;

    virtual void CreateContext(ID3D12Device* device, FG_Constants& fgConstants) = 0;
    virtual void EvaluateState(ID3D12Device* device, FG_Constants& fgConstants) = 0;

    virtual void* FrameGenerationContext() = 0;
    virtual void* SwapchainContext() = 0;

    virtual void ReleaseObjects() = 0;
    virtual void CreateObjects(ID3D12Device* InDevice) = 0;

    Dx12Resource* GetResource(FG_ResourceType type);
    bool GetResourceCopy(FG_ResourceType type, D3D12_RESOURCE_STATES bufferState, ID3D12Resource* output);

    virtual void SetResource(Dx12Resource* inputResource) = 0;

    virtual void SetCommandQueue(FG_ResourceType type, ID3D12CommandQueue* queue) = 0;

    IFGFeature_Dx12() = default;
    virtual ~IFGFeature_Dx12() { DestroyCopyCmdList(); }
};
