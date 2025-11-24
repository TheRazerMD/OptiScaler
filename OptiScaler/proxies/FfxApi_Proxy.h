#pragma once

#include "pch.h"
#include "Util.h"
#include "Config.h"
#include "Logger.h"

#include <proxies/Ntdll_Proxy.h>
#include <proxies/KernelBase_Proxy.h>

#include <inputs/FfxApi_Dx12.h>
#include <inputs/FfxApi_Vk.h>

#include "ffx_api.h"
#include <detours/detours.h>

class FfxApiProxy
{
  private:
    inline static HMODULE _dllDx12 = nullptr;
    inline static HMODULE _dllDx12_SR = nullptr;
    inline static HMODULE _dllDx12_FG = nullptr;
    inline static feature_version _versionDx12 { 0, 0, 0 };
    inline static feature_version _versionDx12_SR { 0, 0, 0 };
    inline static feature_version _versionDx12_FG { 0, 0, 0 };
    inline static bool _dx12Loader = false;
    inline static bool _skipSRCreateCalls = false;
    inline static bool _skipFGCreateCalls = false;
    inline static bool _skipDestroyCalls = false;
    inline static bool _skipSRConfigureCalls = false;
    inline static bool _skipFGConfigureCalls = false;
    inline static bool _skipSRQueryCalls = false;
    inline static bool _skipFGQueryCalls = false;
    inline static bool _skipSRDispatchCalls = false;
    inline static bool _skipFGDispatchCalls = false;

    inline static PfnFfxCreateContext _D3D12_CreateContext = nullptr;
    inline static PfnFfxDestroyContext _D3D12_DestroyContext = nullptr;
    inline static PfnFfxConfigure _D3D12_Configure = nullptr;
    inline static PfnFfxQuery _D3D12_Query = nullptr;
    inline static PfnFfxDispatch _D3D12_Dispatch = nullptr;

    inline static PfnFfxCreateContext _D3D12_CreateContext_SR = nullptr;
    inline static PfnFfxDestroyContext _D3D12_DestroyContext_SR = nullptr;
    inline static PfnFfxConfigure _D3D12_Configure_SR = nullptr;
    inline static PfnFfxQuery _D3D12_Query_SR = nullptr;
    inline static PfnFfxDispatch _D3D12_Dispatch_SR = nullptr;

    inline static PfnFfxCreateContext _D3D12_CreateContext_FG = nullptr;
    inline static PfnFfxDestroyContext _D3D12_DestroyContext_FG = nullptr;
    inline static PfnFfxConfigure _D3D12_Configure_FG = nullptr;
    inline static PfnFfxQuery _D3D12_Query_FG = nullptr;
    inline static PfnFfxDispatch _D3D12_Dispatch_FG = nullptr;

    inline static HMODULE _dllVk = nullptr;
    inline static feature_version _versionVk { 0, 0, 0 };
    inline static PfnFfxCreateContext _VULKAN_CreateContext = nullptr;
    inline static PfnFfxDestroyContext _VULKAN_DestroyContext = nullptr;
    inline static PfnFfxConfigure _VULKAN_Configure = nullptr;
    inline static PfnFfxQuery _VULKAN_Query = nullptr;
    inline static PfnFfxDispatch _VULKAN_Dispatch = nullptr;

    static inline void parse_version(const char* version_str, feature_version* _version)
    {
        const char* p = version_str;

        // Skip non-digits at front
        while (*p)
        {
            if (isdigit((unsigned char) p[0]))
            {
                if (sscanf(p, "%u.%u.%u", &_version->major, &_version->minor, &_version->patch) == 3)
                    return;
            }
            ++p;
        }

        LOG_WARN("can't parse {0}", version_str);
    }

    static bool IsFGType(ffxStructType_t type) { return type >= 0x00020001u && type <= 0x00030009u; }

    // Can't directly check for type when query is used
    // might apply to FFX_API_DESC_TYPE_OVERRIDE_VERSION as well
    static bool IsFGType(ffxQueryDescHeader* header)
    {
        ffxStructType_t type = header->type;

        if (header->type == FFX_API_QUERY_DESC_TYPE_GET_VERSIONS ||
            header->type == FFX_API_QUERY_DESC_TYPE_GET_PROVIDER_VERSION)
        {
            type = header[1].type;
        }

        return IsFGType(type);
    }

