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
