#pragma once

#include "pch.h"
#include "Util.h"
#include "Config.h"
#include "Logger.h"

#include <proxies/Ntdll_Proxy.h>
#include <proxies/KernelBase_Proxy.h>

#include <xefg_swapchain.h>
#include <xefg_swapchain_d3d12.h>
#include <xefg_swapchain_debug.h>

#pragma comment(lib, "Version.lib")

// Common
typedef decltype(&xefgSwapChainGetVersion) PFN_xefgSwapChainGetVersion;
typedef decltype(&xefgSwapChainGetProperties) PFN_xefgSwapChainGetProperties;
typedef decltype(&xefgSwapChainTagFrameConstants) PFN_xefgSwapChainTagFrameConstants;
typedef decltype(&xefgSwapChainSetEnabled) PFN_xefgSwapChainSetEnabled;
typedef decltype(&xefgSwapChainSetPresentId) PFN_xefgSwapChainSetPresentId;
typedef decltype(&xefgSwapChainGetLastPresentStatus) PFN_xefgSwapChainGetLastPresentStatus;
typedef decltype(&xefgSwapChainSetLoggingCallback) PFN_xefgSwapChainSetLoggingCallback;
typedef decltype(&xefgSwapChainDestroy) PFN_xefgSwapChainDestroy;
typedef decltype(&xefgSwapChainSetLatencyReduction) PFN_xefgSwapChainSetLatencyReduction;
typedef decltype(&xefgSwapChainSetSceneChangeThreshold) PFN_xefgSwapChainSetSceneChangeThreshold;
typedef decltype(&xefgSwapChainGetPipelineBuildStatus) PFN_xefgSwapChainGetPipelineBuildStatus;

// Dx12
typedef decltype(&xefgSwapChainD3D12CreateContext) PFN_xefgSwapChainD3D12CreateContext;
typedef decltype(&xefgSwapChainD3D12BuildPipelines) PFN_xefgSwapChainD3D12BuildPipelines;
typedef decltype(&xefgSwapChainD3D12InitFromSwapChain) PFN_xefgSwapChainD3D12InitFromSwapChain;
typedef decltype(&xefgSwapChainD3D12InitFromSwapChainDesc) PFN_xefgSwapChainD3D12InitFromSwapChainDesc;
typedef decltype(&xefgSwapChainD3D12GetSwapChainPtr) PFN_xefgSwapChainD3D12GetSwapChainPtr;
typedef decltype(&xefgSwapChainD3D12TagFrameResource) PFN_xefgSwapChainD3D12TagFrameResource;
typedef decltype(&xefgSwapChainD3D12SetDescriptorHeap) PFN_xefgSwapChainD3D12SetDescriptorHeap;

// Debug
typedef decltype(&xefgSwapChainEnableDebugFeature) PFN_xefgSwapChainEnableDebugFeature;

class XeFGProxy
{
  private:
    inline static HMODULE _dll = nullptr;

    inline static feature_version _xefgVersion {};

    // Common
    inline static PFN_xefgSwapChainGetVersion _xefgSwapChainGetVersion = nullptr;
    inline static PFN_xefgSwapChainGetProperties _xefgSwapChainGetProperties = nullptr;
    inline static PFN_xefgSwapChainTagFrameConstants _xefgSwapChainTagFrameConstants = nullptr;
    inline static PFN_xefgSwapChainSetEnabled _xefgSwapChainSetEnabled = nullptr;
    inline static PFN_xefgSwapChainSetPresentId _xefgSwapChainSetPresentId = nullptr;
    inline static PFN_xefgSwapChainGetLastPresentStatus _xefgSwapChainGetLastPresentStatus = nullptr;
    inline static PFN_xefgSwapChainSetLoggingCallback _xefgSwapChainSetLoggingCallback = nullptr;
    inline static PFN_xefgSwapChainDestroy _xefgSwapChainDestroy = nullptr;
    inline static PFN_xefgSwapChainSetLatencyReduction _xefgSwapChainSetLatencyReduction = nullptr;
    inline static PFN_xefgSwapChainSetSceneChangeThreshold _xefgSwapChainSetSceneChangeThreshold = nullptr;
    inline static PFN_xefgSwapChainGetPipelineBuildStatus _xefgSwapChainGetPipelineBuildStatus = nullptr;