    static bool IsLoader(const std::wstring& filePath)
    {
        auto size = std::filesystem::file_size(filePath);

        // < 1 MB
        return size < 1048576;
    }

  public:
    static HMODULE Dx12Module() { return _dllDx12; }
    static HMODULE Dx12Module_SR() { return _dllDx12_SR; }
    static HMODULE Dx12Module_FG() { return _dllDx12_FG; }

    static bool IsFGReady() { return (_dllDx12 && !_dx12Loader) || _dllDx12_FG != nullptr; }
    static bool IsSRReady() { return (_dllDx12 && !_dx12Loader) || _dllDx12_SR != nullptr; }

    static bool InitFfxDx12(HMODULE module = nullptr)
    {
        // if dll already loaded
        if (_dllDx12 != nullptr && _D3D12_CreateContext != nullptr)
            return true;

        spdlog::info("");

        if (module != nullptr)
        {
            _dllDx12 = module;

            wchar_t path[MAX_PATH];
            DWORD len = GetModuleFileNameW(module, path, MAX_PATH);
            std::wstring fileName(path);
            _dx12Loader = IsLoader(fileName);
        }

        if (_dllDx12 == nullptr)
        {
            // Try new api first
            std::vector<std::wstring> dllNames = { L"amd_fidelityfx_loader_dx12.dll", L"amd_fidelityfx_dx12.dll" };

            for (size_t i = 0; i < dllNames.size(); i++)
            {
                LOG_DEBUG("Trying to load {}", wstring_to_string(dllNames[i]));

                if (_dllDx12 == nullptr && Config::Instance()->FfxDx12Path.has_value())
                {
                    std::filesystem::path libPath(Config::Instance()->FfxDx12Path.value().c_str());
                    std::wstring fileName;

                    if (libPath.has_filename())
                        fileName = libPath.c_str();
                    else
                        fileName = (libPath / dllNames[i]).c_str();

                    _dllDx12 = NtdllProxy::LoadLibraryExW_Ldr(fileName.c_str(), NULL, 0);

                    if (_dllDx12 != nullptr)
                    {
                        LOG_INFO("{} loaded from {}", wstring_to_string(dllNames[i]),
                                 wstring_to_string(Config::Instance()->FfxDx12Path.value()));

                        // hacky but works for now
                        _dx12Loader = IsLoader(fileName);
                        break;
                    }
                }

                if (_dllDx12 == nullptr)
                {
                    auto filePath = (Util::DllPath().parent_path() / dllNames[i]);
                    _dllDx12 = NtdllProxy::LoadLibraryExW_Ldr(filePath.c_str(), NULL, 0);

                    if (_dllDx12 != nullptr)
                    {
                        LOG_INFO("{} loaded from exe folder", wstring_to_string(dllNames[i]));

                        // hacky but works for now
                        _dx12Loader = IsLoader(filePath.c_str());
                        break;
                    }
                }
            }
        }

        if (_dllDx12 != nullptr && _D3D12_Configure == nullptr)
        {
            _D3D12_Configure = (PfnFfxConfigure) KernelBaseProxy::GetProcAddress_()(_dllDx12, "ffxConfigure");
            _D3D12_CreateContext =
                (PfnFfxCreateContext) KernelBaseProxy::GetProcAddress_()(_dllDx12, "ffxCreateContext");
            _D3D12_DestroyContext =
                (PfnFfxDestroyContext) KernelBaseProxy::GetProcAddress_()(_dllDx12, "ffxDestroyContext");
            _D3D12_Dispatch = (PfnFfxDispatch) KernelBaseProxy::GetProcAddress_()(_dllDx12, "ffxDispatch");
            _D3D12_Query = (PfnFfxQuery) KernelBaseProxy::GetProcAddress_()(_dllDx12, "ffxQuery");

            if (Config::Instance()->EnableFfxInputs.value_or_default() && _D3D12_CreateContext != nullptr)
            {
                DetourTransactionBegin();
                DetourUpdateThread(GetCurrentThread());

                if (_D3D12_Configure != nullptr)
                    DetourAttach(&(PVOID&) _D3D12_Configure, ffxConfigure_Dx12);

                if (_D3D12_CreateContext != nullptr)
                    DetourAttach(&(PVOID&) _D3D12_CreateContext, ffxCreateContext_Dx12);

                if (_D3D12_DestroyContext != nullptr)
                    DetourAttach(&(PVOID&) _D3D12_DestroyContext, ffxDestroyContext_Dx12);

                if (_D3D12_Dispatch != nullptr)
                    DetourAttach(&(PVOID&) _D3D12_Dispatch, ffxDispatch_Dx12);

                if (_D3D12_Query != nullptr)
                    DetourAttach(&(PVOID&) _D3D12_Query, ffxQuery_Dx12);

                State::Instance().fsrHooks = true;

                DetourTransactionCommit();
            }
        }

        InitFfxDx12_SR();
        InitFfxDx12_FG();

        bool loadResult =
            _D3D12_CreateContext != nullptr || _D3D12_CreateContext_SR != nullptr || _D3D12_CreateContext_FG != nullptr;

        LOG_INFO("LoadResult: {}", loadResult);

        if (loadResult)
            VersionDx12();
        else
            _dllDx12 = nullptr;

        return loadResult;
    }

