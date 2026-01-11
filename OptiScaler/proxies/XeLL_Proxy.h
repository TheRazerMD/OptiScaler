#pragma once

#include "pch.h"
#include "Util.h"
#include "Config.h"
#include "Logger.h"

#include <proxies/Ntdll_Proxy.h>
#include <proxies/KernelBase_Proxy.h>

#include <xell.h>
#include <xell_d3d12.h>

#include <magic_enum.hpp>

#pragma comment(lib, "Version.lib")

// Common
typedef decltype(&xellDestroyContext) PFN_xellDestroyContext;
typedef decltype(&xellSetSleepMode) PFN_xellSetSleepMode;
typedef decltype(&xellGetSleepMode) PFN_xellGetSleepMode;
typedef decltype(&xellSleep) PFN_xellSleep;
typedef decltype(&xellAddMarkerData) PFN_xellAddMarkerData;
typedef decltype(&xellGetVersion) PFN_xellGetVersion;
typedef decltype(&xellSetLoggingCallback) PFN_xellSetLoggingCallback;

// Dx12
typedef decltype(&xellD3D12CreateContext) PFN_xellD3D12CreateContext;

class XeLLProxy
{
  private:
    inline static HMODULE _dll = nullptr;

    inline static feature_version _xellVersion {};

    inline static xell_context_handle_t _xellContext = nullptr;

    static void xellLogCallback(const char* message, xell_logging_level_t loggingLevel)
    {
        switch (loggingLevel)
        {
        case XELL_LOGGING_LEVEL_DEBUG:
            spdlog::debug("XeLL Log: {}", message);
            return;

        case XELL_LOGGING_LEVEL_INFO:
            spdlog::info("XeLL Log: {}", message);
            return;

        case XELL_LOGGING_LEVEL_WARNING:
            spdlog::warn("XeLL Log: {}", message);
            return;

        default:
            spdlog::error("XeLL Log: {}", message);
            return;
        }
    }

    // Common
    inline static PFN_xellDestroyContext _xellDestroyContext = nullptr;
    inline static PFN_xellSetSleepMode _xellSetSleepMode = nullptr;
    inline static PFN_xellGetSleepMode _xellGetSleepMode = nullptr;
    inline static PFN_xellSleep _xellSleep = nullptr;
    inline static PFN_xellAddMarkerData _xellAddMarkerData = nullptr;
    inline static PFN_xellGetVersion _xellGetVersion = nullptr;
    inline static PFN_xellSetLoggingCallback _xellSetLoggingCallback = nullptr;

    // Dx12
    inline static PFN_xellD3D12CreateContext _xellD3D12CreateContext = nullptr;

    inline static xell_version_t GetDLLVersion(std::wstring dllPath)
    {
        // Step 1: Get the size of the version information
        DWORD handle = 0;
        DWORD versionSize = GetFileVersionInfoSizeW(dllPath.c_str(), &handle);
        xell_version_t version { 0, 0, 0 };

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

    static bool InitXeLL()
    {
        if (_dll != nullptr)
            return true;

        HMODULE mainModule = nullptr;

        do
        {
            mainModule = GetModuleHandle(L"libxell.dll");
            if (mainModule != nullptr)
            {
                _dll = mainModule;
                break;
            }

            auto dllPath = Util::DllPath();
            std::wstring libraryName = L"libxell.dll";

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
                std::filesystem::path libXeLLPath = dllPath.parent_path() / libraryName;
                LOG_INFO(L"Trying to load libxell.dll from dll path: {}", libXeLLPath.wstring());
                mainModule = NtdllProxy::LoadLibraryExW_Ldr(libXeLLPath.c_str(), NULL, 0);
            }

        } while (false);

        if (mainModule != nullptr)
            return HookXeLL(mainModule);

        return false;
    }

