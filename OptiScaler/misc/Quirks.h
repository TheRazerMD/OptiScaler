#pragma once

#include "pch.h"

#include <flag-set-cpp/flag_set.hpp>

enum class GameQuirk : uint64_t
{
    // Config-level quirks, de facto customized defaults
    DisableHudfix,
    DisableFSR3Inputs,
    DisableFSR2Inputs,
    DisableFFXInputs,
    RestoreComputeSigOnNonNvidia,
    RestoreComputeSigOnNvidia,
    ForceAutoExposure,
    DisableReactiveMasks,
    DisableDxgiSpoofing,
    DisableUseFsrInputValues,
    EnableVulkanSpoofing,
    EnableVulkanExtensionSpoofing,
    DisableOptiXessPipelineCreation,
    DontUseNTShared,
    DontUseUnrealBarriers,
    SkipFirst10Frames,
    DisableVsyncOverride,
    UseNtDllHooks,
    UseFSR2PatternMatching,
    AlwaysCaptureFSRFGSwapchain,
    AllowedFrameAhead2,
    DisableXeFGChecks,

    // Quirks that are applied deeper in code
    CyberpunkHudlessStateOverride,
    SkipFsr3Method,
    FastFeatureReset,
    LoadD3D12Manually,
    KernelBaseHooks,
    VulkanDLSSBarrierFixup,
    ForceUnrealEngine,
    NoFSRFGFirstSwapchain,
    FixSlSimulationMarkers,
    HitmanReflexHacks,
    SkipD3D11FeatureLevelElevation,
    // Don't forget to add the new entry to printQuirks
    _
};

struct QuirkEntry
{
    const char* exeName;
    std::initializer_list<GameQuirk> quirks;
};

// For regular exes
#define QUIRK_ENTRY(name, ...)                                                                                         \
    {                                                                                                                  \
        name, { __VA_ARGS__ }                                                                                          \
    }

