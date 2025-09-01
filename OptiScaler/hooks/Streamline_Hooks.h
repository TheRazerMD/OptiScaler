#pragma once

#include "pch.h"

#include <d3d12.h>
#include <sl.h>
#include <sl1.h>
#include <sl_dlss_g.h>
#include <sl_pcl.h>
#include <sl_reflex.h>
#include "include/sl.param/parameters.h"

struct Adapter
{
    LUID id {};
    VendorId::Value vendor {};
    uint32_t bit; // in the adapter bit-mask
    uint32_t architecture {};
    uint32_t implementation {};
    uint32_t revision {};
    uint32_t deviceId {};
    void* nativeInterface {};
};

constexpr uint32_t kMaxNumSupportedGPUs = 8;

struct SystemCaps
{
    uint32_t gpuCount {};
    uint32_t osVersionMajor {};
    uint32_t osVersionMinor {};
    uint32_t osVersionBuild {};
    uint32_t driverVersionMajor {};
    uint32_t driverVersionMinor {};
    Adapter adapters[kMaxNumSupportedGPUs] {};
    uint32_t gpuLoad[kMaxNumSupportedGPUs] {}; // percentage
    bool hwsSupported {};                      // OS wide setting, not per adapter
    bool laptopDevice {};
};

struct SystemCapsSl15
{
    uint32_t gpuCount {};
    uint32_t osVersionMajor {};
    uint32_t osVersionMinor {};
    uint32_t osVersionBuild {};
    uint32_t driverVersionMajor {};
    uint32_t driverVersionMinor {};
    uint32_t architecture[kMaxNumSupportedGPUs] {};
    uint32_t implementation[kMaxNumSupportedGPUs] {};
    uint32_t revision[kMaxNumSupportedGPUs] {};
    uint32_t gpuLoad[kMaxNumSupportedGPUs] {}; // percentage
    bool hwSchedulingEnabled {};
};

class StreamlineHooks
{
  public:
    typedef void* (*PFN_slGetPluginFunction)(const char* functionName);
    typedef bool (*PFN_slOnPluginLoad)(void* params, const char* loaderJSON, const char** pluginJSON);
    typedef sl::Result (*PFN_slSetData)(const sl::BaseStructure* inputs, sl::CommandBuffer* cmdBuffer);
    typedef bool (*PFN_slSetConstants_sl1)(const void* data, uint32_t frameIndex, uint32_t id);
    typedef void (*PFN_slSetParameters_sl1)(void* params);
    typedef bool (*PFN_setVoid)(void* self, const char* key, void** value);

    static void updateForceReflex();

    static void unhookInterposer();
    static void hookInterposer(HMODULE slInterposer);

    static void unhookDlss();
    static void hookDlss(HMODULE slDlss);

    static void unhookDlssg();
    static void hookDlssg(HMODULE slDlssg);

    static void unhookReflex();
    static void hookReflex(HMODULE slReflex);

    static void unhookPcl();
    static void hookPcl(HMODULE slPcl);

    static void unhookCommon();
    static void hookCommon(HMODULE slCommon);

  private:
    static sl::RenderAPI renderApi;
    static std::mutex setConstantsMutex;

    // System caps
    static SystemCaps* systemCaps;
    static SystemCapsSl15* systemCapsSl15;
    static void hookSystemCaps(sl::param::IParameters* params);
    static uint32_t getSystemCapsArch();
    static void setArch(uint32_t arch);
    static void spoofArch(uint32_t currentArch, sl::Feature feature);

    // Interposer
    static decltype(&slInit) o_slInit;
    static decltype(&slSetTag) o_slSetTag;
    static decltype(&slSetTagForFrame) o_slSetTagForFrame;
    static decltype(&slEvaluateFeature) o_slEvaluateFeature;
    static decltype(&slAllocateResources) o_slAllocateResources;
    static decltype(&slSetConstants) o_slSetConstants;
    static decltype(&slGetNativeInterface) o_slGetNativeInterface;
    static decltype(&slSetD3DDevice) o_slSetD3DDevice;
    static decltype(&slGetNewFrameToken) o_slGetNewFrameToken;

    static decltype(&sl1::slInit) o_slInit_sl1;

    static sl::PFun_LogMessageCallback* o_logCallback;
    static sl1::pfunLogMessageCallback* o_logCallback_sl1;

