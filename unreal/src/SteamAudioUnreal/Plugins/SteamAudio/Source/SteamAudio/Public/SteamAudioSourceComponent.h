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
#include "Components/ActorComponent.h"
#include "SteamAudioSourceComponent.generated.h"

class ASteamAudioProbeVolume;
class USteamAudioBakedSourceComponent;

namespace SteamAudio {

class IAudioEngineSource;

}

// ---------------------------------------------------------------------------------------------------------------------
// Enumerations
// ---------------------------------------------------------------------------------------------------------------------

/**
 * Equivalent to IPLOcclusionType.
 */
UENUM(BlueprintType)
enum class EOcclusionType : uint8
{
    RAYCAST		UMETA(DisplayName = "Raycast"),
    VOLUMETRIC	UMETA(DisplayName = "Volumetric"),
};

/**
 * Ways in which reflections can be simulated for a source.
 */
UENUM(BlueprintType)
enum class EReflectionSimulationType : uint8
{
    REALTIME                UMETA(DisplayName = "Real-time"),
    BAKED_STATIC_SOURCE     UMETA(DisplayName = "Baked Static Source"),
    BAKED_STATIC_LISTENER   UMETA(DisplayName = "Baked Static Listener"),
};


// ---------------------------------------------------------------------------------------------------------------------
// USteamAudioSourceComponent
// ---------------------------------------------------------------------------------------------------------------------

/**
 * Specifies Steam Audio simulation settings to use for an Actor that contains an Audio Component.
 */
UCLASS(ClassGroup = (SteamAudio), meta = (BlueprintSpawnableComponent), HideCategories = (Activation, Collision, Tags, Rendering, Physics, LOD, Mobility, Cooking, AssetUserData))
class STEAMAUDIO_API USteamAudioSourceComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    /** If true, occlusion will be simulated via ray tracing. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = OcclusionSettings)
    bool bSimulateOcclusion;

    /** Specifies how rays should be traced to model occlusion. Only if simulating occlusion. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = OcclusionSettings)
    EOcclusionType OcclusionType;

    /** The apparent size of the sound source. Only if using volumetric occlusion. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = OcclusionSettings, meta = (UIMin = "0.0", UIMax = "4.0"))
    float OcclusionRadius;

    /** The number of rays to trace from the listener to various points in a sphere around the source. Only if using
        volumetric occlusion. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = OcclusionSettings, meta = (UIMin = "1", UIMax = "128"))
    int OcclusionSamples;

    /** The occlusion attenuation value. Only if not simulating occlusion. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = OcclusionSettings, meta = (UIMin = "0.0", UIMax = "1.0"))
    float OcclusionValue;

    /** If true, transmission will be simulated via ray tracing. Only if simulating occlusion. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = OcclusionSettings)
    bool bSimulateTransmission;

    /** The low frequency (up to 800 Hz) EQ value for transmission. Only if not simulating transmission. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = OcclusionSettings, meta = (UIMin = "0.0", UIMax = "1.0"))
    float TransmissionLowValue;

    /** The middle frequency (800 Hz to 8 kHz) EQ value for transmission. Only if not simulating transmission. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = OcclusionSettings, meta = (UIMin = "0.0", UIMax = "1.0"))
    float TransmissionMidValue;

    /** The high frequency (8 kHz and above) EQ value for transmission. Only if not simulating transmission. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = OcclusionSettings, meta = (UIMin = "0.0", UIMax = "1.0"))
    float TransmissionHighValue;

    /** The maximum number of rays to trace when finding surfaces between the source and the listener for the
        purposes of simulating transmission. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = OcclusionSettings, meta = (UIMin = "1", UIMax = "8"))
    int MaxTransmissionSurfaces;

    /** If true, reflections from the source to the listener will be simulated. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ReflectionsSettings)
    bool bSimulateReflections;

    /** How to simulate reflections. Only if simulating reflections. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ReflectionsSettings)
    EReflectionSimulationType ReflectionsType;

    /** The static source from which to simulate reflections. Only if simulating reflections. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ReflectionsSettings)
    TSoftObjectPtr<AActor> CurrentBakedSource;

    /** If true, pathing from the source to the listener will be simulated. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PathingSettings)
    bool bSimulatePathing;

    /** The probe volume within which to simulate pathing. Only if simulating pathing. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PathingSettings)
    TSoftObjectPtr<ASteamAudioProbeVolume> PathingProbeBatch;

    /** If true, baked paths are checked for occlusion by dynamic geometry. Only if simulating pathing. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PathingSettings)
    bool bPathValidation;

    /** If true, if a baked path is occluded by dynamic geometry, alternate paths will be searched for at runtime. Only if simulating pathing. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PathingSettings)
    bool bFindAlternatePaths;

    USteamAudioSourceComponent();

    IPLSource GetSource() { return Source; }

    /** Sets simulation inputs for the given type of simulation. */
    void SetInputs(IPLSimulationFlags Flags);

    /** Retrieves simulation outputs for the given type of simulation. */
    IPLSimulationOutputs GetOutputs(IPLSimulationFlags Flags);

    /** Updates component properties for the given type of simulation based on simulation outputs. */
    void UpdateOutputs(IPLSimulationFlags Flags);

    /** Returns the baked data identifier for this source. */
    IPLBakedDataIdentifier GetBakedDataIdentifier() const;

    /**
     * Inherited from UActorComponent
     */

    virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

#if WITH_EDITOR
    /** Used to disable UI controls based on the values of other UI controls. */
    virtual bool CanEditChange(const FProperty* InProperty) const override;
#endif

protected:
    /**
     * Inherited from UActorComponent
     */

    /** Called when the component has been initialized. */
    virtual void BeginPlay() override;

    /** Called when the component is going to be destroyed. */
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    /** The Source object. */
    IPLSource Source;

    /** Retained reference to the Steam Audio simulator. */
    IPLSimulator Simulator;

    /** Interface for communicating with the spatializer effect instance. */
    TSharedPtr<SteamAudio::IAudioEngineSource> AudioEngineSource;
};