    static bool HookXeLL(HMODULE libxellModule)
    {
        // if dll already loaded
        if (_dll != nullptr && _xellDestroyContext != nullptr)
            return true;

        spdlog::info("");

        if (libxellModule == nullptr)
            return false;

        _dll = libxellModule;

        {
            ScopedSkipDxgiLoadChecks skipDxgiLoadChecks {};

            if (_dll != nullptr)
            {
                _xellDestroyContext =
                    (PFN_xellDestroyContext) KernelBaseProxy::GetProcAddress_()(_dll, "xellDestroyContext");
                _xellSetSleepMode = (PFN_xellSetSleepMode) KernelBaseProxy::GetProcAddress_()(_dll, "xellSetSleepMode");
                _xellGetSleepMode = (PFN_xellGetSleepMode) KernelBaseProxy::GetProcAddress_()(_dll, "xellGetSleepMode");
                _xellSleep = (PFN_xellSleep) KernelBaseProxy::GetProcAddress_()(_dll, "xellSleep");
                _xellAddMarkerData =
                    (PFN_xellAddMarkerData) KernelBaseProxy::GetProcAddress_()(_dll, "xellAddMarkerData");
                _xellGetVersion = (PFN_xellGetVersion) KernelBaseProxy::GetProcAddress_()(_dll, "xellGetVersion");
                _xellSetLoggingCallback =
                    (PFN_xellSetLoggingCallback) KernelBaseProxy::GetProcAddress_()(_dll, "xellSetLoggingCallback");

                _xellD3D12CreateContext =
                    (PFN_xellD3D12CreateContext) KernelBaseProxy::GetProcAddress_()(_dll, "xellD3D12CreateContext");
            }
        }

        bool loadResult = _xellDestroyContext != nullptr;
        LOG_INFO("LoadResult: {}", loadResult);
        return loadResult;
    }

    static feature_version Version()
    {
        if (_xellVersion.major == 0 && _xellGetVersion != nullptr)
        {
            if (auto result = _xellGetVersion((xell_version_t*) &_xellVersion); result == XESS_RESULT_SUCCESS)

                LOG_INFO("XeLL Version: v{}.{}.{}", _xellVersion.major, _xellVersion.minor, _xellVersion.patch);
            else
                LOG_ERROR("Can't get XeLL version: {}", (UINT) result);
        }

        if (_xellVersion.major == 0)
        {
            _xellVersion.major = 1;
            _xellVersion.minor = 0;
            _xellVersion.patch = 0;
        }

        return _xellVersion;
    }

    static PFN_xellDestroyContext DestroyContext() { return _xellDestroyContext; }
    static PFN_xellSetSleepMode SetSleepMode() { return _xellSetSleepMode; }
    static PFN_xellGetSleepMode GetSleepMode() { return _xellGetSleepMode; }
    static PFN_xellSleep Sleep() { return _xellSleep; }
    static PFN_xellAddMarkerData AddMarkerData() { return _xellAddMarkerData; }
    static PFN_xellGetVersion GetVersion() { return _xellGetVersion; }
    static PFN_xellSetLoggingCallback SetLoggingCallback() { return _xellSetLoggingCallback; }

    static PFN_xellD3D12CreateContext D3D12CreateContext() { return _xellD3D12CreateContext; }

    static bool DestroyXeLLContext()
    {
        LOG_DEBUG("");

        if (_xellContext != nullptr)
        {
            auto context = _xellContext;
            _xellContext = nullptr;
            auto xellResult = _xellDestroyContext(context);

            LOG_INFO("XeLL DestroyContext result: {} ({})", magic_enum::enum_name(xellResult), (UINT) xellResult);

            // Set it back because context is not destroyed
            if (xellResult != XELL_RESULT_SUCCESS)
                _xellContext = context;
        }

        return true;
    }

    static bool CreateContext(ID3D12Device* device)
    {
        if (!InitXeLL())
        {
            LOG_ERROR("XeLL proxy can't find libxell.dll!");
            return false;
        }

        if (_xellContext != nullptr)
            DestroyXeLLContext();

        xell_result_t xellResult;
        {
            ScopedSkipSpoofing skipSpoofing {};
            xellResult = _xellD3D12CreateContext(device, &_xellContext);
        }

        if (xellResult != XELL_RESULT_SUCCESS)
        {
            LOG_ERROR("XeLL D3D12CreateContext error: {} ({})", magic_enum::enum_name(xellResult), (UINT) xellResult);
            return false;
        }
        else
        {
            LOG_INFO("XeLL context created");
        }

        xellResult = SetLoggingCallback()(_xellContext, XELL_LOGGING_LEVEL_DEBUG, xellLogCallback);
        if (xellResult != XELL_RESULT_SUCCESS)
        {
            LOG_ERROR("XeLL SetLoggingCallback error: {} ({})", magic_enum::enum_name(xellResult), (UINT) xellResult);
        }

        return true;
    }

    static xell_context_handle_t Context() { return _xellContext; }
};
