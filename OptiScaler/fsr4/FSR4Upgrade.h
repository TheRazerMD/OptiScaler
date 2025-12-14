#pragma once

#include <pch.h>
#include <Unknwn.h>
#include <filesystem>

// Forward declarations
struct AmdExtFfxApi;
struct AmdExtFfxQuery;
struct AmdExtFfxCapability2;
struct AmdExtFfxCapability;

struct magicData
{
    uint32_t values[4];
    magicData* nextMagicData;
};

typedef HRESULT(__cdecl* PFN_AmdExtD3DCreateInterface)(IUnknown* pOuter, REFIID riid, void** ppvObject);
typedef HRESULT(STDMETHODCALLTYPE* PFN_UpdateFfxApiProvider)(void* pData, uint32_t dataSizeInBytes);
typedef HRESULT(STDMETHODCALLTYPE* PFN_UpdateFfxApiProviderEx)(void* pData, uint32_t dataSizeInBytes,
                                                               magicData magicData);

void InitFSR4Update();
HRESULT STDMETHODCALLTYPE hkAmdExtD3DCreateInterface(IUnknown* pOuter, REFIID riid, void** ppvObject);

void CheckForGPU();
std::vector<std::filesystem::path> GetDriverStore();
HMODULE GetFSR4Module();

MIDL_INTERFACE("F714E11A-B54E-4E0F-ABC5-DF58B18133D1")
IAmdExtFfxCapability : public IUnknown
{
    virtual HRESULT unknown1() = 0;  // not used
    virtual HRESULT unknown2() = 0;  // not used
    virtual HRESULT unknown3() = 0;  // not used
    virtual HRESULT unknown4() = 0;  // not used
    virtual HRESULT unknown5() = 0;  // not used
    virtual HRESULT unknown6() = 0;  // not used
    virtual HRESULT unknown7() = 0;  // not used
    virtual HRESULT unknown8() = 0;  // not used
    virtual HRESULT unknown9() = 0;  // not used
    virtual HRESULT unknown10() = 0; // not used
    virtual HRESULT unknown11() = 0; // not used
    virtual HRESULT unknown12() = 0; // not used
    virtual HRESULT unknown13() = 0; // not used
    virtual HRESULT CheckWMMASupport(uint64_t* a, uint8_t* data) = 0;
};

MIDL_INTERFACE("BA019D53-CCAB-4CBD-B56A-7230ED4330AD")
IAmdExtFfxCapability2 : public IUnknown
{
  public:
    virtual HRESULT unknown1() = 0; // not used
    virtual HRESULT IsSupported(uint64_t a) = 0;
    virtual HRESULT unknown3() = 0;
};

MIDL_INTERFACE("014937EC-9288-446F-A9AC-D75A8E3A984F")
IAmdExtFfxQuery : public IUnknown
{
  public:
    virtual HRESULT queryInternal(IUnknown * pOuter, REFIID riid, void** ppvObject) = 0;
};

MIDL_INTERFACE("b58d6601-7401-4234-8180-6febfc0e484c")
IAmdExtFfxApi : public IUnknown
{
  public:
    virtual HRESULT UpdateFfxApiProvider(void* pData, uint32_t dataSizeInBytes) = 0;
};
