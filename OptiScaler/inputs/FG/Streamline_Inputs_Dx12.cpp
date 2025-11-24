#include "Streamline_Inputs_Dx12.h"

#include <Config.h>

#include <resource_tracking/ResTrack_dx12.h>

#include <magic_enum.hpp>

std::optional<sl::Constants>* Sl_Inputs_Dx12::getFrameData(IFGFeature_Dx12* fgOutput)
{
    return &slConstants[fgOutput->GetIndexWillBeDispatched()];
}

void Sl_Inputs_Dx12::CheckForFrame(IFGFeature_Dx12* fg, uint32_t frameId)
{
    std::scoped_lock lock(_frameBoundaryMutex);

    if (_isFrameFinished)
    {
        fg->StartNewFrame();
        _isFrameFinished = false;

        if (frameId != 0)
            lastConstantsFrameId = frameId;

        // Reset local tracking variables for the new frame if needed
        interpolationWidth = 0;
        interpolationHeight = 0;
    }
}

bool Sl_Inputs_Dx12::setConstants(const sl::Constants& values, uint32_t frameId)
{
    auto fgOutput = reinterpret_cast<IFGFeature_Dx12*>(State::Instance().currentFG);

    if (fgOutput == nullptr)
        return false;

    CheckForFrame(fgOutput, frameId);

    auto& data = slConstants[fgOutput->GetIndex()];

    data = sl::Constants {};

    if (data.has_value())
    {
        if (values.structVersion == data.value().structVersion)
        {
            data = values;
            return true;
        }
        else if ((data.value().structVersion == sl::kStructVersion2 && values.structVersion == sl::kStructVersion1) ||
                 values.structVersion == 0)
        // Spider-Man Remastered does this funny thing of sending an invalid struct version
        {
            auto* pNext = data.value().next;
            memcpy(&data, &values, sizeof(values) - sizeof(sl::Constants::minRelativeLinearDepthObjectSeparation));
            data.value().structVersion = sl::kStructVersion2;
            data.value().next = pNext;

            return true;
        }
    }

    data.reset();

    LOG_ERROR("Wrong constant struct version");

    return false;
}

bool Sl_Inputs_Dx12::evaluateState(ID3D12Device* device)
{
    auto fgOutput = reinterpret_cast<IFGFeature_Dx12*>(State::Instance().currentFG);

    if (fgOutput == nullptr)
        return false;

    LOG_DEBUG();

    std::scoped_lock lock(_frameBoundaryMutex);

    _isFrameFinished = true;

    auto data = getFrameData(fgOutput);
    if (!data->has_value())
    {
        LOG_WARN("Called without constants being set");
        return false;
    }

    auto& slConstsRef = data->value();

    static UINT64 lastFrameCount = 0;
    static UINT64 repeatsInRow = 0;
    if (lastFrameCount == fgOutput->FrameCount())
    {
        repeatsInRow++;
    }
    else
    {
        lastFrameCount = fgOutput->FrameCount();
        repeatsInRow = 0;
    }

    if (repeatsInRow > 10 && fgOutput->IsActive())
    {
        LOG_WARN("Many frame count repeats in a row, stopping FG");
        State::Instance().FGchanged = true;
        repeatsInRow = 0;
        return false;
    }

    FG_Constants fgConstants {};

    // TODO
    fgConstants.displayWidth = 0;
    fgConstants.displayHeight = 0;

    fgConstants.flags.reset();

    // if ()
    //     fgConstants.flags |= FG_Flags::Hdr;

    if (slConstsRef.depthInverted)
        fgConstants.flags |= FG_Flags::InvertedDepth;

    if (slConstsRef.motionVectorsJittered)
        fgConstants.flags |= FG_Flags::JitteredMVs;

    if (slConstsRef.motionVectorsDilated)
        fgConstants.flags |= FG_Flags::DisplayResolutionMVs;

    if (Config::Instance()->FGAsync.value_or_default())
        fgConstants.flags |= FG_Flags::Async;

    if (infiniteDepth)
        fgConstants.flags |= FG_Flags::InfiniteDepth;

    if (Config::Instance()->FGXeFGDepthInverted.value_or_default() !=
            (slConstsRef.depthInverted == sl::Boolean::eTrue) ||
        Config::Instance()->FGXeFGJitteredMV.value_or_default() !=
            (slConstsRef.motionVectorsJittered == sl::Boolean::eTrue) ||
        Config::Instance()->FGXeFGHighResMV.value_or_default() !=
            (slConstsRef.motionVectorsDilated == sl::Boolean::eTrue))
    {
        Config::Instance()->FGXeFGDepthInverted = (slConstsRef.depthInverted == sl::Boolean::eTrue);
        Config::Instance()->FGXeFGJitteredMV = (slConstsRef.motionVectorsJittered == sl::Boolean::eTrue);
        Config::Instance()->FGXeFGHighResMV = (slConstsRef.motionVectorsDilated == sl::Boolean::eTrue);
        LOG_DEBUG("XeFG DepthInverted: {}", Config::Instance()->FGXeFGDepthInverted.value_or_default());
        LOG_DEBUG("XeFG JitteredMV: {}", Config::Instance()->FGXeFGJitteredMV.value_or_default());
        LOG_DEBUG("XeFG HighResMV: {}", Config::Instance()->FGXeFGHighResMV.value_or_default());
        Config::Instance()->SaveXeFG();
    }

    fgOutput->EvaluateState(device, fgConstants);

    auto result = dispatchFG();

    LOG_DEBUG("Dispatch FG result: {}", result);

    return result;
}

