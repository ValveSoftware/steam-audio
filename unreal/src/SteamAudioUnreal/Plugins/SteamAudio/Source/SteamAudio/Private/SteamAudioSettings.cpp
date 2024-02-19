//
// Copyright 2017-2023 Valve Corporation.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "SteamAudioSettings.h"
#include "SteamAudioMaterial.h"
#include "SOFAFile.h"

// ---------------------------------------------------------------------------------------------------------------------
// USteamAudioSettings
// ---------------------------------------------------------------------------------------------------------------------

USteamAudioSettings::USteamAudioSettings()
    : AudioEngine(EAudioEngineType::UNREAL)
    , bExportLandscapeGeometry(true)
    , bExportBSPGeometry(true)
    , DefaultMeshMaterial("/SteamAudio/Materials/Default.Default")
    , DefaultLandscapeMaterial("/SteamAudio/Materials/Default.Default")
    , DefaultBSPMaterial("/SteamAudio/Materials/Default.Default")
    , SceneType(ESceneType::DEFAULT)
    , MaxOcclusionSamples(16)
    , RealTimeRays(4096)
    , RealTimeBounces(4)
    , RealTimeDuration(1.0f)
    , RealTimeAmbisonicOrder(1)
    , RealTimeMaxSources(32)
    , RealTimeCPUCoresPercentage(5)
    , RealTimeIrradianceMinDistance(1.0f)
    , bBakeConvolution(true)
    , bBakeParametric(false)
    , BakingRays(16384)
    , BakingBounces(16)
    , BakingDuration(1.0f)
    , BakingAmbisonicOrder(1)
    , BakingCPUCoresPercentage(50)
    , BakingIrradianceMinDistance(1.0f)
    , ReverbSubmix(nullptr)
    , BakingVisibilitySamples(4)
    , BakingVisibilityRadius(1.0f)
    , BakingVisibilityThreshold(0.1f)
    , BakingVisibilityRange(1000.0f)
    , BakingPathRange(1000.0f)
    , BakedPathingCPUCoresPercentage(50)
    , SimulationUpdateInterval(0.1f)
    , ReflectionEffectType(EReflectionEffectType::CONVOLUTION)
    , HybridReverbTransitionTime(1.0f)
    , HybridReverbOverlapPercent(25)
    , DeviceType(EOpenCLDeviceType::ANY)
    , MaxReservedComputeUnits(8)
    , FractionComputeUnitsForIRUpdate(0.5f)
    , BakingBatchSize(8)
    , TANDuration(1.0f)
    , TANAmbisonicOrder(1)
    , TANMaxSources(32)
    , HRTFVolume(0.0f)
    , HRTFNormalizationType(EHRTFNormType::NONE)
    , SOFAFile(nullptr)
    , EnableValidation(false)
{}

