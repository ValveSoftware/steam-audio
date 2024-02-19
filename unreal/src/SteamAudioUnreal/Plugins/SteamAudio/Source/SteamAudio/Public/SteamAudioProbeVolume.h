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
#include "GameFramework/Volume.h"
#include "SteamAudioProbeVolume.generated.h"

class ASteamAudioStaticMeshActor;
class USteamAudioProbeComponent;

// ---------------------------------------------------------------------------------------------------------------------
// Enumerations
// ---------------------------------------------------------------------------------------------------------------------

/**
 * Equivalent to IPLProbeGenerationType.
 */
UENUM(BlueprintType)
enum class EProbeGenerationType : uint8
{
    CENTROID        UMETA(DisplayName = "Centroid"),
    UNIFORM_FLOOR   UMETA(DisplayName = "Uniform Floor"),
};


// ---------------------------------------------------------------------------------------------------------------------
// FSteamAudioBakedDataInfo
// ---------------------------------------------------------------------------------------------------------------------

/**
 * Information about a single layer of baked data in a probe batch.
 */
USTRUCT()
struct FSteamAudioBakedDataInfo
{
    GENERATED_USTRUCT_BODY()

    /** Name of the actor associated with this layer (or "Reverb" if this layer is reverb). */
    UPROPERTY()
    FString Name;

    /** See IPLBakedDataIdentifier. */
    UPROPERTY()
    int Type = static_cast<int>(IPL_BAKEDDATATYPE_REFLECTIONS);

    /** See IPLBakedDataIdentifier. */
    UPROPERTY()
    int Variation = static_cast<int>(IPL_BAKEDDATAVARIATION_REVERB);

    /** See IPLBakedDataIdentifier. */
    UPROPERTY()
    FVector EndpointCenter{0.0f, 0.0f, 0.0f};

    /** See IPLBakedDataIdentifier. */
    UPROPERTY()
    float EndpointRadius = 0.0f;

    /** Size (in bytes) of the baked data in this layer. */
    UPROPERTY()
    int Size = 0;
};


// ---------------------------------------------------------------------------------------------------------------------
// ASteamAudioProbeVolume
// ---------------------------------------------------------------------------------------------------------------------

/**
 * A volume in which Steam Audio probes can be placed for baking reflections, reverb, or pathing data.
 */
UCLASS(ClassGroup = (SteamAudio), HideCategories = (Activation, Collision, Cooking), meta = (BlueprintSpawnableComponent))
class STEAMAUDIO_API ASteamAudioProbeVolume : public AVolume
{
    GENERATED_BODY()

public:
    /** Reference to the Steam Audio Serialized Object asset containing probe data. */
    UPROPERTY(VisibleAnywhere, Category = ProbeBatchSettings, meta = (AllowedClasses = "/Script/SteamAudio.SteamAudioSerializedObject"))
    FSoftObjectPath Asset;

    /** How to place probes within the volume, */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ProbeBatchSettings)
    EProbeGenerationType GenerationType;

    /** Horizontal spacing (in meters) between adjacent probes. Only when using uniform floor probe generation. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ProbeBatchSettings)
    float HorizontalSpacing;

    /** Height above the floor (in meters) at which to place probes. Only when using uniform floor probe generation. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ProbeBatchSettings)
    float HeightAboveFloor;

    /** Number of probes generated. */
    UPROPERTY(VisibleAnywhere, Category = ProbeBatchSettings, meta = (DisplayName = "Probes"))
    int32 NumProbes;

    /** Size (in bytes) of the probe data. */
    UPROPERTY(VisibleAnywhere, Category = ProbeBatchSettings)
    int32 DataSize;

    /** Detailed size information about each layer of baked data in this probe volume. */
    UPROPERTY(VisibleAnywhere, Category = ProbeBatchSettings)
    TArray<FSteamAudioBakedDataInfo> DetailedStats;

    /** Component containing probe data for in-editor visualization. */
    UPROPERTY()
    USteamAudioProbeComponent* ProbeComponent;

    ASteamAudioProbeVolume();

    /**
     * Inherited from AActor
     */

#if WITH_EDITOR
    /** Used to disable UI controls based on the values of other UI controls. */
    virtual bool CanEditChange(const FProperty* InProperty) const override;
#endif

    IPLProbeBatch GetProbeBatch() { return ProbeBatch; }

    /** Generates probes. */
    bool GenerateProbes(ASteamAudioStaticMeshActor* StaticMeshActor, FString AssetName);

    /** Sets the total size of baked data (for stats display). */
    void UpdateTotalSize(int Size);

    /** Resets detailed stats. Called when probes are generated. */
    void ResetLayers();

    /** Removes stats for the given layer. Called when the layer is deleted. */
    void RemoveLayer(const FString& Name);

    /** Adds (if missing) or updates stats for the given layer. Called when the layer is baked. */
    void AddOrUpdateLayer(const FString& Name, IPLBakedDataIdentifier& Identifier, int Size);

    /** Adds stats for the given layer. */
    void AddLayer(const FString& Name, IPLBakedDataIdentifier& Identifier, int Size);

    /** Updates stats for the given layer. */
    void UpdateLayer(const FString& Name, int Size);

    /** Returns the index of the given layer in the stats array. */
    int FindLayer(const FString& Name);

protected:
    /**
     * Inherited from AActor
     */

    /** Called when the actor has been initialized. */
    virtual void BeginPlay() override;

    /** Called when the actor is going to be destroyed. */
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    /** Retained reference to the Steam Audio simulator. */
    IPLSimulator Simulator;

    /** The Probe Batch object. */
    IPLProbeBatch ProbeBatch;
};