// For UE exes
#define QUIRK_ENTRY_UE(name, ...)                                                                                      \
    { #name "-win64-shipping.exe", { __VA_ARGS__ } },                                                                  \
    {                                                                                                                  \
        #name "-wingdk-shipping.exe", { __VA_ARGS__ }                                                                  \
    }

// exeName has to be lowercase
static const QuirkEntry quirkTable[] = {
    // Red Dead Redemption
    QUIRK_ENTRY("rdr.exe", GameQuirk::SkipFsr3Method, GameQuirk::NoFSRFGFirstSwapchain, GameQuirk::DisableDxgiSpoofing),
    QUIRK_ENTRY("playrdr.exe", GameQuirk::SkipFsr3Method, GameQuirk::NoFSRFGFirstSwapchain,
                GameQuirk::DisableDxgiSpoofing),

    // No Man's Sky
    QUIRK_ENTRY("nms.exe", GameQuirk::KernelBaseHooks, GameQuirk::VulkanDLSSBarrierFixup),

    // Visions of Mana
    // Use FSR2 Pattern Matching to fix broken FSR2 detection
    QUIRK_ENTRY_UE(visionsofmana, GameQuirk::UseFSR2PatternMatching, GameQuirk::DisableDxgiSpoofing),

    // Silent Hill f
    QUIRK_ENTRY_UE(shf, GameQuirk::AlwaysCaptureFSRFGSwapchain),

    // Path of Exile 2
    QUIRK_ENTRY("pathofexile.exe", GameQuirk::LoadD3D12Manually),
    QUIRK_ENTRY("pathofexile_x64.exe", GameQuirk::LoadD3D12Manually),
    QUIRK_ENTRY("pathofexilesteam.exe", GameQuirk::LoadD3D12Manually),
    QUIRK_ENTRY("pathofexile_x64steam.exe", GameQuirk::LoadD3D12Manually),

    // Where Winds Meet
    QUIRK_ENTRY("wwm.exe", GameQuirk::DisableXeFGChecks),

    // The Last of Us Part I
    QUIRK_ENTRY("tlou-i.exe", GameQuirk::AllowedFrameAhead2),
    QUIRK_ENTRY("tlou-i-l.exe", GameQuirk::AllowedFrameAhead2),

    // Crapcom Games, DLSS without dxgi spoofing needs restore compute in those
    //
    // Kunitsu-Gami: Path of the Goddess, Monster Hunter Wilds, MONSTER HUNTER RISE, Dead Rising Deluxe Remaster
    // (including the demo), Dragon's Dogma 2
    QUIRK_ENTRY("kunitsugami.exe", GameQuirk::RestoreComputeSigOnNonNvidia, GameQuirk::DisableDxgiSpoofing),
    QUIRK_ENTRY("kunitsugamidemo.exe", GameQuirk::RestoreComputeSigOnNonNvidia, GameQuirk::DisableDxgiSpoofing),
    QUIRK_ENTRY("monsterhunterwilds.exe", GameQuirk::RestoreComputeSigOnNonNvidia, GameQuirk::DisableDxgiSpoofing,
                GameQuirk::RestoreComputeSigOnNvidia),
    QUIRK_ENTRY("monsterhunterrise.exe", GameQuirk::RestoreComputeSigOnNvidia), // Seems to fix real DLSS
    QUIRK_ENTRY("drdr.exe", GameQuirk::RestoreComputeSigOnNonNvidia, GameQuirk::DisableDxgiSpoofing),
    QUIRK_ENTRY("dd2ccs.exe", GameQuirk::RestoreComputeSigOnNonNvidia, GameQuirk::DisableDxgiSpoofing),
    QUIRK_ENTRY("dd2.exe", GameQuirk::RestoreComputeSigOnNonNvidia, GameQuirk::DisableDxgiSpoofing),

    // Cyberpunk 2077
    // SL spoof enough to unlock everything DLSS
    QUIRK_ENTRY("cyberpunk2077.exe", GameQuirk::CyberpunkHudlessStateOverride, GameQuirk::DisableHudfix,
                GameQuirk::DisableDxgiSpoofing),

    // Forza Horizon 5
    // SL spoof enough to unlock everything DLSS
    QUIRK_ENTRY("forzahorizon5.exe", GameQuirk::DisableFSR2Inputs, GameQuirk::DisableFSR3Inputs,
                GameQuirk::DisableDxgiSpoofing),

    // Avatar: Frontiers of Pandora
    QUIRK_ENTRY("afop.exe", GameQuirk::DisableFSR2Inputs, GameQuirk::DisableFSR3Inputs, GameQuirk::DisableDxgiSpoofing),

    // Forza Motorsport 8
    // Steam
    QUIRK_ENTRY("forza_steamworks_release_final.exe", GameQuirk::DisableFSR2Inputs, GameQuirk::DisableFSR3Inputs),
    // MS Store
    QUIRK_ENTRY("forza_gaming.desktop.x64_release_final.exe", GameQuirk::DisableFSR2Inputs,
                GameQuirk::DisableFSR3Inputs),

    // Death Stranding and Directors Cut
    // no spoof needed for DLSS inputs
    QUIRK_ENTRY("ds.exe", GameQuirk::DisableDxgiSpoofing, GameQuirk::DisableFSR2Inputs, GameQuirk::DisableFSR3Inputs,
                GameQuirk::UseNtDllHooks),

    // The Callisto Protocol
    // FSR2 only, no spoof needed
    QUIRK_ENTRY_UE(thecallistoprotocol, GameQuirk::DisableUseFsrInputValues, GameQuirk::DisableDxgiSpoofing,
                   GameQuirk::DisableReactiveMasks, GameQuirk::ForceAutoExposure),

    // HITMAN World of Assassination
    // SL spoof enough to unlock everything DLSS
    QUIRK_ENTRY("hitman3.exe", GameQuirk::DisableDxgiSpoofing, GameQuirk::HitmanReflexHacks,
                GameQuirk::DisableFSR2Inputs),

    // ELDEN RING (for ERSS mod) and ER NIGHTREIGN (for NRSS mod)
    // no spoof needed for DLSS inputs
    QUIRK_ENTRY("eldenring.exe", GameQuirk::DisableDxgiSpoofing),
    QUIRK_ENTRY("nightreign.exe", GameQuirk::DisableDxgiSpoofing, GameQuirk::DisableOptiXessPipelineCreation),

    // Returnal
    // no spoof needed for DLSS inputs, but no DLSSG and Reflex
    QUIRK_ENTRY_UE(returnal, GameQuirk::DisableDxgiSpoofing, GameQuirk::DontUseUnrealBarriers),

    // WUCHANG: Fallen Feathers
    // Skip 1 frame use of upscaler which cause crash
    QUIRK_ENTRY("project_plague-deck-shipping.exe", GameQuirk::SkipFirst10Frames),
    QUIRK_ENTRY("project_plague-win64-shipping.exe", GameQuirk::SkipFirst10Frames),

    // Final Fantasy XIV
    QUIRK_ENTRY("ffxiv_dx11.exe", GameQuirk::DisableVsyncOverride),
    QUIRK_ENTRY("graphadapterdesc.exe", GameQuirk::SkipD3D11FeatureLevelElevation),

    // Prey 2017
    // Requires Prey Luma Remastered mod for upscalers
    QUIRK_ENTRY("prey.exe", GameQuirk::DontUseNTShared, GameQuirk::DisableOptiXessPipelineCreation,
                GameQuirk::DisableDxgiSpoofing),

    // Avowed
    // NoBarriers needed to avoid post-loading crash with DLSS
    QUIRK_ENTRY_UE(avowed, GameQuirk::ForceAutoExposure, GameQuirk::DontUseUnrealBarriers, GameQuirk::DisableFSR2Inputs,
                   GameQuirk::DisableFSR3Inputs),

    // SL spoof enough to unlock everything DLSS/No spoof needed for DLSS inputs
    //
    // The Witcher 3, Alan Wake 2, Crysis 3 Remastered, Marvel's Guardians of the Galaxy, UNCHARTED: Legacy of Thieves
    // Collection, Warhammer 40,000: Darktide, Dying Light 2 Stay Human, Dying Light: The Beast, Observer: System Redux,
    // Sackboy: A Big Adventure, Hellblade: Senua's Sacrifice, Pumpkin Jack, Metro Exodus Enhanced Edition, Rise of the
    // Ronin, DYNASTY WARRIORS: ORIGINS, Crysis Remastered, Crysis 2 Remastered, Mortal Shell, Sekiro: Shadows Die
    // Twice (for SekiroTSR mod), The Medium, NINJA GAIDEN 4 (+ WinGDK), Dead Space Remake, God of War (2018), Europa
    // Universalis V, Need for Speed Unbound, Nioh 2 – The Complete Edition, Control Ultimate Edition, Deathloop, Where
    // Winds Meet
    QUIRK_ENTRY("witcher3.exe", GameQuirk::DisableDxgiSpoofing),
    QUIRK_ENTRY("alanwake2.exe", GameQuirk::DisableDxgiSpoofing),
    QUIRK_ENTRY("crysis3remastered.exe", GameQuirk::DisableDxgiSpoofing),
    QUIRK_ENTRY("gotg.exe", GameQuirk::DisableDxgiSpoofing),
    QUIRK_ENTRY("u4.exe", GameQuirk::DisableDxgiSpoofing),
    QUIRK_ENTRY("u4-l.exe", GameQuirk::DisableDxgiSpoofing),
    QUIRK_ENTRY("tll.exe", GameQuirk::DisableDxgiSpoofing),
    QUIRK_ENTRY("tll-l.exe", GameQuirk::DisableDxgiSpoofing),
    QUIRK_ENTRY("darktide.exe", GameQuirk::DisableDxgiSpoofing),
    QUIRK_ENTRY("dyinglightgame_x64_rwdi.exe", GameQuirk::DisableDxgiSpoofing),
    QUIRK_ENTRY("dyinglightgame_thebeast_x64_rwdi.exe", GameQuirk::DisableDxgiSpoofing),
    QUIRK_ENTRY("observersystemredux.exe", GameQuirk::DisableDxgiSpoofing, GameQuirk::ForceAutoExposure),
    QUIRK_ENTRY_UE(sackboy, GameQuirk::DisableDxgiSpoofing, GameQuirk::ForceAutoExposure),
    QUIRK_ENTRY_UE(hellbladegame, GameQuirk::DisableDxgiSpoofing),
    QUIRK_ENTRY_UE(pumpkinjack, GameQuirk::DisableDxgiSpoofing, GameQuirk::ForceAutoExposure),
    QUIRK_ENTRY("metroexodus.exe", GameQuirk::DisableDxgiSpoofing, GameQuirk::ForceAutoExposure),
    QUIRK_ENTRY("ronin.exe", GameQuirk::DisableDxgiSpoofing),
    QUIRK_ENTRY("dworigins.exe", GameQuirk::DisableDxgiSpoofing),
    QUIRK_ENTRY("crysisremastered.exe", GameQuirk::DisableDxgiSpoofing),
    QUIRK_ENTRY("crysis2remastered.exe", GameQuirk::DisableDxgiSpoofing),
    QUIRK_ENTRY_UE(dungeonhaven, GameQuirk::DisableDxgiSpoofing),
    QUIRK_ENTRY("sekiro.exe", GameQuirk::DisableDxgiSpoofing),
    QUIRK_ENTRY_UE(medium, GameQuirk::DisableDxgiSpoofing),
    QUIRK_ENTRY("ninjagaiden4-steam.exe", GameQuirk::DisableDxgiSpoofing),
    QUIRK_ENTRY("ninjagaiden4-wingdk.exe", GameQuirk::DisableDxgiSpoofing), // NG4 WinGDK
    QUIRK_ENTRY("dead space.exe", GameQuirk::DisableDxgiSpoofing),
    QUIRK_ENTRY("gow.exe", GameQuirk::DisableDxgiSpoofing),
    QUIRK_ENTRY("eu5.exe", GameQuirk::DisableDxgiSpoofing),
    QUIRK_ENTRY("needforspeedunbound.exe", GameQuirk::DisableDxgiSpoofing),
    QUIRK_ENTRY("nioh2.exe", GameQuirk::DisableDxgiSpoofing, GameQuirk::ForceAutoExposure),
    QUIRK_ENTRY("control_dx12.exe", GameQuirk::DisableDxgiSpoofing, GameQuirk::ForceAutoExposure),
    QUIRK_ENTRY("deathloop.exe", GameQuirk::DisableDxgiSpoofing),
    QUIRK_ENTRY("wwm.exe", GameQuirk::DisableDxgiSpoofing),

    // FSR2 only, no spoof needed
    //
    // Tiny Tina's Wonderlands, Dead Island 2, The Outer Worlds: Spacer's Choice Edition
    QUIRK_ENTRY("wonderlands.exe", GameQuirk::DisableReactiveMasks, GameQuirk::DisableDxgiSpoofing),
    QUIRK_ENTRY_UE(deadisland, GameQuirk::DisableReactiveMasks, GameQuirk::DisableDxgiSpoofing),
    QUIRK_ENTRY_UE(indiana, GameQuirk::DisableReactiveMasks, GameQuirk::DisableDxgiSpoofing,
                   GameQuirk::ForceAutoExposure),

    // Disable FSR2/3 inputs due to crashing/custom implementations
    //
    // Red Dead Redemption 2, Forgive Me Father 2, Revenge of the Savage Planet, F1 22, Metal Eden, Until Dawn, Bloom
    // and Rage, 171, Microsoft Flight Simulator (2020) - MSFS2020, Star Wars: Outlaws, Banishers: Ghosts of New Eden,
    // Rune Factory Guardians of Azuma, Supraworld, F1 Manager 2024, Keeper (+ WinGDK PaganIdol version)
    QUIRK_ENTRY("rdr2.exe", GameQuirk::DisableFSR2Inputs, GameQuirk::DisableFSR3Inputs),
    QUIRK_ENTRY("playrdr2.exe", GameQuirk::DisableFSR2Inputs, GameQuirk::DisableFSR3Inputs),
    QUIRK_ENTRY_UE(fmf2, GameQuirk::DisableFSR2Inputs, GameQuirk::DisableFSR3Inputs),
    QUIRK_ENTRY_UE(towers, GameQuirk::DisableFSR2Inputs,
                   GameQuirk::DisableFSR3Inputs), // Revenge of the Savage Planet
    QUIRK_ENTRY("f1_22.exe", GameQuirk::DisableFSR2Inputs, GameQuirk::DisableFSR3Inputs),
    QUIRK_ENTRY_UE(metaleden, GameQuirk::DisableFSR2Inputs, GameQuirk::DisableFSR3Inputs),
    QUIRK_ENTRY_UE(bates, GameQuirk::DisableFSR2Inputs, GameQuirk::DisableFSR3Inputs),
    QUIRK_ENTRY("bloom&rage.exe", GameQuirk::DisableFSR2Inputs, GameQuirk::DisableFSR3Inputs),
    QUIRK_ENTRY_UE(bcg, GameQuirk::DisableFSR2Inputs, GameQuirk::DisableFSR3Inputs), // 171
    QUIRK_ENTRY("flightsimulator.exe", GameQuirk::DisableFSR2Inputs, GameQuirk::DisableFSR3Inputs),
    QUIRK_ENTRY("outlaws.exe", GameQuirk::DisableFSR2Inputs, GameQuirk::DisableFSR3Inputs),
    QUIRK_ENTRY("outlaws_plus.exe", GameQuirk::DisableFSR2Inputs, GameQuirk::DisableFSR3Inputs),
    QUIRK_ENTRY_UE(banishers, GameQuirk::DisableFSR2Inputs, GameQuirk::DisableFSR3Inputs),
    QUIRK_ENTRY_UE(game, GameQuirk::DisableFSR2Inputs, GameQuirk::DisableFSR3Inputs), // Rune
    QUIRK_ENTRY_UE(supraworld, GameQuirk::DisableFSR2Inputs, GameQuirk::DisableFSR3Inputs),
    QUIRK_ENTRY("f1manager24.exe", GameQuirk::DisableFSR2Inputs, GameQuirk::DisableFSR3Inputs),
    QUIRK_ENTRY_UE(keeper, GameQuirk::DisableFSR2Inputs, GameQuirk::DisableFSR3Inputs),
    QUIRK_ENTRY_UE(paganidol, GameQuirk::DisableFSR2Inputs,
                   GameQuirk::DisableFSR3Inputs), // Keeper WinGDK PaganIdol

    // Self-explanatory
    //
    // The Persistence, Split Fiction, Minecraft Bedrock, Ghostwire: Tokyo, RoadCraft, STAR WARS Jedi:
    // Survivor, FINAL FANTASY VII REBIRTH, Witchfire, MechWarrior 5: Mercenaries, Ghostrunner, Ghostrunner 2
    QUIRK_ENTRY_UE(persistence, GameQuirk::ForceUnrealEngine),
    QUIRK_ENTRY("splitfiction.exe", GameQuirk::FastFeatureReset),
    QUIRK_ENTRY("minecraft.windows.exe", GameQuirk::KernelBaseHooks),
    QUIRK_ENTRY("gwt.exe", GameQuirk::ForceUnrealEngine),
    QUIRK_ENTRY("roadcraft - retail.exe", GameQuirk::FixSlSimulationMarkers),
    QUIRK_ENTRY("jedisurvivor.exe", GameQuirk::ForceAutoExposure),
    QUIRK_ENTRY("ff7rebirth_.exe", GameQuirk::ForceUnrealEngine),
    QUIRK_ENTRY_UE(witchfire, GameQuirk::DisableUseFsrInputValues),
    QUIRK_ENTRY_UE(mechwarrior, GameQuirk::ForceUnrealEngine),
    QUIRK_ENTRY_UE(ghostrunner, GameQuirk::ForceUnrealEngine),
    QUIRK_ENTRY_UE(ghostrunner2, GameQuirk::ForceUnrealEngine),

    // VULKAN
    // ------

    // No Man's Sky
    QUIRK_ENTRY("nms.exe", GameQuirk::EnableVulkanSpoofing, GameQuirk::EnableVulkanExtensionSpoofing),

    // RTX Remix
    QUIRK_ENTRY("nvremixbridge.exe", GameQuirk::EnableVulkanSpoofing, GameQuirk::EnableVulkanExtensionSpoofing,
                GameQuirk::VulkanDLSSBarrierFixup),

    // Enshrouded
    QUIRK_ENTRY("enshrouded.exe", GameQuirk::EnableVulkanSpoofing, GameQuirk::EnableVulkanExtensionSpoofing),

};

static flag_set<GameQuirk> getQuirksForExe(std::string exeName)
{
    to_lower_in_place(exeName);
    flag_set<GameQuirk> result;

    for (const auto& entry : quirkTable)
    {
        if (exeName == entry.exeName)
        {
            for (auto quirk : entry.quirks)
                result |= quirk;
        }
    }

    return result;
}

static void printQuirks(flag_set<GameQuirk>& quirks)
{
    if (quirks & GameQuirk::CyberpunkHudlessStateOverride)
        spdlog::info("Quirk: Fixing DLSSG's hudless in Cyberpunk");
    if (quirks & GameQuirk::SkipFsr3Method)
        spdlog::info("Quirk: Skipping first FSR 3 method");
    if (quirks & GameQuirk::FastFeatureReset)
        spdlog::info("Quirk: Quick upscaler reinit");
    if (quirks & GameQuirk::LoadD3D12Manually)
        spdlog::info("Quirk: Load d3d12.dll");
    if (quirks & GameQuirk::KernelBaseHooks)
        spdlog::info("Quirk: Enable KernelBase hooks");
    if (quirks & GameQuirk::VulkanDLSSBarrierFixup)
        spdlog::info("Quirk: Fix DLSS/DLSSG barriers on Vulkan");
    if (quirks & GameQuirk::ForceUnrealEngine)
        spdlog::info("Quirk: Force detected engine as Unreal Engine");
    if (quirks & GameQuirk::DisableHudfix)
        spdlog::info("Quirk: Disabling Hudfix due to known issues");
    if (quirks & GameQuirk::ForceAutoExposure)
        spdlog::info("Quirk: Enabling AutoExposure");
    if (quirks & GameQuirk::DisableFFXInputs)
        spdlog::info("Quirk: Disable FSR 3.1 Inputs");
    if (quirks & GameQuirk::DisableFSR3Inputs)
        spdlog::info("Quirk: Disable FSR 3.0 Inputs");
    if (quirks & GameQuirk::DisableFSR2Inputs)
        spdlog::info("Quirk: Disable FSR 2.X Inputs");
    if (quirks & GameQuirk::DisableReactiveMasks)
        spdlog::info("Quirk: Disable Reactive Masks");
    if (quirks & GameQuirk::RestoreComputeSigOnNonNvidia)
        spdlog::info("Quirk: Enabling restore compute signature on AMD/Intel");
    if (quirks & GameQuirk::RestoreComputeSigOnNvidia)
        spdlog::info("Quirk: Enabling restore compute signature on Nvidia");
    if (quirks & GameQuirk::DisableDxgiSpoofing)
        spdlog::info("Quirk: Dxgi spoofing disabled by default");
    if (quirks & GameQuirk::DisableUseFsrInputValues)
        spdlog::info("Quirk: Disable Use FSR Input Values");
    if (quirks & GameQuirk::DisableOptiXessPipelineCreation)
        spdlog::info("Quirk: Disable custom pipeline creation for XeSS");
    if (quirks & GameQuirk::DontUseNTShared)
        spdlog::info("Quirk: Don't use NTShared enabled");
    if (quirks & GameQuirk::DontUseUnrealBarriers)
        spdlog::info("Quirk: Don't use resource barrier fix for Unreal Engine games");
    if (quirks & GameQuirk::SkipFirst10Frames)
        spdlog::info("Quirk: Skipping upscaling for first 10 frames");
    if (quirks & GameQuirk::NoFSRFGFirstSwapchain)
        spdlog::info("Quirk: Skip turning the first swapchain created into an FSR swapchain");
    if (quirks & GameQuirk::FixSlSimulationMarkers)
        spdlog::info("Quirk: Correct simulation start marker's frame id");
    if (quirks & GameQuirk::DisableVsyncOverride)
        spdlog::info("Quirk: Don't use V-Sync overrides");
    if (quirks & GameQuirk::HitmanReflexHacks)
        spdlog::info("Quirk: Hack for broken Hitman reflex");
    if (quirks & GameQuirk::SkipD3D11FeatureLevelElevation)
        spdlog::info("Quirk: Skipping D3D11 feature level elevation, native FSR3.1 will be disabled!");
    if (quirks & GameQuirk::UseNtDllHooks)
        spdlog::info("Quirk: Using NTdll hooks instead of kernel ones");
    if (quirks & GameQuirk::UseFSR2PatternMatching)
        spdlog::info("Quirk: Use FSR2 pattern matching");
    if (quirks & GameQuirk::AlwaysCaptureFSRFGSwapchain)
        spdlog::info("Quirk: Always capture FSR-FG swapchain");
    if (quirks & GameQuirk::AllowedFrameAhead2)
        spdlog::info("Quirk: Allowed Frame Ahead: 2");
    if (quirks & GameQuirk::DisableXeFGChecks)
        spdlog::info("Quirk: Skip pre init checks for XeFG");

    return;
}
