#pragma once

#include <pch.h>
#include <proxies/NVNGX_Proxy.h>
#include <upscalers/IFeature.h>

class DLSSFeature : public virtual IFeature
{
  private:
    feature_version _version = { 0, 0, 0 };

  protected:
    NVSDK_NGX_Handle _dlssHandle = {};
    NVSDK_NGX_Handle* _p_dlssHandle = nullptr;
    inline static bool _dlssInited = false;

    void ProcessEvaluateParams(NVSDK_NGX_Parameter* InParameters);
    void ProcessInitParams(NVSDK_NGX_Parameter* InParameters);
    void ReadVersion();

    float GetSharpness(const NVSDK_NGX_Parameter* InParameters);

  public:
    feature_version Version() override { return feature_version { _version.major, _version.minor, _version.patch }; }
    std::string Name() const override { return "DLSS"; }

    DLSSFeature(unsigned int handleId, NVSDK_NGX_Parameter* InParameters);

    ~DLSSFeature();
};