bool Sl_Inputs_Dx12::reportResource(const sl::ResourceTag& tag, ID3D12GraphicsCommandList* cmdBuffer, uint32_t frameId)
{
    auto& state = State::Instance();
    state.DLSSGLastFrame = state.FGLastFrame;

    auto fgOutput = reinterpret_cast<IFGFeature_Dx12*>(state.currentFG);

    // It's possible for only some resources to be marked ready if FGEnabled is enabled during resource tagging
    if (fgOutput == nullptr || !Config::Instance()->FGEnabled.value_or_default())
        return false;

    LOG_DEBUG("Reporting SL resource type: {} lifecycle: {} frameId: {}", tag.type,
              magic_enum::enum_name(tag.lifecycle), frameId);

    CheckForFrame(fgOutput, frameId);

    if (tag.resource->native == nullptr)
    {
        LOG_TRACE("tag.resource->native is null");
        return false;
    }

    if (!cmdBuffer && tag.lifecycle == sl::eOnlyValidNow)
        LOG_TRACE("cmdBuffer is null");

    auto d3dRes = (ID3D12Resource*) tag.resource->native;
    auto desc = d3dRes->GetDesc();

    Dx12Resource res = {};
    res.resource = d3dRes;
    res.cmdList = cmdBuffer; // Critical for eOnlyValidNow
    res.width = tag.extent ? tag.extent.width : desc.Width;
    res.height = tag.extent ? tag.extent.height : desc.Height;
    res.state = (D3D12_RESOURCE_STATES) tag.resource->state;
    res.frameIndex = fgOutput->GetIndex();
    res.validity =
        (tag.lifecycle == sl::eOnlyValidNow) ? FG_ResourceValidity::ValidNow : FG_ResourceValidity::UntilPresent;

    bool handled = true;

    // Map types
    if (tag.type == sl::kBufferTypeDepth || tag.type == sl::kBufferTypeHiResDepth ||
        tag.type == sl::kBufferTypeLinearDepth)
    {
        res.type = FG_ResourceType::Depth;
        fgOutput->SetResource(&res);
    }
    else if (tag.type == sl::kBufferTypeMotionVectors)
    {
        res.type = FG_ResourceType::Velocity;
        mvsWidth = res.width; // Track locally for dispatch logic
        mvsHeight = res.height;
        fgOutput->SetResource(&res);
    }
    else if (tag.type == sl::kBufferTypeHUDLessColor)
    {
        res.type = FG_ResourceType::HudlessColor;

        interpolationWidth = res.width; // Track for dispatch
        interpolationHeight = res.height;

        fgOutput->SetResource(&res);
    }
    else if (tag.type == sl::kBufferTypeUIColorAndAlpha)
    {
        res.type = FG_ResourceType::UIColor;

        // Fallback size logic
        if (interpolationWidth == 0)
        {
            interpolationWidth = res.width;
            interpolationHeight = res.height;
        }
        fgOutput->SetResource(&res);
    }
    else
    {
        handled = false;
    }

    return handled;
}

