//
// Copyright 2017-2023 Valve Corporation.
//

#pragma once

#include "SteamAudioModule.h"

class USteamAudioSourceComponent;

namespace SteamAudio {

class IAudioEngineSource;

// ---------------------------------------------------------------------------------------------------------------------
// IAudioEngineState
// ---------------------------------------------------------------------------------------------------------------------

/**
 * Interface for communicating between the game engine plugin and the audio engine plugin.
 */
class IAudioEngineState
{
public:
    /** Initializes the audio engine part of the plugin. Call after the Steam Audio Manager is initialized. */
    virtual void Initialize(IPLContext Context, IPLHRTF HRTF, const IPLSimulationSettings& SimulationSettings) = 0;

    /** Shuts down the audio engine part of the plugin. Call before shutting down the Steam Audio Manager. */
    virtual void Destroy() = 0;

    /** Specifies the HRTF to use for rendering. Call right after Initialize(). */
    virtual void SetHRTF(IPLHRTF HRTF) = 0;

    /** Specifies the simulation source to use for reverb. Call in BeginPlay for the Steam Audio Listener. */
    virtual void SetReverbSource(IPLSource Source) = 0;

    /** Retrieves the current listener transform from the audio engine plugin. */
    virtual FTransform GetListenerTransform() = 0;

    /** Retrieves the audio settings for the audio engine. Call this during manager init so the HRTF can be loaded. */
    virtual IPLAudioSettings GetAudioSettings() = 0;

    /** Creates an interface object for communicating with a spatializer effect instance in the audio engine plugin. */
    virtual TSharedPtr<IAudioEngineSource> CreateAudioEngineSource() = 0;
};


// ---------------------------------------------------------------------------------------------------------------------
// IAudioEngineSource
// ---------------------------------------------------------------------------------------------------------------------

/**
 * Interface for communicating between a source component in the game engine plugin, and a spatializer effect instance
 * in the audio engine plugin.
 */
class IAudioEngineSource
{
public:
    /** Initializes communication with whatever spatializer effect is associated with the given actor. */
    virtual void Initialize(AActor* Actor) = 0;

    /** Shuts down communication. */
    virtual void Destroy() = 0;

    /** Sends simulation parameters from the given source component to the spatializer effect instance. */
    virtual void UpdateParameters(USteamAudioSourceComponent* Source) = 0;
};

}
