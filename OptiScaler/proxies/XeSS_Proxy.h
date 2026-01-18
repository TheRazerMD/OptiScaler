#pragma once

#include "pch.h"
#include "Util.h"
#include "Config.h"
#include "Logger.h"

#include <proxies/Ntdll_Proxy.h>
#include <proxies/KernelBase_Proxy.h>

#include <inputs/XeSS_Common.h>
#include <inputs/XeSS_Dx12.h>
#include <inputs/XeSS_Dx11.h>
#include <inputs/XeSS_Vulkan.h>
#include <inputs/XeSS_Dbg.h>

#include "xess_debug.h"
#include "xess_d3d11.h"
#include "xess_d3d12.h"
#include "xess_d3d12_debug.h"
#include "xess_vk.h"
#include "xess_vk_debug.h"

#include "detours/detours.h"

#pragma comment(lib, "Version.lib")

typedef decltype(&xessD3D12CreateContext) PFN_xessD3D12CreateContext;
typedef decltype(&xessD3D12BuildPipelines) PFN_xessD3D12BuildPipelines;
typedef decltype(&xessD3D12Init) PFN_xessD3D12Init;
typedef decltype(&xessD3D12Execute) PFN_xessD3D12Execute;
typedef decltype(&xessSelectNetworkModel) PFN_xessSelectNetworkModel;
typedef decltype(&xessStartDump) PFN_xessStartDump;
typedef decltype(&xessGetVersion) PFN_xessGetVersion;
typedef decltype(&xessIsOptimalDriver) PFN_xessIsOptimalDriver;
typedef decltype(&xessSetLoggingCallback) PFN_xessSetLoggingCallback;
typedef decltype(&xessGetProperties) PFN_xessGetProperties;
typedef decltype(&xessDestroyContext) PFN_xessDestroyContext;
typedef decltype(&xessSetVelocityScale) PFN_xessSetVelocityScale;
typedef decltype(&xessGetVelocityScale) PFN_xessGetVelocityScale;

typedef decltype(&xessD3D12GetInitParams) PFN_xessD3D12GetInitParams;
typedef decltype(&xessForceLegacyScaleFactors) PFN_xessForceLegacyScaleFactors;
typedef decltype(&xessGetExposureMultiplier) PFN_xessGetExposureMultiplier;
typedef decltype(&xessGetInputResolution) PFN_xessGetInputResolution;
typedef decltype(&xessGetIntelXeFXVersion) PFN_xessGetIntelXeFXVersion;
typedef decltype(&xessGetJitterScale) PFN_xessGetJitterScale;
typedef decltype(&xessGetOptimalInputResolution) PFN_xessGetOptimalInputResolution;
typedef decltype(&xessSetExposureMultiplier) PFN_xessSetExposureMultiplier;
typedef decltype(&xessSetJitterScale) PFN_xessSetJitterScale;

typedef decltype(&xessD3D12GetResourcesToDump) PFN_xessD3D12GetResourcesToDump;
typedef decltype(&xessD3D12GetProfilingData) PFN_xessD3D12GetProfilingData;

typedef xess_result_t (*PFN_xessSetContextParameterF)(); // XeSS' headers don't export this
typedef decltype(&xessGetPipelineBuildStatus) PFN_xessGetPipelineBuildStatus;

// Vulkan?!?
typedef decltype(&xessVKGetRequiredInstanceExtensions) PFN_xessVKGetRequiredInstanceExtensions;
typedef decltype(&xessVKGetRequiredDeviceExtensions) PFN_xessVKGetRequiredDeviceExtensions;
typedef decltype(&xessVKGetRequiredDeviceFeatures) PFN_xessVKGetRequiredDeviceFeatures;
typedef decltype(&xessVKCreateContext) PFN_xessVKCreateContext;
typedef decltype(&xessVKBuildPipelines) PFN_xessVKBuildPipelines;
typedef decltype(&xessVKInit) PFN_xessVKInit;
typedef decltype(&xessVKGetInitParams) PFN_xessVKGetInitParams;
typedef decltype(&xessVKExecute) PFN_xessVKExecute;
typedef decltype(&xessVKGetResourcesToDump) PFN_xessVKGetResourcesToDump;

// Dx11
typedef decltype(&xessD3D11CreateContext) PFN_xessD3D11CreateContext;
typedef decltype(&xessD3D11Init) PFN_xessD3D11Init;
typedef decltype(&xessD3D11GetInitParams) PFN_xessD3D11GetInitParams;
typedef decltype(&xessD3D11Execute) PFN_xessD3D11Execute;

class XeSSProxy
{
  private:
    inline static HMODULE _dll = nullptr;
    inline static HMODULE _dlldx11 = nullptr;

    inline static xess_version_t _xessVersion {};
    inline static xess_version_t _xessVersionDx11 {};

    inline static PFN_xessD3D12CreateContext _xessD3D12CreateContext = nullptr;
    inline static PFN_xessD3D12BuildPipelines _xessD3D12BuildPipelines = nullptr;
    inline static PFN_xessD3D12Init _xessD3D12Init = nullptr;
    inline static PFN_xessD3D12Execute _xessD3D12Execute = nullptr;
    inline static PFN_xessSelectNetworkModel _xessSelectNetworkModel = nullptr;
    inline static PFN_xessStartDump _xessStartDump = nullptr;
    inline static PFN_xessGetVersion _xessGetVersion = nullptr;
    inline static PFN_xessIsOptimalDriver _xessIsOptimalDriver = nullptr;
    inline static PFN_xessSetLoggingCallback _xessSetLoggingCallback = nullptr;
    inline static PFN_xessGetProperties _xessGetProperties = nullptr;
    inline static PFN_xessGetVelocityScale _xessGetVelocityScale = nullptr;
    inline static PFN_xessDestroyContext _xessDestroyContext = nullptr;
    inline static PFN_xessSetVelocityScale _xessSetVelocityScale = nullptr;

    inline static PFN_xessD3D12GetInitParams _xessD3D12GetInitParams = nullptr;
    inline static PFN_xessForceLegacyScaleFactors _xessForceLegacyScaleFactors = nullptr;
    inline static PFN_xessGetExposureMultiplier _xessGetExposureMultiplier = nullptr;
    inline static PFN_xessGetInputResolution _xessGetInputResolution = nullptr;
    inline static PFN_xessGetIntelXeFXVersion _xessGetIntelXeFXVersion = nullptr;
    inline static PFN_xessGetJitterScale _xessGetJitterScale = nullptr;
    inline static PFN_xessGetOptimalInputResolution _xessGetOptimalInputResolution = nullptr;
    inline static PFN_xessSetExposureMultiplier _xessSetExposureMultiplier = nullptr;
    inline static PFN_xessSetJitterScale _xessSetJitterScale = nullptr;