    static bool InitFfxDx12_SR(HMODULE module = nullptr)
    {
        // if dll already loaded
        if (_dllDx12_SR != nullptr && _D3D12_CreateContext_SR != nullptr)
            return true;

        spdlog::info("");

        if (module != nullptr)
            _dllDx12_SR = module;

        if (_dllDx12_SR == nullptr)
        {
            // Try new api first
            std::vector<std::wstring> dllNames = { L"amd_fidelityfx_upscaler_dx12.dll" };

            for (size_t i = 0; i < dllNames.size(); i++)
            {
                LOG_DEBUG("Trying to load {}", wstring_to_string(dllNames[i]));

                if (_dllDx12_SR == nullptr && Config::Instance()->FfxDx12Path.has_value())
                {
                    std::filesystem::path libPath(Config::Instance()->FfxDx12Path.value().c_str());

                    if (libPath.has_filename())
                        _dllDx12_SR = NtdllProxy::LoadLibraryExW_Ldr(libPath.c_str(), NULL, 0);
                    else
                        _dllDx12_SR = NtdllProxy::LoadLibraryExW_Ldr((libPath / dllNames[i]).c_str(), NULL, 0);

                    if (_dllDx12_SR != nullptr)
                    {
                        LOG_INFO("{} loaded from {}", wstring_to_string(dllNames[i]),
                                 wstring_to_string(Config::Instance()->FfxDx12Path.value()));
                        break;
                    }
                }

                if (_dllDx12_SR == nullptr)
                {
                    _dllDx12_SR = NtdllProxy::LoadLibraryExW_Ldr(dllNames[i].c_str(), NULL, 0);

                    if (_dllDx12_SR != nullptr)
                    {
                        LOG_INFO("{} loaded from exe folder", wstring_to_string(dllNames[i]));
                        break;
                    }
                }
            }
        }

        if (_dllDx12_SR != nullptr && _D3D12_Configure_SR == nullptr)
        {
            _D3D12_Configure_SR = (PfnFfxConfigure) KernelBaseProxy::GetProcAddress_()(_dllDx12_SR, "ffxConfigure");
            _D3D12_CreateContext_SR =
                (PfnFfxCreateContext) KernelBaseProxy::GetProcAddress_()(_dllDx12_SR, "ffxCreateContext");
            _D3D12_DestroyContext_SR =
                (PfnFfxDestroyContext) KernelBaseProxy::GetProcAddress_()(_dllDx12_SR, "ffxDestroyContext");
            _D3D12_Dispatch_SR = (PfnFfxDispatch) KernelBaseProxy::GetProcAddress_()(_dllDx12_SR, "ffxDispatch");
            _D3D12_Query_SR = (PfnFfxQuery) KernelBaseProxy::GetProcAddress_()(_dllDx12_SR, "ffxQuery");

            if (Config::Instance()->EnableFfxInputs.value_or_default() && _D3D12_CreateContext_SR != nullptr)
            {
                DetourTransactionBegin();
                DetourUpdateThread(GetCurrentThread());

                if (_D3D12_Configure_SR != nullptr)
                    DetourAttach(&(PVOID&) _D3D12_Configure_SR, ffxConfigure_Dx12);

                if (_D3D12_CreateContext_SR != nullptr)
                    DetourAttach(&(PVOID&) _D3D12_CreateContext_SR, ffxCreateContext_Dx12);

                if (_D3D12_DestroyContext_SR != nullptr)
                    DetourAttach(&(PVOID&) _D3D12_DestroyContext_SR, ffxDestroyContext_Dx12);

                if (_D3D12_Dispatch_SR != nullptr)
                    DetourAttach(&(PVOID&) _D3D12_Dispatch_SR, ffxDispatch_Dx12);

                if (_D3D12_Query_SR != nullptr)
                    DetourAttach(&(PVOID&) _D3D12_Query_SR, ffxQuery_Dx12);

                State::Instance().fsrHooks = true;

                DetourTransactionCommit();
            }
        }

        bool loadResult = _D3D12_CreateContext_SR != nullptr;

        LOG_INFO("LoadResult: {}", loadResult);

        if (loadResult)
            VersionDx12_SR();
        else
            _dllDx12_SR = nullptr;

        return loadResult;
    }