    static sl::Result hkslInit(sl::Preferences* pref, uint64_t sdkVersion);
    static bool hkslInit_sl1(sl1::Preferences* pref, int applicationId);
    static sl::Result hkslSetTag(sl::ViewportHandle& viewport, sl::ResourceTag* tags, uint32_t numTags,
                                 sl::CommandBuffer* cmdBuffer);

    static sl::Result hkslSetTagForFrame(const sl::FrameToken& frame, const sl::ViewportHandle& viewport,
                                         const sl::ResourceTag* resources, uint32_t numResources,
                                         sl::CommandBuffer* cmdBuffer);

    static sl::Result hkslEvaluateFeature(sl::Feature feature, const sl::FrameToken& frame,
                                          const sl::BaseStructure** inputs, uint32_t numInputs,
                                          sl::CommandBuffer* cmdBuffer);

    static sl::Result hkslAllocateResources(sl::CommandBuffer* cmdBuffer, sl::Feature feature,
                                            const sl::ViewportHandle& viewport);

    static sl::Result hkslGetNativeInterface(void* proxyInterface, void** baseInterface);

    static sl::Result hkslSetD3DDevice(void* d3dDevice);

    // DLSS
    static PFN_slGetPluginFunction o_dlss_slGetPluginFunction;
    static PFN_slOnPluginLoad o_dlss_slOnPluginLoad;

    static bool hkdlss_slOnPluginLoad(void* params, const char* loaderJSON, const char** pluginJSON);
    static void* hkdlss_slGetPluginFunction(const char* functionName);

    // DLSSG
    static PFN_slGetPluginFunction o_dlssg_slGetPluginFunction;
    static PFN_slOnPluginLoad o_dlssg_slOnPluginLoad;
    static decltype(&slDLSSGSetOptions) o_slDLSSGSetOptions;
    static decltype(&slDLSSGGetState) o_slDLSSGGetState;

    static bool hkdlssg_slOnPluginLoad(void* params, const char* loaderJSON, const char** pluginJSON);
    static sl::Result hkslSetConstants(const sl::Constants& values, const sl::FrameToken& frame,
                                       const sl::ViewportHandle& viewport);
    static sl::Result hkslDLSSGSetOptions(const sl::ViewportHandle& viewport, const sl::DLSSGOptions& options);
    static void* hkdlssg_slGetPluginFunction(const char* functionName);

    // Reflex
    static sl::ReflexMode reflexGamesLastMode;
    static PFN_slGetPluginFunction o_reflex_slGetPluginFunction;
    static PFN_slSetConstants_sl1 o_reflex_slSetConstants_sl1;
    static PFN_slOnPluginLoad o_reflex_slOnPluginLoad;
    static decltype(&slReflexSetOptions) o_slReflexSetOptions;

    static bool hkreflex_slOnPluginLoad(void* params, const char* loaderJSON, const char** pluginJSON);
    static sl::Result hkslReflexSetOptions(const sl::ReflexOptions& options);
    static bool hkreflex_slSetConstants_sl1(const void* data, uint32_t frameIndex, uint32_t id);
    static void* hkreflex_slGetPluginFunction(const char* functionName);

    // PCL
    static PFN_slGetPluginFunction o_pcl_slGetPluginFunction;
    static PFN_slOnPluginLoad o_pcl_slOnPluginLoad;
    static decltype(&slPCLSetMarker) o_slPCLSetMarker;

    static bool hkpcl_slOnPluginLoad(void* params, const char* loaderJSON, const char** pluginJSON);
    static void* hkpcl_slGetPluginFunction(const char* functionName);
    static sl::Result hkslPCLSetMarker(sl::PCLMarker marker, const sl::FrameToken& frame);

    // Common
    static PFN_slGetPluginFunction o_common_slGetPluginFunction;
    static PFN_slOnPluginLoad o_common_slOnPluginLoad;
    static PFN_slSetParameters_sl1 o_common_slSetParameters_sl1;
    static PFN_setVoid o_setVoid;

    static bool hkcommon_slOnPluginLoad(void* params, const char* loaderJSON, const char** pluginJSON);
    static void* hkcommon_slGetPluginFunction(const char* functionName);
    static void hkcommon_slSetParameters_sl1(void* params);
    static bool hk_setVoid(void* self, const char* key, void** value);

    // Logging
    static char* trimStreamlineLog(const char* msg);
    static void streamlineLogCallback(sl::LogType type, const char* msg);
    static void streamlineLogCallback_sl1(sl1::LogType type, const char* msg);
};
