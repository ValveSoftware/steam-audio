//
// Copyright 2017-2023 Valve Corporation.
//

#pragma once

#include "SteamAudioModule.h"
#include "Components/ActorComponent.h"
#include "SteamAudioBakedListenerComponent.generated.h"

// ---------------------------------------------------------------------------------------------------------------------
// USteamAudioBakedListenerComponent
// ---------------------------------------------------------------------------------------------------------------------

/**
 * A virtual listener to which reflections can be baked.
 */
UCLASS(ClassGroup = (SteamAudio), HideCategories = (Activation, Collision, Cooking), meta = (BlueprintSpawnableComponent))
class STEAMAUDIO_API USteamAudioBakedListenerComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    /** Distance within which a probe must lie in order for reflections to be baked. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BakedListenerSettings, meta = (UIMin = 0.0f, UIMax = 1000.0f))
    float InfluenceRadius;

    USteamAudioBakedListenerComponent();
};