    static bool InitFfxDx12_FG(HMODULE module = nullptr)
    {
        // if dll already loaded
        if (_dllDx12_FG != nullptr && _D3D12_CreateContext_FG != nullptr)
            return true;

        spdlog::info("");

        if (module != nullptr)
            _dllDx12_FG = module;

        if (_dllDx12_FG == nullptr)
        {
            // Try new api first
            std::vector<std::wstring> dllNames = { L"amd_fidelityfx_framegeneration_dx12.dll" };

            for (size_t i = 0; i < dllNames.size(); i++)
            {
                LOG_DEBUG("Trying to load {}", wstring_to_string(dllNames[i]));

                if (_dllDx12_FG == nullptr && Config::Instance()->FfxDx12Path.has_value())
                {
                    std::filesystem::path libPath(Config::Instance()->FfxDx12Path.value().c_str());

                    if (libPath.has_filename())
                        _dllDx12_FG = NtdllProxy::LoadLibraryExW_Ldr(libPath.c_str(), NULL, 0);
                    else
                        _dllDx12_FG = NtdllProxy::LoadLibraryExW_Ldr((libPath / dllNames[i]).c_str(), NULL, 0);

                    if (_dllDx12_FG != nullptr)
                    {
                        LOG_INFO("{} loaded from {}", wstring_to_string(dllNames[i]),
                                 wstring_to_string(Config::Instance()->FfxDx12Path.value()));
                        break;
                    }
                }

                if (_dllDx12_FG == nullptr)
                {
                    _dllDx12_FG = NtdllProxy::LoadLibraryExW_Ldr(dllNames[i].c_str(), NULL, 0);

                    if (_dllDx12_FG != nullptr)
                    {
                        LOG_INFO("{} loaded from exe folder", wstring_to_string(dllNames[i]));
                        break;
                    }
                }
            }
        }

        if (_dllDx12_FG != nullptr && _D3D12_Configure_FG == nullptr)
        {
            _D3D12_Configure_FG = (PfnFfxConfigure) KernelBaseProxy::GetProcAddress_()(_dllDx12_FG, "ffxConfigure");
            _D3D12_CreateContext_FG =
                (PfnFfxCreateContext) KernelBaseProxy::GetProcAddress_()(_dllDx12_FG, "ffxCreateContext");
            _D3D12_DestroyContext_FG =
                (PfnFfxDestroyContext) KernelBaseProxy::GetProcAddress_()(_dllDx12_FG, "ffxDestroyContext");
            _D3D12_Dispatch_FG = (PfnFfxDispatch) KernelBaseProxy::GetProcAddress_()(_dllDx12_FG, "ffxDispatch");
            _D3D12_Query_FG = (PfnFfxQuery) KernelBaseProxy::GetProcAddress_()(_dllDx12_FG, "ffxQuery");

            if (Config::Instance()->EnableFfxInputs.value_or_default() && _D3D12_CreateContext_FG != nullptr)
            {
                DetourTransactionBegin();
                DetourUpdateThread(GetCurrentThread());

                if (_D3D12_Configure_FG != nullptr)
                    DetourAttach(&(PVOID&) _D3D12_Configure_FG, ffxConfigure_Dx12);

                if (_D3D12_CreateContext_FG != nullptr)
                    DetourAttach(&(PVOID&) _D3D12_CreateContext_FG, ffxCreateContext_Dx12);

                if (_D3D12_DestroyContext_FG != nullptr)
                    DetourAttach(&(PVOID&) _D3D12_DestroyContext_FG, ffxDestroyContext_Dx12);

                if (_D3D12_Dispatch_FG != nullptr)
                    DetourAttach(&(PVOID&) _D3D12_Dispatch_FG, ffxDispatch_Dx12);

                if (_D3D12_Query_FG != nullptr)
                    DetourAttach(&(PVOID&) _D3D12_Query_FG, ffxQuery_Dx12);

                State::Instance().fsrHooks = true;

                DetourTransactionCommit();
            }
        }

        bool loadResult = _D3D12_CreateContext_FG != nullptr;

        LOG_INFO("LoadResult: {}", loadResult);

        if (loadResult)
            VersionDx12_FG();
        else
            _dllDx12_FG = nullptr;

        return loadResult;
    }

