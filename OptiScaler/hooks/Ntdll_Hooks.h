#pragma once

#include <pch.h>

#include <Util.h>
#include <State.h>
#include <Config.h>
#include <DllNames.h>

#include "LibraryLoad_Hooks.h"

#include <cwctype>

#pragma intrinsic(_ReturnAddress)

class NtdllHooks
{
  private:
    inline static NtdllProxy::PFN_NtLoadDll o_NtLoadDll = nullptr;
    inline static NtdllProxy::PFN_LdrLoadDll o_LdrLoadDll = nullptr;
    inline static NtdllProxy::PFN_LdrUnloadDll o_LdrUnloadDll = nullptr;

    inline static bool _overlayMethodsCalled = false;

    static NTSTATUS NTAPI hkLdrLoadDll(PWSTR PathToFile, PULONG Flags, PUNICODE_STRING ModuleFileName,
                                       PHANDLE ModuleHandle)
    {
        if (ModuleHandle == nullptr)
            return STATUS_INVALID_PARAMETER;

        if (ModuleFileName == nullptr || ModuleFileName->Length == 0)
            return o_LdrLoadDll(PathToFile, Flags, ModuleFileName, ModuleHandle);

        std::wstring_view name(ModuleFileName->Buffer, ModuleFileName->Length / sizeof(wchar_t));

#ifdef _DEBUG
        // LOG_DEBUG("{}, caller: {}", wstring_to_string(name.data()), Util::WhoIsTheCaller(_ReturnAddress()));
#endif

        if (State::Instance().isShuttingDown)
            return o_LdrLoadDll(PathToFile, Flags, ModuleFileName, ModuleHandle);

        if (LibraryLoadHooks::IsApiSetName(name))
            return o_LdrLoadDll(PathToFile, Flags, ModuleFileName, ModuleHandle);

        if (State::SkipDllChecks())
        {
            const std::wstring skip = string_to_wstring(State::SkipDllName());

            if (skip.empty() || LibraryLoadHooks::EndsWithInsensitive(name, std::wstring_view(skip)) ||
                LibraryLoadHooks::EndsWithInsensitive(name, std::wstring(skip + L".dll")))
            {
                LOG_TRACE("Skip checks for: {}", wstring_to_string(name.data()));
                return o_LdrLoadDll(PathToFile, Flags, ModuleFileName, ModuleHandle);
            }
        }

        // if (originalModule != Util::GetCallerModule(_ReturnAddress()))
        {
            auto moduleHandle = LibraryLoadHooks::LoadLibraryCheckW(name.data(), name.data());

            // skip loading of dll
            if (moduleHandle == (HMODULE) 1337)
            {
                return STATUS_DLL_NOT_FOUND;
            }

            if (moduleHandle != nullptr)
            {

                LOG_TRACE("{}, caller: {}", wstring_to_string(name.data()), Util::WhoIsTheCaller(_ReturnAddress()));
                *ModuleHandle = (HANDLE) moduleHandle;
                return (NTSTATUS) 0x00000000L;
            }
        }

        return o_LdrLoadDll(PathToFile, Flags, ModuleFileName, ModuleHandle);
    }

    static NTSTATUS NTAPI hkNtLoadDll(PUNICODE_STRING PathToFile, PULONG Flags, PUNICODE_STRING ModuleFileName,
                                      PHANDLE ModuleHandle)
    {
        if (ModuleHandle == nullptr)
            return STATUS_INVALID_PARAMETER;

        if (ModuleFileName == nullptr || ModuleFileName->Length == 0)
            return o_NtLoadDll(PathToFile, Flags, ModuleFileName, ModuleHandle);

        std::wstring_view name(ModuleFileName->Buffer, ModuleFileName->Length / sizeof(wchar_t));

#ifdef _DEBUG
        // LOG_DEBUG("{}, caller: {}", wstring_to_string(name.data()), Util::WhoIsTheCaller(_ReturnAddress()));
#endif

        if (State::Instance().isShuttingDown)
            return o_NtLoadDll(PathToFile, Flags, ModuleFileName, ModuleHandle);

        if (LibraryLoadHooks::IsApiSetName(name))
            return o_NtLoadDll(PathToFile, Flags, ModuleFileName, ModuleHandle);

        std::wstring_view path;
        if (PathToFile == nullptr || PathToFile->Length == 0)
            path = name;
        else
            path = std::wstring_view(PathToFile->Buffer, PathToFile->Length / sizeof(wchar_t));

        if (State::SkipDllChecks())
        {
            const std::wstring skip = string_to_wstring(State::SkipDllName());

            if (skip.empty() || LibraryLoadHooks::EndsWithInsensitive(name, std::wstring_view(skip)) ||
                LibraryLoadHooks::EndsWithInsensitive(name, std::wstring(skip + L".dll")))
            {
                LOG_TRACE("Skip checks for: {}", wstring_to_string(name.data()));
                return o_NtLoadDll(PathToFile, Flags, ModuleFileName, ModuleHandle);
            }
        }

        // Do not interfere with original modules calls
        if (originalModule != Util::GetCallerModule(_ReturnAddress()))
        {
            auto moduleHandle = LibraryLoadHooks::LoadLibraryCheckW(name.data(), path.data());

            // skip loading of dll
            if (moduleHandle == (HMODULE) 1337)
            {
                return STATUS_DLL_NOT_FOUND;
            }

            if (moduleHandle != nullptr)
            {
                LOG_TRACE("{}, caller: {}", wstring_to_string(name.data()), Util::WhoIsTheCaller(_ReturnAddress()));
                *ModuleHandle = (HANDLE) moduleHandle;
                return (NTSTATUS) 0x00000000L;
            }
        }

        return o_NtLoadDll(PathToFile, Flags, ModuleFileName, ModuleHandle);
    }

    static NTSTATUS NTAPI hkLdrUnloadDll(PVOID lpLibrary)
    {
        if (lpLibrary == nullptr)
            return STATUS_INVALID_PARAMETER;

#ifdef _DEBUG
        // LOG_TRACE("{:X}", (size_t) lpLibrary);
#endif

        if (!State::Instance().isShuttingDown)
        {
            auto result = LibraryLoadHooks::FreeLibrary(lpLibrary);

            if (result.has_value())
                return result.value();
        }

        return o_LdrUnloadDll(lpLibrary);
    }

  public:
    static void Hook()
    {
        LOG_DEBUG();

        if (!Config::Instance()->UseNtdllHooks.value_or_default())
            return;

        if (o_LdrLoadDll != nullptr)
            return;

        if (NtdllProxy::Module() == nullptr)
            return;

        o_NtLoadDll = NtdllProxy::Hook_NtLoadDll(hkNtLoadDll);
        o_LdrLoadDll = NtdllProxy::Hook_LdrLoadDll(hkLdrLoadDll);
        o_LdrUnloadDll = NtdllProxy::Hook_LdrUnloadDll(hkLdrUnloadDll);
    }
};
