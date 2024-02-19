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

#include <fmod.hpp>
#include <fmod_studio.hpp>

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "SteamAudioAudioEngineInterface.h"

class UFMODAudioComponent;

namespace SteamAudio {

// ---------------------------------------------------------------------------------------------------------------------
// FSteamAudioFMODStudioModule
// ---------------------------------------------------------------------------------------------------------------------

typedef void (F_CALL* iplFMODGetVersion_t)(unsigned int* Major, unsigned int* Minor, unsigned int* Patch);
typedef void (F_CALL* iplFMODInitialize_t)(IPLContext Context);
typedef void (F_CALL* iplFMODTerminate_t)();
typedef void (F_CALL* iplFMODSetHRTF_t)(IPLHRTF HRTF);
typedef void (F_CALL* iplFMODSetSimulationSettings_t)(IPLSimulationSettings SimulationSettings);
typedef void (F_CALL* iplFMODSetReverbSource_t)(IPLSource ReverbSource);
typedef IPLint32 (F_CALL* iplFMODAddSource_t)(IPLSource Source);
typedef void (F_CALL* iplFMODRemoveSource_t)(IPLint32 Handle);

class FSteamAudioFMODStudioModule : public IAudioEngineStateFactory
{
public:
    /** Handle to the Steam Audio FMOD Studio plugin (phonon_fmod.dll or similar). */
    void* Library;

    /** Function pointers for the game engine / audio engine communication API. */
    iplFMODGetVersion_t iplFMODGetVersion;
    iplFMODInitialize_t iplFMODInitialize;
    iplFMODTerminate_t iplFMODTerminate;
    iplFMODSetHRTF_t iplFMODSetHRTF;
    iplFMODSetSimulationSettings_t iplFMODSetSimulationSettings;
    iplFMODSetReverbSource_t iplFMODSetReverbSource;
    iplFMODAddSource_t iplFMODAddSource;
    iplFMODRemoveSource_t iplFMODRemoveSource;

    /**
     * Inherited from IModuleInterface
     */

    /** Called when the module is being loaded. */
    virtual void StartupModule() override;

    /** Called when the module is being unloaded. */
    virtual void ShutdownModule() override;

    /** Create an object that we can use to communicate with FMOD Studio. */
    virtual TSharedPtr<IAudioEngineState> CreateAudioEngineState() override;

    /** Returns the module singleton object. */
    static FSteamAudioFMODStudioModule& Get() { return FModuleManager::GetModuleChecked<FSteamAudioFMODStudioModule>("SteamAudioFMODStudio"); }
};


// ---------------------------------------------------------------------------------------------------------------------
// FFMODStudioAudioEngineState
// ---------------------------------------------------------------------------------------------------------------------

/**
 * Communicates between the game engine plugin and FMOD Studio's audio engine.
 */
class FFMODStudioAudioEngineState : public IAudioEngineState
{
public:
    FFMODStudioAudioEngineState();

    /**
     * Inherited from IAudioEngineState
     */

    /** Initializes the Steam Audio FMOD Studio plugin. */
    virtual void Initialize(IPLContext Context, IPLHRTF HRTF, const IPLSimulationSettings& SimulationSettings) override;

    /** Shuts down the Steam Audio FMOD Studio plugin. */
    virtual void Destroy() override;

    /** Does nothing. */
    virtual void SetHRTF(IPLHRTF HRTF) override;

    /** Specifies the simulation source to use for reverb. */
    virtual void SetReverbSource(IPLSource Source) override;

    /** Returns the transform of the current player controller. */
    virtual FTransform GetListenerTransform() override;

    /** Returns the audio settings for FMOD Studio. */
    virtual IPLAudioSettings GetAudioSettings() override;

    /** Creates an interface object for communicating with a spatializer effect instance in the audio engine plugin. */
    virtual TSharedPtr<IAudioEngineSource> CreateAudioEngineSource() override;

    /** Returns the FMOD core system instance. */
    FMOD::System* GetSystem();

private:
    /** The FMOD Studio system. */
    FMOD::Studio::System* StudioSystem;

    /** The FMOD core system. */
    FMOD::System* CoreSystem;

    /** Converts a vector from FMOD Studio's coordinate system to Unreal's coordinate system. */
    FVector ConvertVectorFromFMODStudio(const FMOD_VECTOR& FMODStudioVector);
};


// ---------------------------------------------------------------------------------------------------------------------
// FFMODStudioAudioEngineSource
// ---------------------------------------------------------------------------------------------------------------------

/**
 * Communicates between the game engine plugin and the spatialize effect on a single FMOD event instance.
 */
class FFMODStudioAudioEngineSource : public IAudioEngineSource
{
public:
    FFMODStudioAudioEngineSource();

    /**
     * Inherited from IAudioEngineState
     */

    /** Initializes communication with the spatializer effect associated with the given actor. */
    virtual void Initialize(AActor* Actor) override;

    /** Shuts down communication. */
    virtual void Destroy() override;

    /** Sends simulation parameters from the given source component to the spatializer effect instance. */
    virtual void UpdateParameters(USteamAudioSourceComponent* SteamAudioSourceComponent) override;

    /** Returns the FMOD DSP corresponding to the spatializer effect with which we're communicating. */
    FMOD::DSP* GetDSP();

private:
    /** The FMOD Audio component corresponding to this source. */
    UFMODAudioComponent* FMODAudioComponent;

    /** The DSP effect we want to communicate with. */
    FMOD::DSP* DSP;

    /** The Steam Audio Source component corresponding to this source. */
    USteamAudioSourceComponent* SourceComponent;

    /** The handle of the Steam Audio Source, obtained via iplFMODAddSource. */
    int Handle;
};

}