    // Dx12
    inline static PFN_xefgSwapChainD3D12CreateContext _xefgSwapChainD3D12CreateContext = nullptr;
    inline static PFN_xefgSwapChainD3D12BuildPipelines _xefgSwapChainD3D12BuildPipelines = nullptr;
    inline static PFN_xefgSwapChainD3D12InitFromSwapChain _xefgSwapChainD3D12InitFromSwapChain = nullptr;
    inline static PFN_xefgSwapChainD3D12InitFromSwapChainDesc _xefgSwapChainD3D12InitFromSwapChainDesc = nullptr;
    inline static PFN_xefgSwapChainD3D12GetSwapChainPtr _xefgSwapChainD3D12GetSwapChainPtr = nullptr;
    inline static PFN_xefgSwapChainD3D12TagFrameResource _xefgSwapChainD3D12TagFrameResource = nullptr;
    inline static PFN_xefgSwapChainD3D12SetDescriptorHeap _xefgSwapChainD3D12SetDescriptorHeap = nullptr;

    // Debug
    inline static PFN_xefgSwapChainEnableDebugFeature _xefgSwapChainEnableDebugFeature = nullptr;

    inline static xefg_swapchain_version_t GetDLLVersion(std::wstring dllPath)
    {
        // Step 1: Get the size of the version information
        DWORD handle = 0;
        DWORD versionSize = GetFileVersionInfoSizeW(dllPath.c_str(), &handle);
        xefg_swapchain_version_t version { 0, 0, 0 };

        if (versionSize == 0)
        {
            LOG_ERROR("Failed to get version info size: {0:X}", GetLastError());
            return version;
        }

        // Step 2: Allocate buffer and get the version information
        std::vector<BYTE> versionInfo(versionSize);
        if (handle == 0 && !GetFileVersionInfoW(dllPath.c_str(), handle, versionSize, versionInfo.data()))
        {
            LOG_ERROR("Failed to get version info: {0:X}", GetLastError());
            return version;
        }

        // Step 3: Extract the version information
        VS_FIXEDFILEINFO* fileInfo = nullptr;
        UINT size = 0;
        if (!VerQueryValueW(versionInfo.data(), L"\\", reinterpret_cast<LPVOID*>(&fileInfo), &size))
        {
            LOG_ERROR("Failed to query version value: {0:X}", GetLastError());
            return version;
        }

        if (fileInfo != nullptr)
        {
            // Extract major, minor, build, and revision numbers from version information
            DWORD fileVersionMS = fileInfo->dwFileVersionMS;
            DWORD fileVersionLS = fileInfo->dwFileVersionLS;

            version.major = (fileVersionMS >> 16) & 0xffff;
            version.minor = (fileVersionMS >> 0) & 0xffff;
            version.patch = (fileVersionLS >> 16) & 0xffff;
            version.reserved = (fileVersionLS >> 0) & 0xffff;
        }
        else
        {
            LOG_ERROR("No version information found!");
        }

        return version;
    }

    inline static std::filesystem::path DllPath(HMODULE module)
    {
        static std::filesystem::path dll;

        if (dll.empty())
        {
            wchar_t dllPath[MAX_PATH];
            GetModuleFileNameW(module, dllPath, MAX_PATH);
            dll = std::filesystem::path(dllPath);
        }

        return dll;
    }

  public:
    static HMODULE Module() { return _dll; }

    static bool InitXeFG()
    {
        if (_dll != nullptr)
            return true;

        HMODULE mainModule = nullptr;

        mainModule = GetModuleHandle(L"libxess_fg.dll");
        if (mainModule != nullptr)
        {
            _dll = mainModule;
            // return true;
        }

        auto dllPath = Util::DllPath();

        std::wstring libraryName;
        libraryName = L"libxess_fg.dll";

        // we would like to prioritize file pointed at ini
        // if (Config::Instance()->XeSSLibrary.has_value())
        //{
        //    std::filesystem::path cfgPath(Config::Instance()->XeSSLibrary.value().c_str());
        //    LOG_INFO(L"Trying to load libxell.dll from ini path: {}", cfgPath.wstring());

        //    cfgPath = cfgPath / libraryName;
        //    mainModule = KernelBaseProxy::LoadLibraryExW_()(cfgPath.c_str(), NULL, 0);
        //}

        if (mainModule == nullptr)
        {
            std::filesystem::path libXeFGPath = dllPath.parent_path() / libraryName;
            LOG_INFO(L"Trying to load libxess_fg.dll from dll path: {}", libXeFGPath.wstring());
            mainModule = NtdllProxy::LoadLibraryExW_Ldr(libXeFGPath.c_str(), NULL, 0);
        }

        if (mainModule != nullptr)
            return HookXeFG(mainModule);

        return false;
    }

