//
// Copyright 2017-2023 Valve Corporation.
//

#pragma once

#include "SteamAudioModule.h"
#include "Components/ActorComponent.h"
#include "SteamAudioProbeComponent.generated.h"

// ---------------------------------------------------------------------------------------------------------------------
// USteamAudioProbeComponent
// ---------------------------------------------------------------------------------------------------------------------

/**
 * Component of ASteamAudioProbeVolume that stores the probe positions for in-editor visualization.
 */
UCLASS(ClassGroup = (Audio), HideCategories = (Activation, Collision))
class STEAMAUDIO_API USteamAudioProbeComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    /** The probe positions. */
    UPROPERTY()
    TArray<FVector> ProbePositions;

    FCriticalSection ProbePositionsCriticalSection;
};
