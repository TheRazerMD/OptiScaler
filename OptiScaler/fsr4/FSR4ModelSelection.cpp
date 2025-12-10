#include "FSR4ModelSelection.h"
#include <scanner/scanner.h>
#include <detours/detours.h>

PFN_getModelBlob FSR4ModelSelection::o_getModelBlob = nullptr;
PFN_createModel FSR4ModelSelection::o_createModel = nullptr;

uint32_t getCorrectedPreset(uint32_t preset)
{
    auto correctedPreset = preset;
    // Fixup for Quality preset sometimes using model 0, sometimes using model 1
    if (State::Instance().currentFeature)
    {
        auto target = State::Instance().currentFeature->TargetWidth();
        auto render = State::Instance().currentFeature->RenderWidth();

        auto ratio = (float) target / (float) render;

        // Include Ultra Quality in the fix as well
        if (preset == 0 && ratio >= 1.29f)
            correctedPreset = 1;
    }

    if (Config::Instance()->Fsr4Model.has_value())
    {
        correctedPreset = Config::Instance()->Fsr4Model.value();
    }

    State::Instance().currentFsr4Model = correctedPreset;

    return correctedPreset;
}

uint64_t FSR4ModelSelection::hkgetModelBlob(uint32_t preset, uint64_t unknown, uint64_t* source, uint64_t* size)
{
    LOG_FUNC();

    preset = getCorrectedPreset(preset);

    auto result = o_getModelBlob(preset, unknown, source, size);

    return result;
}

uint64_t FSR4ModelSelection::hkcreateModel(void* context, uint32_t preset)
{
    LOG_FUNC();

    preset = getCorrectedPreset(preset);

    auto result = o_createModel(context, preset);

    return result;
}

void FSR4ModelSelection::Hook(HMODULE module, bool unhookOld)
{
    if (module == nullptr)
        return;

    if (o_getModelBlob != nullptr && unhookOld)
    {
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());

        DetourDetach(&(PVOID&) o_getModelBlob, hkgetModelBlob);
        o_getModelBlob = nullptr;

        DetourTransactionCommit();
    }

    if (o_getModelBlob == nullptr && o_createModel == nullptr)
    {
        const char* pattern = "83 F9 05 0F 87";
        o_getModelBlob = (PFN_getModelBlob) scanner::GetAddress(module, pattern);

        if (o_getModelBlob)
        {
            LOG_DEBUG("Hooking model selection");

            DetourTransactionBegin();
            DetourUpdateThread(GetCurrentThread());

            DetourAttach(&(PVOID&) o_getModelBlob, hkgetModelBlob);

            DetourTransactionCommit();
        }
        else
        {
            // From amd_fidelityfx_upscaler_dx12 4.0.3.604
            const char* pattern =
                "48 89 5C 24 ? 55 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 ? ? ? ? B8 ? ? ? ? E8 ? ? ? ? 48 2B E0 0F "
                "29 B4 24 ? ? ? ? 0F 29 BC 24 ? ? ? ? 48 8B 05 ? ? ? ? 48 33 C4 48 89 85 ? ? ? ? 44 8B F2";
            o_createModel = (PFN_createModel) scanner::GetAddress(module, pattern);

            if (o_createModel)
            {
                LOG_DEBUG("Hooking model selection");

                DetourTransactionBegin();
                DetourUpdateThread(GetCurrentThread());

                DetourAttach(&(PVOID&) o_createModel, hkcreateModel);

                DetourTransactionCommit();
            }
            else
            {
                LOG_ERROR("Couldn't hook model selection");
            }
        }
    }
    else
    {
        LOG_DEBUG("Didn't rehook");
    }
}