FSteamAudioSettings USteamAudioSettings::GetSettings() const
{
    FSteamAudioSettings Settings{};
    Settings.AudioEngine = AudioEngine;
    Settings.bExportLandscapeGeometry = bExportLandscapeGeometry;
    Settings.bExportBSPGeometry = bExportBSPGeometry;
    Settings.DefaultMeshMaterial = GetMaterialForAsset(DefaultMeshMaterial);
    Settings.DefaultLandscapeMaterial = GetMaterialForAsset(DefaultLandscapeMaterial);
    Settings.DefaultBSPMaterial = GetMaterialForAsset(DefaultBSPMaterial);
    Settings.SceneType = static_cast<IPLSceneType>(SceneType);
    Settings.MaxOcclusionSamples = MaxOcclusionSamples;
    Settings.RealTimeRays = RealTimeRays;
    Settings.RealTimeBounces = RealTimeBounces;
    Settings.RealTimeDuration = RealTimeDuration;
    Settings.RealTimeAmbisonicOrder = RealTimeAmbisonicOrder;
    Settings.RealTimeMaxSources = RealTimeMaxSources;
    Settings.RealTimeCPUCoresPercentage = RealTimeCPUCoresPercentage;
    Settings.RealTimeIrradianceMinDistance = RealTimeIrradianceMinDistance;
    Settings.bBakeConvolution = bBakeConvolution;
    Settings.bBakeParametric = bBakeParametric;
    Settings.BakingRays = BakingRays;
    Settings.BakingBounces = BakingBounces;
    Settings.BakingDuration = BakingDuration;
    Settings.BakingAmbisonicOrder = BakingAmbisonicOrder;
    Settings.BakingCPUCoresPercentage = BakingCPUCoresPercentage;
    Settings.BakedPathingCPUCoresPercentage = BakedPathingCPUCoresPercentage;
    Settings.ReverbSubmix = nullptr;
    Settings.BakingVisibilitySamples = BakingVisibilitySamples;
    Settings.BakingVisibilityRadius = BakingVisibilityRadius;
    Settings.BakingVisibilityThreshold = BakingVisibilityThreshold;
    Settings.BakingVisibilityRange = BakingVisibilityRange;
    Settings.BakingPathRange = BakingPathRange;
    Settings.BakedPathingCPUCoresPercentage = BakedPathingCPUCoresPercentage;
    Settings.SimulationUpdateInterval = SimulationUpdateInterval;
    Settings.ReflectionEffectType = static_cast<IPLReflectionEffectType>(ReflectionEffectType);
    Settings.HybridReverbTransitionTime = HybridReverbTransitionTime;
    Settings.HybridReverbOverlapPercent = HybridReverbOverlapPercent;
    Settings.OpenCLDeviceType = static_cast<IPLOpenCLDeviceType>(DeviceType);
    Settings.MaxReservedComputeUnits = MaxReservedComputeUnits;
    Settings.FractionComputeUnitsForIRUpdate = FractionComputeUnitsForIRUpdate;
    Settings.BakingBatchSize = BakingBatchSize;
    Settings.TANDuration = TANDuration;
    Settings.TANAmbisonicOrder = TANAmbisonicOrder;
    Settings.TANMaxSources = TANMaxSources;
    Settings.SOFAFile = Cast<USOFAFile>(SOFAFile.TryLoad());
    Settings.HRTFVolume = (Settings.SOFAFile) ? Settings.SOFAFile->Volume : HRTFVolume;
    Settings.HRTFNormType = (Settings.SOFAFile) ? static_cast<IPLHRTFNormType>(Settings.SOFAFile->NormalizationType) : static_cast<IPLHRTFNormType>(HRTFNormalizationType);
    return Settings;
}

IPLMaterial USteamAudioSettings::GetMaterialForAsset(FSoftObjectPath Asset) const
{
    IPLMaterial SteamAudioMaterial{};
    SteamAudioMaterial.absorption[0] = 0.1f;
    SteamAudioMaterial.absorption[1] = 0.1f;
    SteamAudioMaterial.absorption[2] = 0.1f;
    SteamAudioMaterial.scattering = 0.5f;
    SteamAudioMaterial.transmission[0] = 0.1f;
    SteamAudioMaterial.transmission[1] = 0.1f;
    SteamAudioMaterial.transmission[2] = 0.1f;

    USteamAudioMaterial* Material = Cast<USteamAudioMaterial>(Asset.TryLoad());
    if (Material)
    {
        SteamAudioMaterial.absorption[0] = Material->AbsorptionLow;
        SteamAudioMaterial.absorption[1] = Material->AbsorptionMid;
        SteamAudioMaterial.absorption[2] = Material->AbsorptionHigh;
        SteamAudioMaterial.scattering = Material->Scattering;
        SteamAudioMaterial.transmission[0] = Material->TransmissionLow;
        SteamAudioMaterial.transmission[1] = Material->TransmissionMid;
        SteamAudioMaterial.transmission[2] = Material->TransmissionHigh;
    }

    return SteamAudioMaterial;
}

UObject* USteamAudioSettings::GetObjectForAsset(FSoftObjectPath Asset) const
{
    return Asset.TryLoad();
}