    static feature_version VersionDx12()
    {
        if (_versionDx12.major == 0 && _D3D12_Query != nullptr /* && device != nullptr*/)
        {
            ffxQueryDescGetVersions versionQuery {};
            versionQuery.header.type = FFX_API_QUERY_DESC_TYPE_GET_VERSIONS;
            versionQuery.createDescType = 0x00010000u; // FFX_API_CREATE_CONTEXT_DESC_TYPE_UPSCALE
            uint64_t versionCount = 0;
            versionQuery.outputCount = &versionCount;

            auto queryResult = _D3D12_Query(nullptr, &versionQuery.header);

            // get number of versions for allocation
            if (versionCount > 0 && queryResult == FFX_API_RETURN_OK)
            {

                std::vector<uint64_t> versionIds;
                std::vector<const char*> versionNames;
                versionIds.resize(versionCount);
                versionNames.resize(versionCount);
                versionQuery.versionIds = versionIds.data();
                versionQuery.versionNames = versionNames.data();

                // fill version ids and names arrays.
                queryResult = _D3D12_Query(nullptr, &versionQuery.header);

                if (queryResult == FFX_API_RETURN_OK)
                {
                    parse_version(versionNames[0], &_versionDx12);
                    LOG_INFO("FfxApi Dx12 version: {}.{}.{}", _versionDx12.major, _versionDx12.minor,
                             _versionDx12.patch);
                }
                else
                {
                    LOG_WARN("_D3D12_Query 2 result: {}", (UINT) queryResult);
                }
            }
            else
            {
                LOG_WARN("_D3D12_Query result: {}", (UINT) queryResult);
            }
        }

        if (_versionDx12.major == 0 && _D3D12_Query_SR != nullptr)
            _versionDx12 = VersionDx12_SR();

        if (_versionDx12.major == 0 && _D3D12_Query_FG != nullptr)
            _versionDx12 = VersionDx12_FG();

        return _versionDx12;
    }

    static feature_version VersionDx12_SR()
    {
        if (_D3D12_Query_SR == nullptr)
            return VersionDx12();

        if (_versionDx12_SR.major == 0)
        {
            ffxQueryDescGetVersions versionQuery {};
            versionQuery.header.type = FFX_API_QUERY_DESC_TYPE_GET_VERSIONS;
            versionQuery.createDescType = 0x00010000u; // FFX_API_CREATE_CONTEXT_DESC_TYPE_UPSCALE
            uint64_t versionCount = 0;
            versionQuery.outputCount = &versionCount;

            auto queryResult = _D3D12_Query_SR(nullptr, &versionQuery.header);

            // get number of versions for allocation
            if (versionCount > 0 && queryResult == FFX_API_RETURN_OK)
            {

                std::vector<uint64_t> versionIds;
                std::vector<const char*> versionNames;
                versionIds.resize(versionCount);
                versionNames.resize(versionCount);
                versionQuery.versionIds = versionIds.data();
                versionQuery.versionNames = versionNames.data();

                // fill version ids and names arrays.
                queryResult = _D3D12_Query_SR(nullptr, &versionQuery.header);

                if (queryResult == FFX_API_RETURN_OK)
                {
                    parse_version(versionNames[0], &_versionDx12_SR);
                    LOG_INFO("FfxApi Dx12 SR version: {}.{}.{}", _versionDx12_SR.major, _versionDx12_SR.minor,
                             _versionDx12_SR.patch);
                }
                else
                {
                    LOG_WARN("_D3D12_Query 2 result: {}", (UINT) queryResult);
                }
            }
            else
            {
                LOG_WARN("_D3D12_Query result: {}", (UINT) queryResult);
            }
        }

        return _versionDx12_SR;
    }

