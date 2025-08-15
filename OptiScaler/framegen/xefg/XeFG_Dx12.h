#pragma once

#include <framegen/IFGFeature_Dx12.h>

#include <proxies/XeLL_Proxy.h>
#include <proxies/XeFG_Proxy.h>

#include <xell.h>
#include <xell_d3d12.h>
#include <xefg_swapchain.h>
#include <xefg_swapchain_d3d12.h>
#include <xefg_swapchain_debug.h>

#include <nvapi/fakenvapi.h>

class XeFG_Dx12 : public virtual IFGFeature_Dx12
{
  private:
    xefg_swapchain_handle_t _swapChainContext = nullptr;
    xefg_swapchain_handle_t _fgContext = nullptr;

    uint32_t _width = 0;
    uint32_t _height = 0;
    int _featureFlags = 0;

    static void xefgLogCallback(const char* message, xefg_swapchain_logging_level_t level, void* userData);

    bool CreateSwapchainContext(ID3D12Device* device);
    bool DestroySwapchainContext();
    xefg_swapchain_d3d12_resource_data_t GetResourceData(FG_ResourceType type);

    bool Dispatch();

  public:
    // IFGFeature
    const char* Name() override final;
    feature_version Version() override final;

    // IFGFeature_Dx12
    bool CreateSwapchain(IDXGIFactory* factory, ID3D12CommandQueue* cmdQueue, DXGI_SWAP_CHAIN_DESC* desc,
                         IDXGISwapChain** swapChain) override final;
    bool CreateSwapchain1(IDXGIFactory* factory, ID3D12CommandQueue* cmdQueue, HWND hwnd, DXGI_SWAP_CHAIN_DESC1* desc,
                          DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pFullscreenDesc, IDXGISwapChain1** swapChain) override final;
    bool ReleaseSwapchain(HWND hwnd) override final;

    void CreateContext(ID3D12Device* device, FG_Constants& fgConstants) override final;
    void StopAndDestroyContext(bool destroy, bool shutDown) override final;

    void EvaluateState(ID3D12Device* device, FG_Constants& fgConstants) override final;

    void ReleaseObjects() override final;
    void CreateObjects(ID3D12Device* InDevice) override final;

    bool Present() override final;

    void SetResource(Dx12Resource* inputResource) override final;
    void SetResourceReady(FG_ResourceType type) override final;
    void SetCommandQueue(FG_ResourceType type, ID3D12CommandQueue* queue) override final;

    void* FrameGenerationContext() override final;
    void* SwapchainContext() override final;

    XeFG_Dx12() : IFGFeature_Dx12(), IFGFeature()
    {
        if (XeFGProxy::Module() == nullptr)
            XeFGProxy::InitXeFG();
    }
};