    static bool HookXeFG(HMODULE libxefgModule)
    {
        // if dll already loaded
        if (_dll != nullptr && _xefgSwapChainGetVersion != nullptr)
            return true;

        spdlog::info("");

        if (libxefgModule == nullptr)
            return false;

        _dll = libxefgModule;

        State::Instance().skipDxgiLoadChecks = true;

        if (_dll != nullptr)
        {
            // Common
            _xefgSwapChainGetVersion =
                (PFN_xefgSwapChainGetVersion) KernelBaseProxy::GetProcAddress_()(_dll, "xefgSwapChainGetVersion");
            _xefgSwapChainGetProperties =
                (PFN_xefgSwapChainGetProperties) KernelBaseProxy::GetProcAddress_()(_dll, "xefgSwapChainGetProperties");
            _xefgSwapChainTagFrameConstants = (PFN_xefgSwapChainTagFrameConstants) KernelBaseProxy::GetProcAddress_()(
                _dll, "xefgSwapChainTagFrameConstants");
            _xefgSwapChainSetEnabled =
                (PFN_xefgSwapChainSetEnabled) KernelBaseProxy::GetProcAddress_()(_dll, "xefgSwapChainSetEnabled");
            _xefgSwapChainSetPresentId =
                (PFN_xefgSwapChainSetPresentId) KernelBaseProxy::GetProcAddress_()(_dll, "xefgSwapChainSetPresentId");
            _xefgSwapChainGetLastPresentStatus =
                (PFN_xefgSwapChainGetLastPresentStatus) KernelBaseProxy::GetProcAddress_()(
                    _dll, "xefgSwapChainGetLastPresentStatus");
            _xefgSwapChainSetLoggingCallback = (PFN_xefgSwapChainSetLoggingCallback) KernelBaseProxy::GetProcAddress_()(
                _dll, "xefgSwapChainSetLoggingCallback");
            _xefgSwapChainDestroy =
                (PFN_xefgSwapChainDestroy) KernelBaseProxy::GetProcAddress_()(_dll, "xefgSwapChainDestroy");
            _xefgSwapChainSetLatencyReduction =
                (PFN_xefgSwapChainSetLatencyReduction) KernelBaseProxy::GetProcAddress_()(
                    _dll, "xefgSwapChainSetLatencyReduction");
            _xefgSwapChainSetSceneChangeThreshold =
                (PFN_xefgSwapChainSetSceneChangeThreshold) KernelBaseProxy::GetProcAddress_()(
                    _dll, "xefgSwapChainSetSceneChangeThreshold");
            _xefgSwapChainGetPipelineBuildStatus =
                (PFN_xefgSwapChainGetPipelineBuildStatus) KernelBaseProxy::GetProcAddress_()(
                    _dll, "xefgSwapChainGetPipelineBuildStatus");

            // Dx12
            _xefgSwapChainD3D12CreateContext = (PFN_xefgSwapChainD3D12CreateContext) KernelBaseProxy::GetProcAddress_()(
                _dll, "xefgSwapChainD3D12CreateContext");
            _xefgSwapChainD3D12BuildPipelines =
                (PFN_xefgSwapChainD3D12BuildPipelines) KernelBaseProxy::GetProcAddress_()(
                    _dll, "xefgSwapChainD3D12BuildPipelines");
            _xefgSwapChainD3D12InitFromSwapChain =
                (PFN_xefgSwapChainD3D12InitFromSwapChain) KernelBaseProxy::GetProcAddress_()(
                    _dll, "xefgSwapChainD3D12InitFromSwapChain");
            _xefgSwapChainD3D12InitFromSwapChainDesc =
                (PFN_xefgSwapChainD3D12InitFromSwapChainDesc) KernelBaseProxy::GetProcAddress_()(
                    _dll, "xefgSwapChainD3D12InitFromSwapChainDesc");
            _xefgSwapChainD3D12GetSwapChainPtr =
                (PFN_xefgSwapChainD3D12GetSwapChainPtr) KernelBaseProxy::GetProcAddress_()(
                    _dll, "xefgSwapChainD3D12GetSwapChainPtr");
            _xefgSwapChainD3D12TagFrameResource =
                (PFN_xefgSwapChainD3D12TagFrameResource) KernelBaseProxy::GetProcAddress_()(
                    _dll, "xefgSwapChainD3D12TagFrameResource");
            _xefgSwapChainD3D12SetDescriptorHeap =
                (PFN_xefgSwapChainD3D12SetDescriptorHeap) KernelBaseProxy::GetProcAddress_()(
                    _dll, "xefgSwapChainD3D12SetDescriptorHeap");

            // Debug
            _xefgSwapChainEnableDebugFeature = (PFN_xefgSwapChainEnableDebugFeature) KernelBaseProxy::GetProcAddress_()(
                _dll, "xefgSwapChainEnableDebugFeature");
        }

        State::Instance().skipDxgiLoadChecks = true;

        bool loadResult = _xefgSwapChainGetVersion != nullptr;
        LOG_INFO("LoadResult: {}", loadResult);
        return loadResult;
    }