    static feature_version VersionDx12_FG()
    {
        if (_D3D12_Query_FG == nullptr)
            return VersionDx12();

        if (_versionDx12_FG.major == 0)
        {
            ffxQueryDescGetVersions versionQuery {};
            versionQuery.header.type = FFX_API_QUERY_DESC_TYPE_GET_VERSIONS;
            versionQuery.createDescType = 0x00020001u; // FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATION
            uint64_t versionCount = 0;
            versionQuery.outputCount = &versionCount;

            auto queryResult = _D3D12_Query_FG(nullptr, &versionQuery.header);

            // get number of versions for allocation
            if (versionCount > 0 && queryResult == FFX_API_RETURN_OK)
            {

                std::vector<uint64_t> versionIds;
                std::vector<const char*> versionNames;
                versionIds.resize(versionCount);
                versionNames.resize(versionCount);
                versionQuery.versionIds = versionIds.data();
                versionQuery.versionNames = versionNames.data();

                // fill version ids and names arrays.
                queryResult = _D3D12_Query_FG(nullptr, &versionQuery.header);

                if (queryResult == FFX_API_RETURN_OK)
                {
                    parse_version(versionNames[0], &_versionDx12_FG);
                    LOG_INFO("FfxApi Dx12 FG version: {}.{}.{}", _versionDx12_FG.major, _versionDx12_FG.minor,
                             _versionDx12_FG.patch);
                }
                else
                {
                    LOG_WARN("_D3D12_Query 2 result: {}", (UINT) queryResult);
                }
            }
            else
            {
                LOG_WARN("_D3D12_Query result: {}", (UINT) queryResult);
            }
        }

        return _versionDx12_FG;
    }

    static ffxReturnCode_t D3D12_CreateContext(ffxContext* context, ffxCreateContextDescHeader* desc,
                                               const ffxAllocationCallbacks* memCb)
    {
        auto isFg = IsFGType(desc->type);

        if (isFg && _dllDx12_FG != nullptr)
            return _D3D12_CreateContext_FG(context, desc, memCb);
        else if (!isFg && _dllDx12_SR != nullptr)
            return _D3D12_CreateContext_SR(context, desc, memCb);

        if (_dllDx12 != nullptr && !(isFg && _skipFGCreateCalls) && !(!isFg && _skipSRCreateCalls))
        {
            if (isFg)
                _skipFGCreateCalls = true;
            else
                _skipSRCreateCalls = true;

            auto result = _D3D12_CreateContext(context, desc, memCb);

            if (isFg)
                _skipFGCreateCalls = false;
            else
                _skipSRCreateCalls = false;

            return result;
        }

        return FFX_API_RETURN_NO_PROVIDER;
    }

    static ffxReturnCode_t D3D12_DestroyContext(ffxContext* context, const ffxAllocationCallbacks* memCb)
    {
        ffxReturnCode_t result = FFX_API_RETURN_ERROR;

        if (_dllDx12 != nullptr && !_skipDestroyCalls)
        {
            _skipDestroyCalls = true;
            result = _D3D12_DestroyContext(context, memCb);
            _skipDestroyCalls = false;
        }

        if (result == FFX_API_RETURN_OK)
            return result;

        if (_dllDx12_SR != nullptr)
            result = _D3D12_DestroyContext_SR(context, memCb);

        if (result == FFX_API_RETURN_OK)
            return result;

        if (_dllDx12_FG != nullptr)
            result = _D3D12_DestroyContext_FG(context, memCb);

        if (result == FFX_API_RETURN_OK)
            return result;

        return FFX_API_RETURN_NO_PROVIDER;
    }

    static ffxReturnCode_t D3D12_Configure(ffxContext* context, const ffxConfigureDescHeader* desc)
    {
        auto isFg = IsFGType(desc->type);

        if (isFg && _dllDx12_FG != nullptr)
            return _D3D12_Configure_FG(context, desc);
        else if (!isFg && _dllDx12_SR != nullptr)
            return _D3D12_Configure_SR(context, desc);

        if (_dllDx12 != nullptr && !(isFg && _skipFGConfigureCalls) && !(!isFg && _skipSRConfigureCalls))
        {
            if (isFg)
                _skipFGConfigureCalls = true;
            else
                _skipSRConfigureCalls = true;

            auto result = _D3D12_Configure(context, desc);

            if (isFg)
                _skipFGConfigureCalls = false;
            else
                _skipSRConfigureCalls = false;

            return result;
        }

        return FFX_API_RETURN_NO_PROVIDER;
    }

