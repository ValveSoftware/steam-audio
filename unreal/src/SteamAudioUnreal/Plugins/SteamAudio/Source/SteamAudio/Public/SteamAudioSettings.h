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

#pragma once

#include "SteamAudioModule.h"
#include "SteamAudioSettings.generated.h"

// ---------------------------------------------------------------------------------------------------------------------
// Enumerations
// ---------------------------------------------------------------------------------------------------------------------

/**
 * The audio engine with which we want to integrate.
 */
UENUM(BlueprintType)
enum class EAudioEngineType : uint8
{
    UNREAL      UMETA(DisplayName = "Unreal"),
    FMODSTUDIO  UMETA(DisplayName = "FMOD Studio"),
    WWISE       UMETA(DisplayName = "Wwise"),
};

/**
 * Equivalent to IPLSceneType.
 */
UENUM(BlueprintType)
enum class ESceneType : uint8
{
    DEFAULT     UMETA(DisplayName = "Default"),
    EMBREE      UMETA(DisplayName = "Embree"),
    RADEONRAYS  UMETA(DisplayName = "Radeon Rays"),
};

/**
 * Equivalent to IPLReflectionEffectType.
 */
UENUM(BlueprintType)
enum class EReflectionEffectType : uint8
{
    CONVOLUTION     UMETA(DisplayName = "Convolution"),
    PARAMETRIC      UMETA(DisplayName = "Parametric"),
    HYBRID          UMETA(DisplayName = "Hybrid"),
    TRUEAUDIONEXT   UMETA(DisplayName = "TrueAudio Next"),
};

/**
 * Equivalent to IPLOpenCLDeviceType.
 */
UENUM(BlueprintType)
enum class EOpenCLDeviceType : uint8
{
    ANY UMETA(DisplayName = "Any"),
    CPU UMETA(DisplayName = "CPU"),
    GPU UMETA(DisplayName = "GPU"),
};

/**
 * Equivalent to IPLHRTFNormType.
 */
UENUM(BlueprintType)
enum class EHRTFNormType : uint8
{
    NONE    UMETA(DisplayName = "None"),
    RMS     UMETA(DisplayName = "RMS"),
};


// ---------------------------------------------------------------------------------------------------------------------
// FSteamAudioSettings
// ---------------------------------------------------------------------------------------------------------------------

class USOFAFile;

/**
 * Used to store a copy of the current Steam Audio settings upon initialization, with Unreal plugin types replaced by
 * their corresponding Steam Audio API types.
 */
struct FSteamAudioSettings
{
    EAudioEngineType AudioEngine;
    bool bExportLandscapeGeometry;
    bool bExportBSPGeometry;
    IPLMaterial DefaultMeshMaterial;
    IPLMaterial DefaultLandscapeMaterial;
    IPLMaterial DefaultBSPMaterial;
    IPLSceneType SceneType;
    int MaxOcclusionSamples;
    int RealTimeRays;
    int RealTimeBounces;
    float RealTimeDuration;
    int RealTimeAmbisonicOrder;
    int RealTimeMaxSources;
    int RealTimeCPUCoresPercentage;
    float RealTimeIrradianceMinDistance;
    bool bBakeConvolution;
    bool bBakeParametric;
    int BakingRays;
    int BakingBounces;
    float BakingDuration;
    int BakingAmbisonicOrder;
    int BakingCPUCoresPercentage;
    float BakingIrradianceMinDistance;
    UObject* ReverbSubmix;
    int BakingVisibilitySamples;
    float BakingVisibilityRadius;
    float BakingVisibilityThreshold;
    float BakingVisibilityRange;
    float BakingPathRange;
    int BakedPathingCPUCoresPercentage;
    float SimulationUpdateInterval;
    IPLReflectionEffectType ReflectionEffectType;
    float HybridReverbTransitionTime;
    int HybridReverbOverlapPercent;
    IPLOpenCLDeviceType OpenCLDeviceType;
    int MaxReservedComputeUnits;
    float FractionComputeUnitsForIRUpdate;
    int BakingBatchSize;
    float TANDuration;
    int TANAmbisonicOrder;
    int TANMaxSources;
    USOFAFile* SOFAFile;
    float HRTFVolume;
    IPLHRTFNormType HRTFNormType;
};


// ---------------------------------------------------------------------------------------------------------------------
// USteamAudioSettings
// ---------------------------------------------------------------------------------------------------------------------

/**
 * Global settings for the Steam Audio plugin.
 */
UCLASS(Config = Engine, DefaultConfig)
class STEAMAUDIO_API USteamAudioSettings : public UObject
{
    GENERATED_BODY()

public:
    /** The audio engine with which we want to integrate. If this is set to use third-party middleware, the
        corresponding Steam Audio support plugin must also be enabled in your project settings. */
    UPROPERTY(GlobalConfig, EditAnywhere, Category = AudioEngineSettings)
    EAudioEngineType AudioEngine;