    static feature_version Version()
    {
        if (_xefgVersion.major == 0 && _xefgSwapChainGetVersion != nullptr)
        {
            xefg_swapchain_version_t tempVersion;
            if (auto result = _xefgSwapChainGetVersion(&tempVersion); result == XESS_RESULT_SUCCESS)
            {
                _xefgVersion.major = tempVersion.major;
                _xefgVersion.minor = tempVersion.minor;
                _xefgVersion.patch = tempVersion.patch;

                LOG_INFO("XeFG Version: v{}.{}.{}", _xefgVersion.major, _xefgVersion.minor, _xefgVersion.patch);
            }
            else
            {
                LOG_ERROR("Can't get XeFG version: {}", (UINT) result);
            }
        }

        if (_xefgVersion.major == 0)
        {
            _xefgVersion.major = 1;
            _xefgVersion.minor = 0;
            _xefgVersion.patch = 0;
        }

        return _xefgVersion;
    }

    // Common
    static PFN_xefgSwapChainGetVersion GetVersion() { return _xefgSwapChainGetVersion; }
    static PFN_xefgSwapChainGetProperties GetProperties() { return _xefgSwapChainGetProperties; }
    static PFN_xefgSwapChainTagFrameConstants TagFrameConstants() { return _xefgSwapChainTagFrameConstants; }
    static PFN_xefgSwapChainSetEnabled SetEnabled() { return _xefgSwapChainSetEnabled; }
    static PFN_xefgSwapChainSetPresentId SetPresentId() { return _xefgSwapChainSetPresentId; }
    static PFN_xefgSwapChainGetLastPresentStatus GetLastPresentStatus() { return _xefgSwapChainGetLastPresentStatus; }
    static PFN_xefgSwapChainSetLoggingCallback SetLoggingCallback() { return _xefgSwapChainSetLoggingCallback; }
    static PFN_xefgSwapChainDestroy Destroy() { return _xefgSwapChainDestroy; }
    static PFN_xefgSwapChainSetLatencyReduction SetLatencyReduction() { return _xefgSwapChainSetLatencyReduction; }
    static PFN_xefgSwapChainSetSceneChangeThreshold SetSceneChangeThreshold()
    {
        return _xefgSwapChainSetSceneChangeThreshold;
    }
    static PFN_xefgSwapChainGetPipelineBuildStatus GetPipelineBuildStatus()
    {
        return _xefgSwapChainGetPipelineBuildStatus;
    }

    // Dx12
    static PFN_xefgSwapChainD3D12CreateContext D3D12CreateContext() { return _xefgSwapChainD3D12CreateContext; }
    static PFN_xefgSwapChainD3D12BuildPipelines D3D12BuildPipelines() { return _xefgSwapChainD3D12BuildPipelines; }
    static PFN_xefgSwapChainD3D12InitFromSwapChain D3D12InitFromSwapChain()
    {
        return _xefgSwapChainD3D12InitFromSwapChain;
    }
    static PFN_xefgSwapChainD3D12InitFromSwapChainDesc D3D12InitFromSwapChainDesc()
    {
        return _xefgSwapChainD3D12InitFromSwapChainDesc;
    }
    static PFN_xefgSwapChainD3D12GetSwapChainPtr D3D12GetSwapChainPtr() { return _xefgSwapChainD3D12GetSwapChainPtr; }
    static PFN_xefgSwapChainD3D12TagFrameResource D3D12TagFrameResource()
    {
        return _xefgSwapChainD3D12TagFrameResource;
    }
    static PFN_xefgSwapChainD3D12SetDescriptorHeap D3D12SetDescriptorHeap()
    {
        return _xefgSwapChainD3D12SetDescriptorHeap;
    }

    // Debug
    static PFN_xefgSwapChainEnableDebugFeature EnableDebugFeature() { return _xefgSwapChainEnableDebugFeature; }
};
