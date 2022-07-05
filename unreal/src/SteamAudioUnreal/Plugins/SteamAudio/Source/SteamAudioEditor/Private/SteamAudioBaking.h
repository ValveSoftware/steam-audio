//
// Copyright (C) Valve Corporation. All rights reserved.
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
