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