    static ffxReturnCode_t D3D12_Query(ffxContext* context, ffxQueryDescHeader* desc)
    {
        auto isFg = IsFGType(desc);

        if (isFg && _dllDx12_FG != nullptr)
            return _D3D12_Query_FG(context, desc);
        else if (!isFg && _dllDx12_SR != nullptr)
            return _D3D12_Query_SR(context, desc);

        if (_dllDx12 != nullptr && !(isFg && _skipFGQueryCalls) && !(!isFg && _skipSRQueryCalls))
        {
            if (isFg)
                _skipFGQueryCalls = true;
            else
                _skipSRQueryCalls = true;

            auto result = _D3D12_Query(context, desc);

            if (isFg)
                _skipFGQueryCalls = false;
            else
                _skipSRQueryCalls = false;

            return result;
        }

        return FFX_API_RETURN_NO_PROVIDER;
    }

    static ffxReturnCode_t D3D12_Dispatch(ffxContext* context, const ffxDispatchDescHeader* desc)
    {
        auto isFg = IsFGType(desc->type);

        if (isFg && _dllDx12_FG != nullptr)
            return _D3D12_Dispatch_FG(context, desc);
        else if (!isFg && _dllDx12_SR != nullptr)
            return _D3D12_Dispatch_SR(context, desc);

        if (_dllDx12 != nullptr && !(isFg && _skipFGDispatchCalls) && !(!isFg && _skipSRDispatchCalls))
        {
            if (isFg)
                _skipFGDispatchCalls = true;
            else
                _skipSRDispatchCalls = true;

            auto result = _D3D12_Dispatch(context, desc);

            if (isFg)
                _skipFGDispatchCalls = false;
            else
                _skipSRDispatchCalls = false;

            return result;
        }

        return FFX_API_RETURN_NO_PROVIDER;
    }

    static HMODULE VkModule() { return _dllVk; }

    static bool InitFfxVk(HMODULE module = nullptr)
    {
        // if dll already loaded
        if (_dllVk != nullptr && _VULKAN_CreateContext != nullptr)
            return true;

        spdlog::info("");

        LOG_DEBUG("Loading amd_fidelityfx_vk.dll methods");

        if (module != nullptr)
            _dllVk = module;

        if (_dllVk == nullptr && Config::Instance()->FfxVkPath.has_value())
        {
            std::filesystem::path libPath(Config::Instance()->FfxVkPath.value().c_str());

            if (libPath.has_filename())
                _dllVk = NtdllProxy::LoadLibraryExW_Ldr(libPath.c_str(), NULL, 0);
            else
                _dllVk = NtdllProxy::LoadLibraryExW_Ldr((libPath / L"amd_fidelityfx_vk.dll").c_str(), NULL, 0);

            if (_dllVk != nullptr)
            {
                LOG_INFO("amd_fidelityfx_vk.dll loaded from {0}",
                         wstring_to_string(Config::Instance()->FfxVkPath.value()));
            }
        }

        if (_dllVk == nullptr)
        {
            _dllVk = NtdllProxy::LoadLibraryExW_Ldr(L"amd_fidelityfx_vk.dll", NULL, 0);

            if (_dllVk != nullptr)
                LOG_INFO("amd_fidelityfx_vk.dll loaded from exe folder");
        }

        if (_dllVk != nullptr && _VULKAN_CreateContext == nullptr)
        {
            _VULKAN_Configure = (PfnFfxConfigure) KernelBaseProxy::GetProcAddress_()(_dllVk, "ffxConfigure");
            _VULKAN_CreateContext =
                (PfnFfxCreateContext) KernelBaseProxy::GetProcAddress_()(_dllVk, "ffxCreateContext");
            _VULKAN_DestroyContext =
                (PfnFfxDestroyContext) KernelBaseProxy::GetProcAddress_()(_dllVk, "ffxDestroyContext");
            _VULKAN_Dispatch = (PfnFfxDispatch) KernelBaseProxy::GetProcAddress_()(_dllVk, "ffxDispatch");
            _VULKAN_Query = (PfnFfxQuery) KernelBaseProxy::GetProcAddress_()(_dllVk, "ffxQuery");

            if (Config::Instance()->EnableFfxInputs.value_or_default() && _VULKAN_CreateContext != nullptr)
            {
                DetourTransactionBegin();
                DetourUpdateThread(GetCurrentThread());

                if (_VULKAN_Configure != nullptr)
                    DetourAttach(&(PVOID&) _VULKAN_Configure, ffxConfigure_Vk);

                if (_VULKAN_CreateContext != nullptr)
                    DetourAttach(&(PVOID&) _VULKAN_CreateContext, ffxCreateContext_Vk);

                if (_VULKAN_DestroyContext != nullptr)
                    DetourAttach(&(PVOID&) _VULKAN_DestroyContext, ffxDestroyContext_Vk);

                if (_VULKAN_Dispatch != nullptr)
                    DetourAttach(&(PVOID&) _VULKAN_Dispatch, ffxDispatch_Vk);

                if (_VULKAN_Query != nullptr)
                    DetourAttach(&(PVOID&) _VULKAN_Query, ffxQuery_Vk);

                State::Instance().fsrHooks = true;

                DetourTransactionCommit();
            }
        }

        bool loadResult = _VULKAN_CreateContext != nullptr;

        LOG_INFO("LoadResult: {}", loadResult);

        if (loadResult)
            VersionVk();
        else
            _dllVk = nullptr;

        return loadResult;
    }

