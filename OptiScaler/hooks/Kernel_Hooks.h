#pragma once

#include <pch.h>

#include <proxies/Kernel32_Proxy.h>
#include <proxies/KernelBase_Proxy.h>

// Enables hooking of GetModuleHandle
// which might create issues, not tested very well
// #define HOOK_GET_MODULE

#ifdef HOOK_GET_MODULE
// Handle nvngx.dll calls on GetModule handle
// #define GET_MODULE_NVNGX

// Handle Opti dll calls on GetModule handle
#define GET_MODULE_DLL
#endif

class KernelHooks
{
  private:
    inline static Kernel32Proxy::PFN_FreeLibrary o_K32_FreeLibrary = nullptr;
    inline static Kernel32Proxy::PFN_LoadLibraryA o_K32_LoadLibraryA = nullptr;
    inline static Kernel32Proxy::PFN_LoadLibraryW o_K32_LoadLibraryW = nullptr;
    inline static Kernel32Proxy::PFN_LoadLibraryExA o_K32_LoadLibraryExA = nullptr;
    inline static Kernel32Proxy::PFN_LoadLibraryExW o_K32_LoadLibraryExW = nullptr;
    inline static Kernel32Proxy::PFN_GetProcAddress o_K32_GetProcAddress = nullptr;
    inline static Kernel32Proxy::PFN_GetModuleHandleA o_K32_GetModuleHandleA = nullptr;
    inline static Kernel32Proxy::PFN_GetModuleHandleExW o_K32_GetModuleHandleExW = nullptr;
    inline static Kernel32Proxy::PFN_GetFileAttributesW o_K32_GetFileAttributesW = nullptr;
    inline static Kernel32Proxy::PFN_CreateFileW o_K32_CreateFileW = nullptr;

    inline static KernelBaseProxy::PFN_FreeLibrary o_KB_FreeLibrary = nullptr;
    inline static KernelBaseProxy::PFN_LoadLibraryA o_KB_LoadLibraryA = nullptr;
    inline static KernelBaseProxy::PFN_LoadLibraryW o_KB_LoadLibraryW = nullptr;
    inline static KernelBaseProxy::PFN_LoadLibraryExA o_KB_LoadLibraryExA = nullptr;
    inline static KernelBaseProxy::PFN_LoadLibraryExW o_KB_LoadLibraryExW = nullptr;
    inline static KernelBaseProxy::PFN_GetProcAddress o_KB_GetProcAddress = nullptr;

    static constexpr HMODULE amdxc64Mark = HMODULE(0xFFFFFFFF13372137);

    static FARPROC WINAPI hk_K32_GetProcAddress(HMODULE hModule, LPCSTR lpProcName);
    static HMODULE WINAPI hk_K32_GetModuleHandleA(LPCSTR lpModuleName);
    static BOOL WINAPI hk_K32_GetModuleHandleExW(DWORD dwFlags, LPCWSTR lpModuleName, HMODULE* phModule);
    static FARPROC WINAPI hk_KB_GetProcAddress(HMODULE hModule, LPCSTR lpProcName);
    static DWORD WINAPI hk_K32_GetFileAttributesW(LPCWSTR lpFileName);
    static HANDLE WINAPI hk_K32_CreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                                            LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
                                            DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);

    static HMODULE hk_K32_LoadLibraryW(LPCWSTR lpLibFileName);
    static HMODULE hk_K32_LoadLibraryA(LPCSTR lpLibFileName);
    static HMODULE hk_K32_LoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags);
    static HMODULE hk_K32_LoadLibraryExA(LPCSTR lpLibFileName, HANDLE hFile, DWORD dwFlags);
    static HMODULE hk_KB_LoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags);
    static BOOL hk_K32_FreeLibrary(HMODULE lpLibrary);

  public:
    static void Hook()
    {
        if (o_K32_FreeLibrary != nullptr)
            return;

        LOG_DEBUG("");

        o_K32_GetProcAddress = Kernel32Proxy::Hook_GetProcAddress(hk_K32_GetProcAddress);
        o_K32_GetModuleHandleA = Kernel32Proxy::Hook_GetModuleHandleA(hk_K32_GetModuleHandleA);
        o_K32_GetModuleHandleExW = Kernel32Proxy::Hook_GetModuleHandleExW(hk_K32_GetModuleHandleExW);
        o_K32_GetFileAttributesW = Kernel32Proxy::Hook_GetFileAttributesW(hk_K32_GetFileAttributesW);
        o_K32_CreateFileW = Kernel32Proxy::Hook_CreateFileW(hk_K32_CreateFileW);

        if (!Config::Instance()->UseNtdllHooks.value_or_default())
        {
            o_K32_FreeLibrary = Kernel32Proxy::Hook_FreeLibrary(hk_K32_FreeLibrary);
            o_K32_LoadLibraryA = Kernel32Proxy::Hook_LoadLibraryA(hk_K32_LoadLibraryA);
            o_K32_LoadLibraryW = Kernel32Proxy::Hook_LoadLibraryW(hk_K32_LoadLibraryW);
            o_K32_LoadLibraryExA = Kernel32Proxy::Hook_LoadLibraryExA(hk_K32_LoadLibraryExA);
            o_K32_LoadLibraryExW = Kernel32Proxy::Hook_LoadLibraryExW(hk_K32_LoadLibraryExW);
        }
    }

    static void HookBase()
    {
        if (o_KB_GetProcAddress != nullptr)
            return;

        LOG_DEBUG("");

        o_KB_GetProcAddress = KernelBaseProxy::Hook_GetProcAddress(hk_KB_GetProcAddress);

        if (!Config::Instance()->UseNtdllHooks.value_or_default())
        {
            o_KB_LoadLibraryExW = KernelBaseProxy::Hook_LoadLibraryExW(hk_KB_LoadLibraryExW);
        }
    }
};
