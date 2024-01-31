//
// Copyright 2017-2023 Valve Corporation.
//

#pragma once

#include "SteamAudioAudioEngineInterface.h"

namespace SteamAudio {

// ---------------------------------------------------------------------------------------------------------------------
// FUnrealAudioEngineState
// ---------------------------------------------------------------------------------------------------------------------

/**
 * Communicates between the game engine plugin and the audio engine plugin for Unreal's built-in audio engine.
 */
class FUnrealAudioEngineState : public IAudioEngineState
{
public:
    /**
     * Inherited from IAudioEngineState
     */

    /** Does nothing. */
    virtual void Initialize(IPLContext Context, IPLHRTF HRTF, const IPLSimulationSettings& SimulationSettings) override;

    /** Does nothing. */
    virtual void Destroy() override;

    /** Does nothing. */
    virtual void SetHRTF(IPLHRTF HRTF) override;

    /** Specifies the simulation source to use for reverb. Call in BeginPlay for the Steam Audio Listener. */
    virtual void SetReverbSource(IPLSource Source) override;

    /** Returns the listener transform used by the Unreal audio device. */
    virtual FTransform GetListenerTransform() override;

    /** Returns the audio settings from the current Unreal audio device. */
    virtual IPLAudioSettings GetAudioSettings() override;

    /** Creates an interface object for communicating with a spatializer effect instance in the audio engine plugin. */
    virtual TSharedPtr<IAudioEngineSource> CreateAudioEngineSource() override;
};


// ---------------------------------------------------------------------------------------------------------------------
// FUnrealAudioEngineSource
// ---------------------------------------------------------------------------------------------------------------------

/**
 * Communicates between the game engine plugin and the spatializer effect for Unreal's built-in audio engine.
 */
class FUnrealAudioEngineSource : public IAudioEngineSource
{
public:
    /**
     * Inherited from IAudioEngineSource
     */

    /** Does nothing. */
    virtual void Initialize(AActor* Actor) override;

    /** Does nothing. */
    virtual void Destroy() override;

    /** Does nothing. */
    virtual void UpdateParameters(USteamAudioSourceComponent* Source) override;
};

}
