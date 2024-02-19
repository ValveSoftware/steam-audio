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
#include "SteamAudioListenerComponent.generated.h"

class APlayerController;
class USteamAudioBakedListenerComponent;

// ---------------------------------------------------------------------------------------------------------------------
// Enumerations
// ---------------------------------------------------------------------------------------------------------------------

/**
 * Ways in which reverb can be simulated for a listener.
 */
UENUM(BlueprintType)
enum class EReverbSimulationType : uint8
{
	REALTIME    UMETA(DisplayName = "Real-time"),
	BAKED       UMETA(DisplayName = "Baked"),
};


// ---------------------------------------------------------------------------------------------------------------------
// USteamAudioListenerComponent
// ---------------------------------------------------------------------------------------------------------------------

/**
 * Specifies Steam Audio simulation settings to use for listener-centric effects.
 */
UCLASS(ClassGroup = (SteamAudio), meta = (BlueprintSpawnableComponent), HideCategories = (Activation, Collision, Tags, Rendering, Physics, LOD, Mobility, Cooking, AssetUserData))
class STEAMAUDIO_API USteamAudioListenerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** Reference to the current baked listener to use for sources that are configured to use baked static listener reflections. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BakedListenerSettings)
    TSoftObjectPtr<AActor> CurrentBakedListener;

	/** If true, listener-centric reverb will be simulated. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ReverbSettings)
	bool bSimulateReverb;

	/** How to simulate listener-centric reverb. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ReverbSettings)
	EReverbSimulationType ReverbType;

	USteamAudioListenerComponent();

    /** Sets simulation inputs. */
	void SetInputs();

    /** Retrieves simulation outputs. */
	IPLSimulationOutputs GetOutputs();

    /** Updates component properties based on simulation outputs. */
	void UpdateOutputs();

    /** Returns the baked data identifier for this source. */
    IPLBakedDataIdentifier GetBakedDataIdentifier() const;

	/**
	 * Inherited from UActorComponent
	 */

#if WITH_EDITOR
     /** Used to disable UI controls based on the values of other UI controls. */
	virtual bool CanEditChange(const FProperty* InProperty) const override;
#endif

	/** Returns the current (active) listener. */
	static USteamAudioListenerComponent* GetCurrentListener();

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

	/** The current player controller. This drives the listener position and orientation. */
	APlayerController* PlayerController;

	/** The current listener. */
	static USteamAudioListenerComponent* CurrentListener;
};
