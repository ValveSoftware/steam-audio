//
// Copyright (C) Valve Corporation. All rights reserved.
//

#pragma once

#include "SteamAudioModule.h"
#include "Components/ActorComponent.h"
#include "SteamAudioBakedSourceComponent.generated.h"

// ---------------------------------------------------------------------------------------------------------------------
// USteamAudioBakedSourceComponent
// ---------------------------------------------------------------------------------------------------------------------

/**
 * A static source position from which reflections can be baked.
 */
UCLASS(ClassGroup = (SteamAudio), HideCategories = (Activation, Collision, Cooking), meta = (BlueprintSpawnableComponent))
class STEAMAUDIO_API USteamAudioBakedSourceComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    /** Radius within which a probe must lie in order for reflections to be baked. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BakedSourceSettings, meta = (UIMin = 0.0f, UIMax = 1000.0f))
    float InfluenceRadius;

    USteamAudioBakedSourceComponent();
};