    inline static PFN_xessD3D12GetResourcesToDump _xessD3D12GetResourcesToDump = nullptr;
    inline static PFN_xessD3D12GetProfilingData _xessD3D12GetProfilingData = nullptr;
    inline static PFN_xessSetContextParameterF _xessSetContextParameterF = nullptr;
    inline static PFN_xessGetPipelineBuildStatus _xessGetPipelineBuildStatus = nullptr;

    inline static PFN_xessVKGetRequiredInstanceExtensions _xessVKGetRequiredInstanceExtensions = nullptr;
    inline static PFN_xessVKGetRequiredDeviceExtensions _xessVKGetRequiredDeviceExtensions = nullptr;
    inline static PFN_xessVKGetRequiredDeviceFeatures _xessVKGetRequiredDeviceFeatures = nullptr;
    inline static PFN_xessVKCreateContext _xessVKCreateContext = nullptr;
    inline static PFN_xessVKBuildPipelines _xessVKBuildPipelines = nullptr;
    inline static PFN_xessVKInit _xessVKInit = nullptr;
    inline static PFN_xessVKGetInitParams _xessVKGetInitParams = nullptr;
    inline static PFN_xessVKExecute _xessVKExecute = nullptr;
    inline static PFN_xessVKGetResourcesToDump _xessVKGetResourcesToDump = nullptr;

    inline static PFN_xessD3D11CreateContext _xessD3D11CreateContext = nullptr;
    inline static PFN_xessD3D11Init _xessD3D11Init = nullptr;
    inline static PFN_xessD3D11GetInitParams _xessD3D11GetInitParams = nullptr;
    inline static PFN_xessD3D11Execute _xessD3D11Execute = nullptr;

    inline static PFN_xessSelectNetworkModel _xessSelectNetworkModelDx11 = nullptr;
    inline static PFN_xessStartDump _xessStartDumpDx11 = nullptr;
    inline static PFN_xessGetVersion _xessGetVersionDx11 = nullptr;
    inline static PFN_xessIsOptimalDriver _xessIsOptimalDriverDx11 = nullptr;
    inline static PFN_xessSetLoggingCallback _xessSetLoggingCallbackDx11 = nullptr;
    inline static PFN_xessGetVelocityScale _xessGetVelocityScaleDx11 = nullptr;
    inline static PFN_xessGetProperties _xessGetPropertiesDx11 = nullptr;
    inline static PFN_xessDestroyContext _xessDestroyContextDx11 = nullptr;
    inline static PFN_xessSetVelocityScale _xessSetVelocityScaleDx11 = nullptr;
    inline static PFN_xessForceLegacyScaleFactors _xessForceLegacyScaleFactorsDx11 = nullptr;
    inline static PFN_xessGetExposureMultiplier _xessGetExposureMultiplierDx11 = nullptr;
    inline static PFN_xessGetInputResolution _xessGetInputResolutionDx11 = nullptr;
    inline static PFN_xessGetIntelXeFXVersion _xessGetIntelXeFXVersionDx11 = nullptr;
    inline static PFN_xessGetJitterScale _xessGetJitterScaleDx11 = nullptr;
    inline static PFN_xessGetOptimalInputResolution _xessGetOptimalInputResolutionDx11 = nullptr;
    inline static PFN_xessSetExposureMultiplier _xessSetExposureMultiplierDx11 = nullptr;
    inline static PFN_xessSetJitterScale _xessSetJitterScaleDx11 = nullptr;
    inline static PFN_xessSetContextParameterF _xessSetContextParameterFDx11 = nullptr;
    inline static PFN_xessGetPipelineBuildStatus _xessGetPipelineBuildStatusDx11 = nullptr;

