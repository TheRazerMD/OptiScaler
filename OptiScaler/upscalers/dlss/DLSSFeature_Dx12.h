#pragma once
#include "DLSSFeature.h"
#include <upscalers/IFeature_Dx12.h>
#include <shaders/rcas/RCAS_Dx12.h>
#include <string>

class DLSSFeatureDx12 : public DLSSFeature, public IFeature_Dx12
{
  private:
  protected:
  public:
    bool Init(ID3D12Device* InDevice, ID3D12GraphicsCommandList* InCommandList,
              NVSDK_NGX_Parameter* InParameters) override;
    bool Evaluate(ID3D12GraphicsCommandList* InCommandList, NVSDK_NGX_Parameter* InParameters) override;

    feature_version Version() override { return DLSSFeature::Version(); }
    std::string Name() const override { return DLSSFeature::Name(); }

    DLSSFeatureDx12(unsigned int InHandleId, NVSDK_NGX_Parameter* InParameters);
    ~DLSSFeatureDx12();
};