    /** If true, Landscape actors (terrain) will be exported as part of a level's static geometry. */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = SceneExportSettings)
	bool bExportLandscapeGeometry;

    /** If true, BSP geometry will be exported as part of a level's static geometry. */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = SceneExportSettings, meta = (DisplayName = "Export BSP Geometry"))
	bool bExportBSPGeometry;

    /** Reference to the Steam Audio Material asset to use as the default material for Static Mesh actors. */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = SceneExportSettings, meta = (AllowedClasses = "/Script/SteamAudio.SteamAudioMaterial"))
	FSoftObjectPath DefaultMeshMaterial;

    /** Reference to the Steam Audio Material asset to use as the default material for Landscape actors. */
    UPROPERTY(GlobalConfig, EditAnywhere, Category = SceneExportSettings, meta = (AllowedClasses = "/Script/SteamAudio.SteamAudioMaterial"))
    FSoftObjectPath DefaultLandscapeMaterial;

    /** Reference to the Steam Audio Material asset to use as the default material for BSP geometry. */
    UPROPERTY(GlobalConfig, EditAnywhere, Category = SceneExportSettings, meta = (AllowedClasses = "/Script/SteamAudio.SteamAudioMaterial", DisplayName = "Default BSP Material"))
    FSoftObjectPath DefaultBSPMaterial;

    UPROPERTY(GlobalConfig, EditAnywhere, Category = RayTracerSettings)
    ESceneType SceneType;

    /** The maximum possible value of Occlusion Samples that can be specified on any Steam Audio Source component. */
    UPROPERTY(GlobalConfig, EditAnywhere, Category = OcclusionSettings, meta = (UIMin = 1, UIMax = 128))
    int MaxOcclusionSamples;

    UPROPERTY(GlobalConfig, EditAnywhere, Category = ReflectionsSettings, meta = (UIMin = 1024, UIMax = 65536))
    int RealTimeRays;

    UPROPERTY(GlobalConfig, EditAnywhere, Category = ReflectionsSettings, meta = (UIMin = 1, UIMax = 64))
    int RealTimeBounces;

    UPROPERTY(GlobalConfig, EditAnywhere, Category = ReflectionsSettings, meta = (UIMin = 0.1f, UIMax = 10.0f))
    float RealTimeDuration;

    UPROPERTY(GlobalConfig, EditAnywhere, Category = ReflectionsSettings, meta = (UIMin = 0, UIMax = 3))
    int RealTimeAmbisonicOrder;

    UPROPERTY(GlobalConfig, EditAnywhere, Category = ReflectionsSettings, meta = (UIMin = 1, UIMax = 128))
    int RealTimeMaxSources;

    UPROPERTY(GlobalConfig, EditAnywhere, Category = ReflectionsSettings, meta = (UIMin = 0, UIMax = 100, DisplayName = "Real Time CPU Cores Percentage"))
    int RealTimeCPUCoresPercentage;

    UPROPERTY(GlobalConfig, EditAnywhere, Category = ReflectionsSettings, meta = (UIMin = 0.1f, UIMax = 10.0f))
    float RealTimeIrradianceMinDistance;

    UPROPERTY(GlobalConfig, EditAnywhere, Category = ReflectionsSettings)
    bool bBakeConvolution;

    UPROPERTY(GlobalConfig, EditAnywhere, Category = ReflectionsSettings)
    bool bBakeParametric;

    UPROPERTY(GlobalConfig, EditAnywhere, Category = ReflectionsSettings, meta = (UIMin = 1024, UIMax = 65536))
    int BakingRays;

    UPROPERTY(GlobalConfig, EditAnywhere, Category = ReflectionsSettings, meta = (UIMin = 1, UIMax = 64))
    int BakingBounces;

    UPROPERTY(GlobalConfig, EditAnywhere, Category = ReflectionsSettings, meta = (UIMin = 0.1f, UIMax = 10.0f))
    float BakingDuration;

    UPROPERTY(GlobalConfig, EditAnywhere, Category = ReflectionsSettings, meta = (UIMin = 0, UIMax = 3))
    int BakingAmbisonicOrder;

    UPROPERTY(GlobalConfig, EditAnywhere, Category = ReflectionsSettings, meta = (UIMin = 0, UIMax = 100, DisplayName = "Baking CPU Cores Percentage"))
    int BakingCPUCoresPercentage;

    UPROPERTY(GlobalConfig, EditAnywhere, Category = ReflectionsSettings, meta = (UIMin = 0.1f, UIMax = 10.0f))
    float BakingIrradianceMinDistance;

    UPROPERTY(GlobalConfig, EditAnywhere, Category = ReverbSettings, meta = (AllowedClasses = "/Script/Engine.SoundSubmix"))
    FSoftObjectPath ReverbSubmix;

    UPROPERTY(GlobalConfig, EditAnywhere, Category = PathingSettings, meta = (UIMin = 1, UIMax = 32))
    int BakingVisibilitySamples;

	UPROPERTY(GlobalConfig, EditAnywhere, Category = PathingSettings, meta = (UIMin = 0.0f, UIMax = 2.0f))
	float BakingVisibilityRadius;

	UPROPERTY(GlobalConfig, EditAnywhere, Category = PathingSettings, meta = (UIMin = 0.0f, UIMax = 1.0f))
	float BakingVisibilityThreshold;

	UPROPERTY(GlobalConfig, EditAnywhere, Category = PathingSettings, meta = (UIMin = 0.0f, UIMax = 1000.0f))
	float BakingVisibilityRange;

	UPROPERTY(GlobalConfig, EditAnywhere, Category = PathingSettings, meta = (UIMin = 0.0f, UIMax = 1000.0f))
	float BakingPathRange;

	UPROPERTY(GlobalConfig, EditAnywhere, Category = PathingSettings, meta = (UIMin = 0, UIMax = 100, DisplayName = "Baked Pathing CPU Cores Percentage"))
	int BakedPathingCPUCoresPercentage;

    UPROPERTY(GlobalConfig, EditAnywhere, Category = SimulationUpdateSettings, meta = (UIMin = 0.1f, UIMax = 1.0f))
    float SimulationUpdateInterval;

    UPROPERTY(GlobalConfig, EditAnywhere, Category = ReflectionEffectSettings)
    EReflectionEffectType ReflectionEffectType;

	UPROPERTY(GlobalConfig, EditAnywhere, Category = HybridReverbSettings, meta = (UIMin = 0.1f, UIMax = 2.0f))
	float HybridReverbTransitionTime;

	UPROPERTY(GlobalConfig, EditAnywhere, Category = HybridReverbSettings, meta = (UIMin = 0, UIMax = 100))
	int HybridReverbOverlapPercent;

	UPROPERTY(GlobalConfig, EditAnywhere, Category = "OpenCL Settings")
	EOpenCLDeviceType DeviceType;

    UPROPERTY(GlobalConfig, EditAnywhere, Category = "OpenCL Settings", meta = (UIMin = 0, UIMax = 16))
    int MaxReservedComputeUnits;

    UPROPERTY(GlobalConfig, EditAnywhere, Category = "OpenCL Settings", meta = (UIMin = 0.0f, UIMax = 1.0f, DisplayName = "Fraction Compute Units For IR Update"))
    float FractionComputeUnitsForIRUpdate;

	UPROPERTY(GlobalConfig, EditAnywhere, Category = RadeonRaysSettings, meta = (UIMin = 1, UIMax = 16))
	int BakingBatchSize;

	UPROPERTY(GlobalConfig, EditAnywhere, Category = "TrueAudio Next Settings", meta = (UIMin = 0.1f, UIMax = 10.0f, DisplayName = "TAN Duration"))
	float TANDuration;

	UPROPERTY(GlobalConfig, EditAnywhere, Category = "TrueAudio Next Settings", meta = (UIMin = 0, UIMax = 3, DisplayName = "TAN Ambisonic Order"))
	int TANAmbisonicOrder;

	UPROPERTY(GlobalConfig, EditAnywhere, Category = "TrueAudio Next Settings", meta = (UIMin = 1, UIMax = 128, DisplayName = "TAN Max Sources"))
	int TANMaxSources;

    UPROPERTY(Config, EditAnywhere, Category = "Default HRTF Settings", meta = (DisplayName = "HRTF Volume Gain (dB)", UIMin = "-12.0", UIMax = "12.0"))
    float HRTFVolume;

    UPROPERTY(Config, EditAnywhere, Category = "Default HRTF Settings", meta = (DisplayName = "HRTF Normalization Type"))
    EHRTFNormType HRTFNormalizationType;

    UPROPERTY(Config, EditAnywhere, Category = "Custom HRTF Settings", meta = (DisplayName = "SOFA File", AllowedClasses = "/Script/SteamAudio.SOFAFile"))
    FSoftObjectPath SOFAFile;

    UPROPERTY(Config, EditAnywhere, Category = "Advanced Settings")
    bool EnableValidation;

    USteamAudioSettings();

    /** Returns a copy of the settings in a raw struct. */
    FSteamAudioSettings GetSettings() const;

    /** Loads a Steam Audio Material asset and converts it into an IPLMaterial struct. */
    IPLMaterial GetMaterialForAsset(FSoftObjectPath Asset) const;

    /** Loads a Steam Audio Reverb Submix asset. */
    UObject* GetObjectForAsset(FSoftObjectPath Asset) const;
};