    inline static xess_version_t GetDLLVersion(std::wstring dllPath)
    {
        // Step 1: Get the size of the version information
        DWORD handle = 0;
        DWORD versionSize = GetFileVersionInfoSizeW(dllPath.c_str(), &handle);
        xess_version_t version { 0, 0, 0 };

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
    static HMODULE ModuleDx11() { return _dlldx11; }

    static bool InitXeSS()
    {
        if (_dll != nullptr)
            return true;

        HMODULE mainModule = nullptr;

        mainModule = GetModuleHandle(L"libxess.dll");
        if (mainModule != nullptr)
        {
            _dll = mainModule;
            return true;
        }

        auto dllPath = Util::DllPath();

        std::wstring libraryName;
        libraryName = L"libxess.dll";

        // we would like to prioritize file pointed at ini
        if (Config::Instance()->XeSSLibrary.has_value())
        {
            std::filesystem::path cfgPath(Config::Instance()->XeSSLibrary.value().c_str());
            LOG_INFO(L"Trying to load libxess.dll from ini path: {}", cfgPath.wstring());

            cfgPath = cfgPath / libraryName;
            mainModule = NtdllProxy::LoadLibraryExW_Ldr(cfgPath.c_str(), NULL, 0);
        }

        if (mainModule == nullptr)
        {
            std::filesystem::path libXessPath = dllPath.parent_path() / libraryName;
            LOG_INFO(L"Trying to load libxess.dll from dll path: {}", libXessPath.wstring());
            mainModule = NtdllProxy::LoadLibraryExW_Ldr(libXessPath.c_str(), NULL, 0);
        }

        if (mainModule != nullptr)
            return HookXeSS(mainModule);

        return false;
    }

    static bool InitXeSSDx11()
    {
        if (_dlldx11 != nullptr)
            return true;

        HMODULE dx11Module = nullptr;

        do
        {
            dx11Module = GetModuleHandle(L"libxess_dx11.dll");
            if (dx11Module != nullptr)
            {
                break;
            }

            auto dllPath = Util::DllPath();

            std::wstring libraryName;
            libraryName = L"libxess_dx11.dll";

            // we would like to prioritize file pointed at ini
            if (Config::Instance()->XeSSLibrary.has_value())
            {
                std::filesystem::path cfgPath(Config::Instance()->XeSSLibrary.value().c_str());
                LOG_INFO(L"Trying to load libxess.dll from ini path: {}", cfgPath.wstring());

                auto dx11Path = cfgPath.parent_path() / libraryName;
                dx11Module = NtdllProxy::LoadLibraryExW_Ldr(dx11Path.c_str(), NULL, 0);
            }

            if (dx11Module == nullptr)
            {
                std::filesystem::path libXessDx11Path = dllPath.parent_path() / libraryName;
                LOG_INFO(L"Trying to load libxess.dll from dll path: {}", libXessDx11Path.wstring());
                dx11Module = NtdllProxy::LoadLibraryExW_Ldr(libXessDx11Path.c_str(), NULL, 0);
            }
        } while (false);

        if (dx11Module != nullptr)
        {
            _dlldx11 = dx11Module;
            return HookXeSSDx11(dx11Module);
        }

        return false;
    }

    static bool HookXeSS(HMODULE libxessModule = nullptr)
    {
        // if dll already loaded
        if (_dll != nullptr && _xessD3D12CreateContext != nullptr)
            return true;

        LOG_DEBUG("Loading XeSS methods from: {:X}", (size_t) libxessModule);

        if (libxessModule != nullptr)
            _dll = libxessModule;

        if (_dll == nullptr && Config::Instance()->XeSSLibrary.has_value())
        {
            std::filesystem::path libPath(Config::Instance()->XeSSLibrary.value().c_str());

            if (libPath.has_filename())
                _dll = NtdllProxy::LoadLibraryExW_Ldr(libPath.c_str(), NULL, 0);
            else
                _dll = NtdllProxy::LoadLibraryExW_Ldr((libPath / L"libxess.dll").c_str(), NULL, 0);

            if (_dll != nullptr)
            {
                LOG_INFO("libxess.dll loaded from {0}", wstring_to_string(Config::Instance()->XeSSLibrary.value()));
            }
        }

        if (_dll == nullptr)
        {
            _dll = NtdllProxy::LoadLibraryExW_Ldr(L"libxess.dll", NULL, 0);

            if (_dll != nullptr)
                LOG_INFO("libxess.dll loaded from exe folder");
        }

        {
            ScopedSkipDxgiLoadChecks skipDxgiLoadChecks {};

            if (_dll != nullptr)
            {
                _xessD3D12CreateContext =
                    (PFN_xessD3D12CreateContext) KernelBaseProxy::GetProcAddress_()(_dll, "xessD3D12CreateContext");
                _xessD3D12BuildPipelines =
                    (PFN_xessD3D12BuildPipelines) KernelBaseProxy::GetProcAddress_()(_dll, "xessD3D12BuildPipelines");
                _xessD3D12Init = (PFN_xessD3D12Init) KernelBaseProxy::GetProcAddress_()(_dll, "xessD3D12Init");
                _xessD3D12Execute = (PFN_xessD3D12Execute) KernelBaseProxy::GetProcAddress_()(_dll, "xessD3D12Execute");
                _xessSelectNetworkModel =
                    (PFN_xessSelectNetworkModel) KernelBaseProxy::GetProcAddress_()(_dll, "xessSelectNetworkModel");
                _xessStartDump = (PFN_xessStartDump) KernelBaseProxy::GetProcAddress_()(_dll, "xessStartDump");
                _xessGetVersion = (PFN_xessGetVersion) KernelBaseProxy::GetProcAddress_()(_dll, "xessGetVersion");
                _xessIsOptimalDriver =
                    (PFN_xessIsOptimalDriver) KernelBaseProxy::GetProcAddress_()(_dll, "xessIsOptimalDriver");
                _xessSetLoggingCallback =
                    (PFN_xessSetLoggingCallback) KernelBaseProxy::GetProcAddress_()(_dll, "xessSetLoggingCallback");
                _xessGetProperties =
                    (PFN_xessGetProperties) KernelBaseProxy::GetProcAddress_()(_dll, "xessGetProperties");
                _xessDestroyContext =
                    (PFN_xessDestroyContext) KernelBaseProxy::GetProcAddress_()(_dll, "xessDestroyContext");
                _xessSetVelocityScale =
                    (PFN_xessSetVelocityScale) KernelBaseProxy::GetProcAddress_()(_dll, "xessSetVelocityScale");

                _xessD3D12GetInitParams =
                    (PFN_xessD3D12GetInitParams) KernelBaseProxy::GetProcAddress_()(_dll, "xessD3D12GetInitParams");
                _xessGetVelocityScale =
                    (PFN_xessGetVelocityScale) KernelBaseProxy::GetProcAddress_()(_dll, "xessGetVelocityScale");
                _xessForceLegacyScaleFactors = (PFN_xessForceLegacyScaleFactors) KernelBaseProxy::GetProcAddress_()(
                    _dll, "xessForceLegacyScaleFactors");
                _xessGetExposureMultiplier = (PFN_xessGetExposureMultiplier) KernelBaseProxy::GetProcAddress_()(
                    _dll, "xessGetExposureMultiplier");
                _xessGetInputResolution =
                    (PFN_xessGetInputResolution) KernelBaseProxy::GetProcAddress_()(_dll, "xessGetInputResolution");
                _xessGetIntelXeFXVersion =
                    (PFN_xessGetIntelXeFXVersion) KernelBaseProxy::GetProcAddress_()(_dll, "xessGetIntelXeFXVersion");
                _xessGetJitterScale =
                    (PFN_xessGetJitterScale) KernelBaseProxy::GetProcAddress_()(_dll, "xessGetJitterScale");
                _xessGetOptimalInputResolution = (PFN_xessGetOptimalInputResolution) KernelBaseProxy::GetProcAddress_()(
                    _dll, "xessGetOptimalInputResolution");
                _xessSetExposureMultiplier = (PFN_xessSetExposureMultiplier) KernelBaseProxy::GetProcAddress_()(
                    _dll, "xessSetExposureMultiplier");
                _xessSetJitterScale =
                    (PFN_xessSetJitterScale) KernelBaseProxy::GetProcAddress_()(_dll, "xessSetJitterScale");

                _xessD3D12GetResourcesToDump = (PFN_xessD3D12GetResourcesToDump) KernelBaseProxy::GetProcAddress_()(
                    _dll, "xessD3D12GetResourcesToDump");
                _xessD3D12GetProfilingData = (PFN_xessD3D12GetProfilingData) KernelBaseProxy::GetProcAddress_()(
                    _dll, "xessD3D12GetProfilingData");
                _xessSetContextParameterF =
                    (PFN_xessSetContextParameterF) KernelBaseProxy::GetProcAddress_()(_dll, "xessSetContextParameterF");
                _xessGetPipelineBuildStatus = (PFN_xessGetPipelineBuildStatus) KernelBaseProxy::GetProcAddress_()(
                    _dll, "xessGetPipelineBuildStatus");

                _xessVKGetRequiredInstanceExtensions =
                    (PFN_xessVKGetRequiredInstanceExtensions) KernelBaseProxy::GetProcAddress_()(
                        _dll, "xessVKGetRequiredInstanceExtensions");
                _xessVKGetRequiredDeviceExtensions =
                    (PFN_xessVKGetRequiredDeviceExtensions) KernelBaseProxy::GetProcAddress_()(
                        _dll, "xessVKGetRequiredDeviceExtensions");
                _xessVKGetRequiredDeviceFeatures =
                    (PFN_xessVKGetRequiredDeviceFeatures) KernelBaseProxy::GetProcAddress_()(
                        _dll, "xessVKGetRequiredDeviceFeatures");
                _xessVKCreateContext =
                    (PFN_xessVKCreateContext) KernelBaseProxy::GetProcAddress_()(_dll, "xessVKCreateContext");
                _xessVKBuildPipelines =
                    (PFN_xessVKBuildPipelines) KernelBaseProxy::GetProcAddress_()(_dll, "xessVKBuildPipelines");
                _xessVKInit = (PFN_xessVKInit) KernelBaseProxy::GetProcAddress_()(_dll, "xessVKInit");
                _xessVKGetInitParams =
                    (PFN_xessVKGetInitParams) KernelBaseProxy::GetProcAddress_()(_dll, "xessVKGetInitParams");
                _xessVKExecute = (PFN_xessVKExecute) KernelBaseProxy::GetProcAddress_()(_dll, "xessVKExecute");
            }

            // if libxess not loaded
            if (_xessD3D12CreateContext == nullptr)
            {
                LOG_INFO("Trying to load libxess.dll with Detours");

                _xessD3D12CreateContext =
                    (PFN_xessD3D12CreateContext) DetourFindFunction("libxess.dll", "xessD3D12CreateContext");
                _xessD3D12BuildPipelines =
                    (PFN_xessD3D12BuildPipelines) DetourFindFunction("libxess.dll", "xessD3D12BuildPipelines");
                _xessD3D12Init = (PFN_xessD3D12Init) DetourFindFunction("libxess.dll", "xessD3D12Init");
                _xessGetVersion = (PFN_xessGetVersion) DetourFindFunction("libxess.dll", "xessGetVersion");
                _xessD3D12Execute = (PFN_xessD3D12Execute) DetourFindFunction("libxess.dll", "xessD3D12Execute");
                _xessSelectNetworkModel =
                    (PFN_xessSelectNetworkModel) DetourFindFunction("libxess.dll", "xessSelectNetworkModel");
                _xessStartDump = (PFN_xessStartDump) DetourFindFunction("libxess.dll", "xessStartDump");
                _xessIsOptimalDriver =
                    (PFN_xessIsOptimalDriver) DetourFindFunction("libxess.dll", "xessIsOptimalDriver");
                _xessSetLoggingCallback =
                    (PFN_xessSetLoggingCallback) DetourFindFunction("libxess.dll", "xessSetLoggingCallback");
                _xessGetProperties = (PFN_xessGetProperties) DetourFindFunction("libxess.dll", "xessGetProperties");
                _xessDestroyContext = (PFN_xessDestroyContext) DetourFindFunction("libxess.dll", "xessDestroyContext");
                _xessSetVelocityScale =
                    (PFN_xessSetVelocityScale) DetourFindFunction("libxess.dll", "xessSetVelocityScale");
                _xessD3D12GetInitParams =
                    (PFN_xessD3D12GetInitParams) DetourFindFunction("libxess.dll", "xessD3D12GetInitParams");
                _xessGetVelocityScale =
                    (PFN_xessGetVelocityScale) DetourFindFunction("libxess.dll", "xessGetVelocityScale");
                _xessForceLegacyScaleFactors =
                    (PFN_xessForceLegacyScaleFactors) DetourFindFunction("libxess.dll", "xessForceLegacyScaleFactors");
                _xessGetExposureMultiplier =
                    (PFN_xessGetExposureMultiplier) DetourFindFunction("libxess.dll", "xessGetExposureMultiplier");
                _xessGetInputResolution =
                    (PFN_xessGetInputResolution) DetourFindFunction("libxess.dll", "xessGetInputResolution");
                _xessGetIntelXeFXVersion =
                    (PFN_xessGetIntelXeFXVersion) DetourFindFunction("libxess.dll", "xessGetIntelXeFXVersion");
                _xessGetJitterScale = (PFN_xessGetJitterScale) DetourFindFunction("libxess.dll", "xessGetJitterScale");
                _xessGetOptimalInputResolution = (PFN_xessGetOptimalInputResolution) DetourFindFunction(
                    "libxess.dll", "xessGetOptimalInputResolution");
                _xessSetExposureMultiplier =
                    (PFN_xessSetExposureMultiplier) DetourFindFunction("libxess.dll", "xessSetExposureMultiplier");
                _xessSetJitterScale = (PFN_xessSetJitterScale) DetourFindFunction("libxess.dll", "xessSetJitterScale");
                _xessD3D12GetResourcesToDump =
                    (PFN_xessD3D12GetResourcesToDump) DetourFindFunction("libxess.dll", "xessD3D12GetResourcesToDump");
                _xessD3D12GetProfilingData =
                    (PFN_xessD3D12GetProfilingData) DetourFindFunction("libxess.dll", "xessD3D12GetProfilingData");
                _xessSetContextParameterF =
                    (PFN_xessSetContextParameterF) DetourFindFunction("libxess.dll", "xessSetContextParameterF");
                _xessGetPipelineBuildStatus =
                    (PFN_xessGetPipelineBuildStatus) DetourFindFunction("libxess.dll", "xessGetPipelineBuildStatus");

                _xessVKGetRequiredInstanceExtensions = (PFN_xessVKGetRequiredInstanceExtensions) DetourFindFunction(
                    "libxess.dll", "xessVKGetRequiredInstanceExtensions");
                _xessVKGetRequiredDeviceExtensions = (PFN_xessVKGetRequiredDeviceExtensions) DetourFindFunction(
                    "libxess.dll", "xessVKGetRequiredDeviceExtensions");
                _xessVKGetRequiredDeviceFeatures = (PFN_xessVKGetRequiredDeviceFeatures) DetourFindFunction(
                    "libxess.dll", "xessVKGetRequiredDeviceFeatures");
                _xessVKCreateContext =
                    (PFN_xessVKCreateContext) DetourFindFunction("libxess.dll", "xessVKCreateContext");
                _xessVKBuildPipelines =
                    (PFN_xessVKBuildPipelines) DetourFindFunction("libxess.dll", "xessVKBuildPipelines");
                _xessVKInit = (PFN_xessVKInit) DetourFindFunction("libxess.dll", "xessVKInit");
                _xessVKGetInitParams =
                    (PFN_xessVKGetInitParams) DetourFindFunction("libxess.dll", "xessVKGetInitParams");
                _xessVKExecute = (PFN_xessVKExecute) DetourFindFunction("libxess.dll", "xessVKExecute");
                _xessVKGetResourcesToDump =
                    (PFN_xessVKGetResourcesToDump) DetourFindFunction("libxess.dll", "xessVKGetResourcesToDump");
            }
        }

        if (Config::Instance()->EnableXeSSInputs.value_or_default() && _xessD3D12CreateContext != nullptr)
        {
            // read version from file because
            // xessGetVersion cause access violation errors
            // HMODULE moduleHandle = nullptr;
            // moduleHandle = GetModuleHandle(L"libxess.dll");
            // if (moduleHandle != nullptr)
            //{
            //    auto path = DllPath(moduleHandle);
            //    _xessVersion = GetDLLVersion(path.wstring());
            //}

            // if (_xessVersion.major == 0)
            //     _xessGetVersion(&_xessVersion);

            DetourTransactionBegin();
            DetourUpdateThread(GetCurrentThread());

            if (_xessD3D12CreateContext != nullptr)
                DetourAttach(&(PVOID&) _xessD3D12CreateContext, hk_xessD3D12CreateContext);

            if (_xessD3D12BuildPipelines != nullptr)
                DetourAttach(&(PVOID&) _xessD3D12BuildPipelines, hk_xessD3D12BuildPipelines);

            if (_xessD3D12Init != nullptr)
                DetourAttach(&(PVOID&) _xessD3D12Init, hk_xessD3D12Init);

            if (_xessGetVersion != nullptr)
                DetourAttach(&(PVOID&) _xessGetVersion, hk_xessGetVersion);

            if (_xessD3D12Execute != nullptr)
                DetourAttach(&(PVOID&) _xessD3D12Execute, hk_xessD3D12Execute);

            if (_xessSelectNetworkModel != nullptr)
                DetourAttach(&(PVOID&) _xessSelectNetworkModel, hk_xessSelectNetworkModel);

            if (_xessStartDump != nullptr)
                DetourAttach(&(PVOID&) _xessStartDump, hk_xessStartDump);

            if (_xessIsOptimalDriver != nullptr)
                DetourAttach(&(PVOID&) _xessIsOptimalDriver, hk_xessIsOptimalDriver);

            if (_xessSetLoggingCallback != nullptr)
                DetourAttach(&(PVOID&) _xessSetLoggingCallback, hk_xessSetLoggingCallback);

            if (_xessGetProperties != nullptr)
                DetourAttach(&(PVOID&) _xessGetProperties, hk_xessGetProperties);

            if (_xessDestroyContext != nullptr)
                DetourAttach(&(PVOID&) _xessDestroyContext, hk_xessDestroyContext);

            if (_xessSetVelocityScale != nullptr)
                DetourAttach(&(PVOID&) _xessSetVelocityScale, hk_xessSetVelocityScale);

            if (_xessD3D12GetInitParams != nullptr)
                DetourAttach(&(PVOID&) _xessD3D12GetInitParams, hk_xessD3D12GetInitParams);

            if (_xessForceLegacyScaleFactors != nullptr)
                DetourAttach(&(PVOID&) _xessForceLegacyScaleFactors, hk_xessForceLegacyScaleFactors);

            if (_xessGetExposureMultiplier != nullptr)
                DetourAttach(&(PVOID&) _xessGetExposureMultiplier, hk_xessGetExposureMultiplier);

            if (_xessGetInputResolution != nullptr)
                DetourAttach(&(PVOID&) _xessGetInputResolution, hk_xessGetInputResolution);

            if (_xessGetIntelXeFXVersion != nullptr)
                DetourAttach(&(PVOID&) _xessGetIntelXeFXVersion, hk_xessGetIntelXeFXVersion);

            if (_xessGetJitterScale != nullptr)
                DetourAttach(&(PVOID&) _xessGetJitterScale, hk_xessGetJitterScale);

            if (_xessGetVelocityScale != nullptr)
                DetourAttach(&(PVOID&) _xessGetVelocityScale, hk_xessGetVelocityScale);

            if (_xessGetOptimalInputResolution != nullptr)
                DetourAttach(&(PVOID&) _xessGetOptimalInputResolution, hk_xessGetOptimalInputResolution);

            if (_xessSetExposureMultiplier != nullptr)
                DetourAttach(&(PVOID&) _xessSetExposureMultiplier, hk_xessSetExposureMultiplier);

            if (_xessSetJitterScale != nullptr)
                DetourAttach(&(PVOID&) _xessSetJitterScale, hk_xessSetJitterScale);

            if (_xessD3D12GetResourcesToDump != nullptr)
                DetourAttach(&(PVOID&) _xessD3D12GetResourcesToDump, hk_xessD3D12GetResourcesToDump);

            if (_xessD3D12GetProfilingData != nullptr)
                DetourAttach(&(PVOID&) _xessD3D12GetProfilingData, hk_xessD3D12GetProfilingData);

            if (_xessSetContextParameterF != nullptr)
                DetourAttach(&(PVOID&) _xessSetContextParameterF, hk_xessSetContextParameterF);

            if (_xessVKCreateContext != nullptr)
                DetourAttach(&(PVOID&) _xessVKCreateContext, hk_xessVKCreateContext);

            if (_xessVKBuildPipelines != nullptr)
                DetourAttach(&(PVOID&) _xessVKBuildPipelines, hk_xessVKBuildPipelines);

            if (_xessVKInit != nullptr)
                DetourAttach(&(PVOID&) _xessVKInit, hk_xessVKInit);

            if (_xessVKGetInitParams != nullptr)
                DetourAttach(&(PVOID&) _xessVKGetInitParams, hk_xessVKGetInitParams);

            if (_xessVKExecute != nullptr)
                DetourAttach(&(PVOID&) _xessVKExecute, hk_xessVKExecute);

            if (_xessVKGetResourcesToDump != nullptr)
                DetourAttach(&(PVOID&) _xessVKGetResourcesToDump, hk_xessVKGetResourcesToDump);

            if (_xessVKGetRequiredDeviceExtensions != nullptr)
                DetourAttach(&(PVOID&) _xessVKGetRequiredDeviceExtensions, hk_xessVKGetRequiredDeviceExtensions);

            if (_xessVKGetRequiredDeviceFeatures != nullptr)
                DetourAttach(&(PVOID&) _xessVKGetRequiredDeviceFeatures, hk_xessVKGetRequiredDeviceFeatures);

            if (_xessVKGetRequiredInstanceExtensions != nullptr)
                DetourAttach(&(PVOID&) _xessVKGetRequiredInstanceExtensions, hk_xessVKGetRequiredInstanceExtensions);

            if (_xessGetPipelineBuildStatus != nullptr)
                DetourAttach(&(PVOID&) _xessGetPipelineBuildStatus, hk_xessGetPipelineBuildStatus);

            DetourTransactionCommit();
        }

        bool loadResult = _xessD3D12CreateContext != nullptr;
        LOG_INFO("LoadResult: {}", loadResult);
        return loadResult;
    }

    static bool HookXeSSDx11(HMODULE libxessModule = nullptr)
    {
        // if dll already loaded
        if (_dlldx11 != nullptr && _xessD3D11CreateContext != nullptr)
            return true;

        spdlog::info("");

        if (libxessModule != nullptr)
            _dlldx11 = libxessModule;

        if (_dlldx11 == nullptr && Config::Instance()->XeSSDx11Library.has_value())
        {
            std::filesystem::path libPath(Config::Instance()->XeSSDx11Library.value().c_str());

            if (libPath.has_filename())
                _dlldx11 = NtdllProxy::LoadLibraryExW_Ldr(libPath.c_str(), NULL, 0);
            else
                _dlldx11 = NtdllProxy::LoadLibraryExW_Ldr((libPath / L"libxess_dx11.dll").c_str(), NULL, 0);

            if (_dlldx11 != nullptr)
            {
                LOG_INFO("libxess_dx11.dll loaded from {0}",
                         wstring_to_string(Config::Instance()->XeSSDx11Library.value()));
            }
        }

        if (_dlldx11 == nullptr)
        {
            _dlldx11 = NtdllProxy::LoadLibraryExW_Ldr(L"libxess_dx11.dll", NULL, 0);

            if (_dlldx11 != nullptr)
                LOG_INFO("libxess_dx11.dll loaded from exe folder");
        }

        {
            ScopedSkipDxgiLoadChecks skipDxgiLoadChecks {};

            if (_dlldx11 != nullptr)
            {
                _xessD3D11CreateContext =
                    (PFN_xessD3D11CreateContext) KernelBaseProxy::GetProcAddress_()(_dlldx11, "xessD3D11CreateContext");
                _xessD3D11GetInitParams =
                    (PFN_xessD3D11GetInitParams) KernelBaseProxy::GetProcAddress_()(_dlldx11, "xessD3D11GetInitParams");
                _xessD3D11Init = (PFN_xessD3D11Init) KernelBaseProxy::GetProcAddress_()(_dlldx11, "xessD3D11Init");
                _xessD3D11Execute =
                    (PFN_xessD3D11Execute) KernelBaseProxy::GetProcAddress_()(_dlldx11, "xessD3D11Execute");
                _xessDestroyContextDx11 =
                    (PFN_xessDestroyContext) KernelBaseProxy::GetProcAddress_()(_dlldx11, "xessDestroyContext");
                _xessForceLegacyScaleFactorsDx11 = (PFN_xessForceLegacyScaleFactors) KernelBaseProxy::GetProcAddress_()(
                    _dlldx11, "xessForceLegacyScaleFactors");
                _xessGetExposureMultiplierDx11 = (PFN_xessGetExposureMultiplier) KernelBaseProxy::GetProcAddress_()(
                    _dlldx11, "xessGetExposureMultiplier");
                _xessGetInputResolutionDx11 =
                    (PFN_xessGetInputResolution) KernelBaseProxy::GetProcAddress_()(_dlldx11, "xessGetInputResolution");
                _xessGetIntelXeFXVersionDx11 = (PFN_xessGetIntelXeFXVersion) KernelBaseProxy::GetProcAddress_()(
                    _dlldx11, "xessGetIntelXeFXVersion");
                _xessGetJitterScaleDx11 =
                    (PFN_xessGetJitterScale) KernelBaseProxy::GetProcAddress_()(_dlldx11, "xessGetJitterScale");
                _xessGetOptimalInputResolutionDx11 =
                    (PFN_xessGetOptimalInputResolution) KernelBaseProxy::GetProcAddress_()(
                        _dlldx11, "xessGetOptimalInputResolution");
                _xessGetPipelineBuildStatusDx11 = (PFN_xessGetPipelineBuildStatus) KernelBaseProxy::GetProcAddress_()(
                    _dlldx11, "xessGetPipelineBuildStatus");
                _xessGetPropertiesDx11 =
                    (PFN_xessGetProperties) KernelBaseProxy::GetProcAddress_()(_dlldx11, "xessGetProperties");
                _xessGetVelocityScaleDx11 =
                    (PFN_xessGetVelocityScale) KernelBaseProxy::GetProcAddress_()(_dlldx11, "xessGetVelocityScale");
                _xessGetVersionDx11 =
                    (PFN_xessGetVersion) KernelBaseProxy::GetProcAddress_()(_dlldx11, "xessGetVersion");
                _xessIsOptimalDriverDx11 =
                    (PFN_xessIsOptimalDriver) KernelBaseProxy::GetProcAddress_()(_dlldx11, "xessIsOptimalDriver");
                _xessSelectNetworkModelDx11 =
                    (PFN_xessSelectNetworkModel) KernelBaseProxy::GetProcAddress_()(_dlldx11, "xessSelectNetworkModel");
                _xessSetContextParameterFDx11 = (PFN_xessSetContextParameterF) KernelBaseProxy::GetProcAddress_()(
                    _dlldx11, "xessSetContextParameterF");
                _xessSetExposureMultiplierDx11 = (PFN_xessSetExposureMultiplier) KernelBaseProxy::GetProcAddress_()(
                    _dlldx11, "xessSetExposureMultiplier");
                _xessSetJitterScaleDx11 =
                    (PFN_xessSetJitterScale) KernelBaseProxy::GetProcAddress_()(_dlldx11, "xessSetJitterScale");
                _xessSetLoggingCallbackDx11 =
                    (PFN_xessSetLoggingCallback) KernelBaseProxy::GetProcAddress_()(_dlldx11, "xessSetLoggingCallback");
                _xessSetVelocityScaleDx11 =
                    (PFN_xessSetVelocityScale) KernelBaseProxy::GetProcAddress_()(_dlldx11, "xessSetVelocityScale");
                _xessStartDumpDx11 = (PFN_xessStartDump) KernelBaseProxy::GetProcAddress_()(_dlldx11, "xessStartDump");
            }

            if (_xessD3D11CreateContext == nullptr)
            {
                _xessD3D11CreateContext =
                    (PFN_xessD3D11CreateContext) DetourFindFunction("libxess_dx11.dll", "xessD3D11CreateContext");
                _xessD3D11Execute = (PFN_xessD3D11Execute) DetourFindFunction("libxess_dx11.dll", "xessD3D11Execute");
                _xessD3D11GetInitParams =
                    (PFN_xessD3D11GetInitParams) DetourFindFunction("libxess_dx11.dll", "xessD3D11GetInitParams");
                _xessD3D11Init = (PFN_xessD3D11Init) DetourFindFunction("libxess_dx11.dll", "xessD3D11Init");

                _xessDestroyContextDx11 =
                    (PFN_xessDestroyContext) DetourFindFunction("libxess_dx11.dll", "xessDestroyContext");
                _xessForceLegacyScaleFactorsDx11 = (PFN_xessForceLegacyScaleFactors) DetourFindFunction(
                    "libxess_dx11.dll", "xessForceLegacyScaleFactors");
                _xessGetExposureMultiplierDx11 =
                    (PFN_xessGetExposureMultiplier) DetourFindFunction("libxess_dx11.dll", "xessGetExposureMultiplier");
                _xessGetInputResolutionDx11 =
                    (PFN_xessGetInputResolution) DetourFindFunction("libxess_dx11.dll", "xessGetInputResolution");
                _xessGetIntelXeFXVersionDx11 =
                    (PFN_xessGetIntelXeFXVersion) DetourFindFunction("libxess_dx11.dll", "xessGetIntelXeFXVersion");
                _xessGetJitterScaleDx11 =
                    (PFN_xessGetJitterScale) DetourFindFunction("libxess_dx11.dll", "xessGetJitterScale");
                _xessGetOptimalInputResolutionDx11 = (PFN_xessGetOptimalInputResolution) DetourFindFunction(
                    "libxess_dx11.dll", "xessGetOptimalInputResolution");
                _xessGetPipelineBuildStatusDx11 = (PFN_xessGetPipelineBuildStatus) DetourFindFunction(
                    "libxess_dx11.dll", "xessGetPipelineBuildStatus");
                _xessGetPropertiesDx11 =
                    (PFN_xessGetProperties) DetourFindFunction("libxess_dx11.dll", "xessGetProperties");
                _xessGetVelocityScale =
                    (PFN_xessGetVelocityScale) DetourFindFunction("libxess_dx11.dll", "xessGetVelocityScale");
                _xessGetVersionDx11 =
                    (PFN_xessGetVersion) KernelBaseProxy::GetProcAddress_()(_dlldx11, "xessGetVersion");
                _xessIsOptimalDriverDx11 =
                    (PFN_xessIsOptimalDriver) DetourFindFunction("libxess_dx11.dll", "xessIsOptimalDriver");
                _xessSelectNetworkModelDx11 =
                    (PFN_xessSelectNetworkModel) DetourFindFunction("libxess_dx11.dll", "xessSelectNetworkModel");
                _xessSetContextParameterFDx11 =
                    (PFN_xessSetContextParameterF) DetourFindFunction("libxess_dx11.dll", "xessSetContextParameterF");
                _xessSetExposureMultiplierDx11 =
                    (PFN_xessSetExposureMultiplier) DetourFindFunction("libxess_dx11.dll", "xessSetExposureMultiplier");
                _xessSetJitterScaleDx11 =
                    (PFN_xessSetJitterScale) DetourFindFunction("libxess_dx11.dll", "xessSetJitterScale");
                _xessSetLoggingCallbackDx11 =
                    (PFN_xessSetLoggingCallback) DetourFindFunction("libxess_dx11.dll", "xessSetLoggingCallback");
                _xessSetVelocityScaleDx11 =
                    (PFN_xessSetVelocityScale) DetourFindFunction("libxess_dx11.dll", "xessSetVelocityScale");
                _xessStartDumpDx11 = (PFN_xessStartDump) DetourFindFunction("libxess_dx11.dll", "xessStartDump");
            }
        }

        if (Config::Instance()->EnableXeSSInputs.value_or_default() && _xessD3D11CreateContext != nullptr)
        {
            DetourTransactionBegin();
            DetourUpdateThread(GetCurrentThread());

            if (_xessD3D11CreateContext != nullptr)
                DetourAttach(&(PVOID&) _xessD3D11CreateContext, hk_xessD3D11CreateContext);

            if (_xessD3D11Execute != nullptr)
                DetourAttach(&(PVOID&) _xessD3D11Execute, hk_xessD3D11Execute);

            if (_xessD3D11GetInitParams != nullptr)
                DetourAttach(&(PVOID&) _xessD3D11GetInitParams, hk_xessD3D11GetInitParams);

            if (_xessD3D11Init != nullptr)
                DetourAttach(&(PVOID&) _xessD3D11Init, hk_xessD3D11Init);

            if (_xessDestroyContextDx11 != nullptr)
                DetourAttach(&(PVOID&) _xessDestroyContextDx11, hk_xessDestroyContext);

            if (_xessForceLegacyScaleFactorsDx11 != nullptr)
                DetourAttach(&(PVOID&) _xessForceLegacyScaleFactorsDx11, hk_xessForceLegacyScaleFactors);

            if (_xessGetExposureMultiplierDx11 != nullptr)
                DetourAttach(&(PVOID&) _xessGetExposureMultiplierDx11, hk_xessGetExposureMultiplier);

            if (_xessGetInputResolutionDx11 != nullptr)
                DetourAttach(&(PVOID&) _xessGetInputResolutionDx11, hk_xessGetInputResolution);

            if (_xessGetIntelXeFXVersionDx11 != nullptr)
                DetourAttach(&(PVOID&) _xessGetIntelXeFXVersionDx11, hk_xessGetIntelXeFXVersion);

            if (_xessGetJitterScaleDx11 != nullptr)
                DetourAttach(&(PVOID&) _xessGetJitterScaleDx11, hk_xessGetJitterScale);

            if (_xessGetOptimalInputResolutionDx11 != nullptr)
                DetourAttach(&(PVOID&) _xessGetOptimalInputResolutionDx11, hk_xessGetOptimalInputResolution);

            if (_xessGetPipelineBuildStatusDx11 != nullptr)
                DetourAttach(&(PVOID&) _xessGetPipelineBuildStatusDx11, hk_xessGetPipelineBuildStatus);

            if (_xessGetPropertiesDx11 != nullptr)
                DetourAttach(&(PVOID&) _xessGetPropertiesDx11, hk_xessGetProperties);

            if (_xessGetVelocityScaleDx11 != nullptr)
                DetourAttach(&(PVOID&) _xessGetVelocityScaleDx11, hk_xessGetVelocityScale);

            if (_xessGetVersionDx11 != nullptr)
                DetourAttach(&(PVOID&) _xessGetVersionDx11, hk_xessGetVersionDx11);

            if (_xessIsOptimalDriverDx11 != nullptr)
                DetourAttach(&(PVOID&) _xessIsOptimalDriverDx11, hk_xessIsOptimalDriver);

            if (_xessSelectNetworkModelDx11 != nullptr)
                DetourAttach(&(PVOID&) _xessSelectNetworkModelDx11, hk_xessSelectNetworkModel);

            if (_xessSetContextParameterFDx11 != nullptr)
                DetourAttach(&(PVOID&) _xessSetContextParameterFDx11, hk_xessSetContextParameterF);

            if (_xessSetExposureMultiplierDx11 != nullptr)
                DetourAttach(&(PVOID&) _xessSetExposureMultiplierDx11, hk_xessSetExposureMultiplier);

            if (_xessSetJitterScaleDx11 != nullptr)
                DetourAttach(&(PVOID&) _xessSetJitterScaleDx11, hk_xessSetJitterScale);

            if (_xessSetLoggingCallbackDx11 != nullptr)
                DetourAttach(&(PVOID&) _xessSetLoggingCallbackDx11, hk_xessSetLoggingCallback);

            if (_xessSetVelocityScaleDx11 != nullptr)
                DetourAttach(&(PVOID&) _xessSetVelocityScaleDx11, hk_xessSetVelocityScale);

            if (_xessStartDumpDx11 != nullptr)
                DetourAttach(&(PVOID&) _xessStartDumpDx11, hk_xessStartDump);

            DetourTransactionCommit();
        }

        bool loadResult = _xessD3D11CreateContext != nullptr;
        LOG_INFO("LoadResult: {}", loadResult);
        return loadResult;
    }

    static xess_version_t Version()
    {
        if (_xessVersion.major == 0 && _xessGetVersion != nullptr)
        {
            if (auto result = _xessGetVersion(&_xessVersion); result == XESS_RESULT_SUCCESS)

                LOG_INFO("XeSS Version: v{}.{}.{}", _xessVersion.major, _xessVersion.minor, _xessVersion.patch);
            else
                LOG_ERROR("Can't get XeSS version: {}", (UINT) result);
        }

        // If dll version cant be read disable 1.3.x specific stuff
        if (_xessVersion.major == 0)
        {
            _xessVersion.major = 1;
            _xessVersion.minor = 2;
            _xessVersion.patch = 0;
        }

        return _xessVersion;
    }

    static xess_version_t VersionDx11()
    {
        if (_xessVersionDx11.major == 0 && _xessGetVersionDx11 != nullptr)
            _xessGetVersionDx11(&_xessVersionDx11);

        // If dll version cant be read disable 1.3.x specific stuff
        if (_xessVersionDx11.major == 0)
        {
            _xessVersionDx11.major = 1;
            _xessVersionDx11.minor = 2;
            _xessVersionDx11.patch = 0;
        }

        return _xessVersionDx11;
    }

    static PFN_xessD3D12CreateContext D3D12CreateContext() { return _xessD3D12CreateContext; }
    static PFN_xessD3D12BuildPipelines D3D12BuildPipelines() { return _xessD3D12BuildPipelines; }
    static PFN_xessD3D12Init D3D12Init() { return _xessD3D12Init; }
    static PFN_xessD3D12Execute D3D12Execute() { return _xessD3D12Execute; }
    static PFN_xessSelectNetworkModel SelectNetworkModel() { return _xessSelectNetworkModel; }
    static PFN_xessStartDump StartDump() { return _xessStartDump; }
    static PFN_xessGetVersion GetVersion() { return _xessGetVersion; }
    static PFN_xessIsOptimalDriver IsOptimalDriver() { return _xessIsOptimalDriver; }
    static PFN_xessSetLoggingCallback SetLoggingCallback() { return _xessSetLoggingCallback; }
    static PFN_xessGetProperties GetProperties() { return _xessGetProperties; }
    static PFN_xessDestroyContext DestroyContext() { return _xessDestroyContext; }
    static PFN_xessSetVelocityScale SetVelocityScale() { return _xessSetVelocityScale; }

    static PFN_xessD3D12GetInitParams D3D12GetInitParams() { return _xessD3D12GetInitParams; }
    static PFN_xessForceLegacyScaleFactors ForceLegacyScaleFactors() { return _xessForceLegacyScaleFactors; }
    static PFN_xessGetExposureMultiplier GetExposureMultiplier() { return _xessGetExposureMultiplier; }
    static PFN_xessGetInputResolution GetInputResolution() { return _xessGetInputResolution; }
    static PFN_xessGetIntelXeFXVersion GetIntelXeFXVersion() { return _xessGetIntelXeFXVersion; }
    static PFN_xessGetJitterScale GetJitterScale() { return _xessGetJitterScale; }
    static PFN_xessGetOptimalInputResolution GetOptimalInputResolution() { return _xessGetOptimalInputResolution; }
    static PFN_xessSetExposureMultiplier SetExposureMultiplier() { return _xessSetExposureMultiplier; }
    static PFN_xessSetJitterScale SetJitterScale() { return _xessSetJitterScale; }

    static PFN_xessD3D12GetResourcesToDump D3D12GetResourcesToDump() { return _xessD3D12GetResourcesToDump; }
    static PFN_xessD3D12GetProfilingData D3D12GetProfilingData() { return _xessD3D12GetProfilingData; }
    static PFN_xessSetContextParameterF SetContextParameterF() { return _xessSetContextParameterF; }

    static PFN_xessVKGetRequiredInstanceExtensions VKGetRequiredInstanceExtensions()
    {
        return _xessVKGetRequiredInstanceExtensions;
    }
    static PFN_xessVKGetRequiredDeviceExtensions VKGetRequiredDeviceExtensions()
    {
        return _xessVKGetRequiredDeviceExtensions;
    }
    static PFN_xessVKGetRequiredDeviceFeatures VKGetRequiredDeviceFeatures()
    {
        return _xessVKGetRequiredDeviceFeatures;
    }
    static PFN_xessVKCreateContext VKCreateContext() { return _xessVKCreateContext; }
    static PFN_xessVKBuildPipelines VKBuildPipelines() { return _xessVKBuildPipelines; }
    static PFN_xessVKInit VKInit() { return _xessVKInit; }
    static PFN_xessVKGetInitParams VKGetInitParams() { return _xessVKGetInitParams; }
    static PFN_xessVKExecute VKExecute() { return _xessVKExecute; }
    static PFN_xessVKGetResourcesToDump VKGetResourcesToDump() { return _xessVKGetResourcesToDump; }

    static PFN_xessD3D11CreateContext D3D11CreateContext() { return _xessD3D11CreateContext; }
    static PFN_xessD3D11Init D3D11Init() { return _xessD3D11Init; }
    static PFN_xessD3D11GetInitParams D3D11GetInitParams() { return _xessD3D11GetInitParams; }
    static PFN_xessD3D11Execute D3D11Execute() { return _xessD3D11Execute; }
    static PFN_xessSelectNetworkModel D3D11SelectNetworkModel() { return _xessSelectNetworkModelDx11; }
    static PFN_xessStartDump D3D11StartDump() { return _xessStartDumpDx11; }
    static PFN_xessGetVersion D3D11GetVersion() { return _xessGetVersionDx11; }
    static PFN_xessIsOptimalDriver D3D11IsOptimalDriver() { return _xessIsOptimalDriverDx11; }
    static PFN_xessSetLoggingCallback D3D11SetLoggingCallback() { return _xessSetLoggingCallbackDx11; }
    static PFN_xessGetProperties D3D11GetProperties() { return _xessGetPropertiesDx11; }
    static PFN_xessDestroyContext D3D11DestroyContext() { return _xessDestroyContextDx11; }
    static PFN_xessSetVelocityScale D3D11SetVelocityScale() { return _xessSetVelocityScaleDx11; }
    static PFN_xessForceLegacyScaleFactors D3D11ForceLegacyScaleFactors() { return _xessForceLegacyScaleFactorsDx11; }
    static PFN_xessGetExposureMultiplier D3D11GetExposureMultiplier() { return _xessGetExposureMultiplierDx11; }
    static PFN_xessGetInputResolution D3D11GetInputResolution() { return _xessGetInputResolutionDx11; }
    static PFN_xessGetIntelXeFXVersion D3D11GetIntelXeFXVersion() { return _xessGetIntelXeFXVersionDx11; }
    static PFN_xessGetJitterScale D3D11GetJitterScale() { return _xessGetJitterScaleDx11; }
    static PFN_xessGetOptimalInputResolution D3D11GetOptimalInputResolution()
    {
        return _xessGetOptimalInputResolutionDx11;
    }
    static PFN_xessSetExposureMultiplier D3D11SetExposureMultiplier() { return _xessSetExposureMultiplierDx11; }
    static PFN_xessSetJitterScale D3D11SetJitterScale() { return _xessSetJitterScaleDx11; }
};
