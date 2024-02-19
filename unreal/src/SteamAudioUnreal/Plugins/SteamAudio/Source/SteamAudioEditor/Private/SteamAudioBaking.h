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

#include "SteamAudioEditorModule.h"

class UWorld;
class ULevel;
class USteamAudioBakedSourceComponent;
class USteamAudioBakedListenerComponent;
class ASteamAudioProbeVolume;


namespace SteamAudio {

// ---------------------------------------------------------------------------------------------------------------------
// FBakeTask
// ---------------------------------------------------------------------------------------------------------------------

enum class EBakeTaskType
{
    STATIC_SOURCE_REFLECTIONS,
    STATIC_LISTENER_REFLECTIONS,
    REVERB,
    PATHING
};

enum class EBakeResult
{
    FAILURE,
    PARTIAL_SUCCESS,
    SUCCESS,
};

struct FBakeTask
{
    EBakeTaskType Type;
    USteamAudioBakedSourceComponent* BakedSource;
    USteamAudioBakedListenerComponent* BakedListener;
    ASteamAudioProbeVolume* PathingProbeVolume;

    FString GetLayerName() const;
};


// ---------------------------------------------------------------------------------------------------------------------
// Baking
// ---------------------------------------------------------------------------------------------------------------------

#if WITH_EDITOR

extern std::atomic<bool> GIsBaking;

DECLARE_DELEGATE(FSteamAudioBakeComplete);

/** Runs one or more bakes for a level. */
void STEAMAUDIOEDITOR_API Bake(UWorld* World, ULevel* Level, const TArray<FBakeTask>& Tasks,
    FSteamAudioBakeComplete OnBakeComplete = FSteamAudioBakeComplete());

#endif

}