bool Sl_Inputs_Dx12::dispatchFG()
{
    auto fgOutput = reinterpret_cast<IFGFeature_Dx12*>(State::Instance().currentFG);
    if (fgOutput == nullptr)
        return false;

    auto data = getFrameData(fgOutput);
    if (!data->has_value())
        return false;

    auto& slConstsRef = data->value();

    if (State::Instance().FGchanged)
        return false;

    if (!fgOutput->IsActive())
        return false;

    // Nukem's function, licensed under GPLv3
    auto loadCameraMatrix = [&]()
    {
        if (data->value().orthographicProjection)
            return false;

        float projMatrix[4][4];
        memcpy(projMatrix, (void*) &slConstsRef.cameraViewToClip, sizeof(projMatrix));

        // BUG: Various RTX Remix-based games pass in an identity matrix which is completely useless. No
        // idea why.
        const bool isEmptyOrIdentityMatrix = [&]()
        {
            float m[4][4] = {};
            if (memcmp(projMatrix, m, sizeof(m)) == 0)
                return true;

            m[0][0] = m[1][1] = m[2][2] = m[3][3] = 1.0f;
            return memcmp(projMatrix, m, sizeof(m)) == 0;
        }();

        if (isEmptyOrIdentityMatrix)
            return false;

        // a 0 0 0
        // 0 b 0 0
        // 0 0 c e
        // 0 0 d 0
        const double b = projMatrix[1][1];
        const double c = projMatrix[2][2];
        const double d = projMatrix[3][2];
        const double e = projMatrix[2][3];

        if (e < 0.0)
        {
            slConstsRef.cameraNear = static_cast<float>((c == 0.0) ? 0.0 : (d / c));
            slConstsRef.cameraFar = static_cast<float>(d / (c + 1.0));
        }
        else
        {
            slConstsRef.cameraNear = static_cast<float>((c == 0.0) ? 0.0 : (-d / c));
            slConstsRef.cameraFar = static_cast<float>(-d / (c - 1.0));
        }

        if (slConstsRef.depthInverted)
            std::swap(slConstsRef.cameraNear, slConstsRef.cameraFar);

        slConstsRef.cameraFOV = static_cast<float>(2.0 * std::atan(1.0 / b));
        return true;
    };

    static bool dontRecalc = false;

    LOG_TRACE("Camera from SL pre recalc near: {}, far: {}", slConstsRef.cameraNear, slConstsRef.cameraFar);

    // UE seems to not be passing the correct cameraViewToClip
    // and we can't use it to calculate cameraNear and cameraFar.
    if (engineType != sl::EngineType::eUnreal && !dontRecalc)
        loadCameraMatrix();

    // Workaround for more games with broken cameraViewToClip
    if (!dontRecalc && (slConstsRef.cameraNear < 0.0f || slConstsRef.cameraFar < 0.0f))
        dontRecalc = true;

    infiniteDepth = false;
    if (slConstsRef.cameraNear != 0.0f && slConstsRef.cameraFar == 0.0f)
    {
        // A CameraFar value of zero indicates an infinite far plane. Due to a bug in FSR's
        // setupDeviceDepthToViewSpaceDepthParams function, CameraFar must always be greater than
        // CameraNear when in use.

        infiniteDepth = true;
        slConstsRef.cameraFar = slConstsRef.cameraNear + 1.0f;
    }

    fgOutput->SetCameraValues(slConstsRef.cameraNear, slConstsRef.cameraFar, slConstsRef.cameraFOV,
                              slConstsRef.cameraAspectRatio, 0.0f);

    fgOutput->SetJitter(slConstsRef.jitterOffset.x, slConstsRef.jitterOffset.y);

    // Streamline is not 100% clear on if we should multiply by resolution or not.
    // But UE games and Dead Rising expect that multiplication to be done, even if the scale is 1.0.
    // bool multiplyByResolution = dataCopy.mvecScale.x != 1.f || dataCopy.mvecScale.y != 1.f;
    bool multiplyByResolution = true;
    if (multiplyByResolution)
        fgOutput->SetMVScale(slConstsRef.mvecScale.x * mvsWidth, slConstsRef.mvecScale.y * mvsHeight);
    else
        fgOutput->SetMVScale(slConstsRef.mvecScale.x, slConstsRef.mvecScale.y);

    fgOutput->SetCameraData(
        reinterpret_cast<float*>(&slConstsRef.cameraPos), reinterpret_cast<float*>(&slConstsRef.cameraUp),
        reinterpret_cast<float*>(&slConstsRef.cameraRight), reinterpret_cast<float*>(&slConstsRef.cameraFwd));

    fgOutput->SetReset(slConstsRef.reset == sl::Boolean::eTrue);

    fgOutput->SetFrameTimeDelta(static_cast<float>(State::Instance().lastFGFrameTime));

    fgOutput->SetInterpolationRect(interpolationWidth, interpolationHeight);
    interpolationWidth = 0;
    interpolationHeight = 0;

    return true;
}

void Sl_Inputs_Dx12::markPresent(uint64_t frameId) { LOG_TRACE("Present Start: {}", frameId); }
