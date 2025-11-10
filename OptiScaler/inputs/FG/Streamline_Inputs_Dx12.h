#pragma once

#include <pch.h>
#include <sl.h>
#include <framegen/IFGFeature_Dx12.h>

class Sl_Inputs_Dx12
{
  private:
    bool infiniteDepth = false;
    // index is streamlineFrameId % BUFFER_COUNT
    std::optional<sl::Constants> slConstants[BUFFER_COUNT] {};
    sl::EngineType engineType = sl::EngineType::eCount;

    bool dispatched = false;
    std::mutex reportResourceMutex {};
    std::mutex newFrameMutex {};

    uint32_t lastConstantsFrameId = UINT32_MAX;
    uint64_t lastPresentFrameId = 0;
    uint32_t indexToFrameIdMapping[BUFFER_COUNT] {};
    Dx12Resource _hudlessResource {};
    Dx12Resource _uiResource {};

    uint64_t mvsWidth = 0;
    uint32_t mvsHeight = 0;

    // TODO: add full position
    // uint32_t interpolationTop = 0;
    // uint32_t interpolationLeft = 0;
    uint64_t interpolationWidth = 0;
    uint32_t interpolationHeight = 0;

    std::optional<sl::Constants>* getFrameData(IFGFeature_Dx12* fgOutput);
    void SetHudlessResource();
    void SetUIResource();

  public:
    bool setConstants(const sl::Constants& constants, uint32_t frameId);
    bool evaluateState(ID3D12Device* device);
    bool reportResource(const sl::ResourceTag& tag, ID3D12GraphicsCommandList* cmdBuffer, uint32_t frameId);
    void reportEngineType(sl::EngineType type) { engineType = type; };
    bool dispatchFG();
    void markPresent(uint64_t frameId);

    // TODO: some shutdown and cleanup methods
};
