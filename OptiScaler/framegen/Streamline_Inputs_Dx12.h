#pragma once

#include <pch.h>
#include <sl.h>

class Sl_Inputs_Dx12
{
  private:
    std::optional<sl::Constants> slConstants;
    bool infiniteDepth = false;
    sl::EngineType engineType = sl::EngineType::eCount;

    bool depthSent = false;
    bool hudlessSent = false;
    bool mvsSent = false;
    bool uiSent = false;
    bool uiRequired = false;
    bool distortionFieldSent = false;
    bool distortionFieldRequired = false;

    bool allRequiredSent = false;

    bool dispatched = false;

  public:
    bool setConstants(const sl::Constants& constants);
    bool evaluateState(ID3D12Device* device);
    bool reportResource(const sl::ResourceTag& tag, ID3D12GraphicsCommandList* cmdBuffer);
    void reportEngineType(sl::EngineType type) { engineType = type; };
    bool dispatchFG(ID3D12GraphicsCommandList* cmdBuffer);
    bool readyForDispatch() const { return allRequiredSent; };

    // TODO: some shutdown and cleanup methods
};