    static feature_version VersionVk()
    {
        if (_versionVk.major == 0 && _VULKAN_Query != nullptr)
        {
            ffxQueryDescGetVersions versionQuery {};
            versionQuery.header.type = FFX_API_QUERY_DESC_TYPE_GET_VERSIONS;
            versionQuery.createDescType = 0x00010000u; // FFX_API_CREATE_CONTEXT_DESC_TYPE_UPSCALE
            uint64_t versionCount = 0;
            versionQuery.outputCount = &versionCount;

            auto queryResult = _VULKAN_Query(nullptr, &versionQuery.header);

            // get number of versions for allocation
            if (versionCount > 0 && queryResult == FFX_API_RETURN_OK)
            {

                std::vector<uint64_t> versionIds;
                std::vector<const char*> versionNames;
                versionIds.resize(versionCount);
                versionNames.resize(versionCount);
                versionQuery.versionIds = versionIds.data();
                versionQuery.versionNames = versionNames.data();

                queryResult = _VULKAN_Query(nullptr, &versionQuery.header);

                if (queryResult == FFX_API_RETURN_OK)
                {
                    parse_version(versionNames[0], &_versionVk);
                    LOG_INFO("FfxApi Vulkan version: {}.{}.{}", _versionVk.major, _versionVk.minor, _versionVk.patch);
                }
                else
                {
                    LOG_WARN("_VULKAN_Query 2 result: {}", (UINT) queryResult);
                }
            }
            else
            {
                LOG_WARN("_VULKAN_Query result: {}", (UINT) queryResult);
            }
        }

        return _versionVk;
    }

    static PfnFfxCreateContext VULKAN_CreateContext() { return _VULKAN_CreateContext; }
    static PfnFfxDestroyContext VULKAN_DestroyContext() { return _VULKAN_DestroyContext; }
    static PfnFfxConfigure VULKAN_Configure() { return _VULKAN_Configure; }
    static PfnFfxQuery VULKAN_Query() { return _VULKAN_Query; }
    static PfnFfxDispatch VULKAN_Dispatch() { return _VULKAN_Dispatch; }

    static std::string ReturnCodeToString(ffxReturnCode_t result)
    {
        switch (result)
        {
        case FFX_API_RETURN_OK:
            return "The oparation was successful.";
        case FFX_API_RETURN_ERROR:
            return "An error occurred that is not further specified.";
        case FFX_API_RETURN_ERROR_UNKNOWN_DESCTYPE:
            return "The structure type given was not recognized for the function or context with which it was used. "
                   "This is likely a programming error.";
        case FFX_API_RETURN_ERROR_RUNTIME_ERROR:
            return "The underlying runtime (e.g. D3D12, Vulkan) or effect returned an error code.";
        case FFX_API_RETURN_NO_PROVIDER:
            return "No provider was found for the given structure type. This is likely a programming error.";
        case FFX_API_RETURN_ERROR_MEMORY:
            return "A memory allocation failed.";
        case FFX_API_RETURN_ERROR_PARAMETER:
            return "A parameter was invalid, e.g. a null pointer, empty resource or out-of-bounds enum value.";
        default:
            return "Unknown";
        }
    }
};
