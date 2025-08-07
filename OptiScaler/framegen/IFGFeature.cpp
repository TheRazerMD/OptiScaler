#include "IFGFeature.h"

#include <Config.h>

int IFGFeature::GetIndex() { return (_frameCount % BUFFER_COUNT); }

UINT64 IFGFeature::StartNewFrame()
{
    LOG_FUNC();

    _frameCount++;
    auto fIndex = GetIndex();

    _hudlessReady[fIndex] = false;
    _depthReady[fIndex] = false;
    _mvsReady[fIndex] = false;
    _uiReady[fIndex] = false;
    _distortionFieldReady[fIndex] = false;

    _waitingExecute[fIndex] = false;

    _noUi[fIndex] = true;
    _noDistortionField[fIndex] = true;
    _noHudless[fIndex] = true;

    return _frameCount;
}

bool IFGFeature::WaitingExecution() { return _waitingExecute[GetIndex()]; }
void IFGFeature::SetExecuted() { _waitingExecute[GetIndex()] = false; }

bool IFGFeature::DepthReady() { return _depthReady[GetIndex()]; }
void IFGFeature::SetDepthReady() { _depthReady[GetIndex()] = true; }

bool IFGFeature::MVsReady() { return _mvsReady[GetIndex()]; }
void IFGFeature::SetMVsReady() { _mvsReady[GetIndex()] = true; }

bool IFGFeature::UIReady() { return _uiReady[GetIndex()]; }
void IFGFeature::SetUIReady() { _uiReady[GetIndex()] = true; }
bool IFGFeature::UsingUI() { return !_noUi[GetIndex()]; }

bool IFGFeature::DistortionFieldReady() { return _distortionFieldReady[GetIndex()]; }
void IFGFeature::SetDistortionFieldReady() { _distortionFieldReady[GetIndex()] = true; }
bool IFGFeature::UsingDistortionField() { return !_noDistortionField[GetIndex()]; }

bool IFGFeature::HudlessReady() { return _hudlessReady[GetIndex()]; }
void IFGFeature::SetHudlessReady() { _hudlessReady[GetIndex()] = true; }
bool IFGFeature::UsingHudless() { return !_noHudless[GetIndex()]; }

bool IFGFeature::CheckForRealObject(std::string functionName, IUnknown* pObject, IUnknown** ppRealObject)
{
    if (streamlineRiid.Data1 == 0)
    {
        auto iidResult = IIDFromString(L"{ADEC44E2-61F0-45C3-AD9F-1B37379284FF}", &streamlineRiid);

        if (iidResult != S_OK)
            return false;
    }

    auto qResult = pObject->QueryInterface(streamlineRiid, (void**) ppRealObject);

    if (qResult == S_OK && *ppRealObject != nullptr)
    {
        LOG_INFO("{} Streamline proxy found!", functionName);
        (*ppRealObject)->Release();
        return true;
    }

    return false;
}

bool IFGFeature::IsActive() { return _isActive; }

bool IFGFeature::IsPaused() { return _targetFrame >= _frameCount; }

bool IFGFeature::IsDispatched() { return _lastDispatchedFrame == _frameCount; }

void IFGFeature::SetJitter(float x, float y)
{
    _jitterX = x;
    _jitterY = y;
}

void IFGFeature::SetMVScale(float x, float y, bool multiplyByResolution)
{
    _mvScaleX = x;
    _mvScaleY = y;
    _mvScaleMultiplyByResolution = multiplyByResolution;
}

void IFGFeature::SetCameraValues(float nearValue, float farValue, float vFov, float aspectRatio, float meterFactor)
{
    _cameraFar = farValue;
    _cameraNear = nearValue;
    _cameraVFov = vFov;
    _cameraAspectRatio = aspectRatio;
    _meterFactor = meterFactor;
}

void IFGFeature::SetCameraData(float cameraPosition[3], float cameraUp[3], float cameraRight[3], float cameraForward[3])
{
    std::memcpy(_cameraPosition, cameraPosition, 3 * sizeof(float));
    std::memcpy(_cameraUp, cameraUp, 3 * sizeof(float));
    std::memcpy(_cameraRight, cameraRight, 3 * sizeof(float));
    std::memcpy(_cameraForward, cameraForward, 3 * sizeof(float));
}

void IFGFeature::SetFrameTimeDelta(float delta) { _ftDelta = delta; }

void IFGFeature::SetReset(UINT reset) { _reset = reset; }

void IFGFeature::ResetCounters()
{
    _frameCount = 0;
    _targetFrame = 0;
}

void IFGFeature::UpdateTarget()
{
    _targetFrame = _frameCount + 10;
    LOG_DEBUG("Current frame: {} target frame: {}", _frameCount, _targetFrame);
}

UINT64 IFGFeature::FrameCount() { return _frameCount; }

UINT64 IFGFeature::LastDispatchedFrame() { return _lastDispatchedFrame; }

UINT64 IFGFeature::TargetFrame() { return _targetFrame; }
