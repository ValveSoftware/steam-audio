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

#include <AK/SoundEngine/Common/AkTypes.h>

#include "AkComponent.h"
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "SteamAudioAudioEngineInterface.h"

typedef struct {
    IPLfloat32 MetersPerUnit;
} IPLWwiseSettings;

namespace SteamAudio {

// ---------------------------------------------------------------------------------------------------------------------
// FSteamAudioWwiseModule
// ---------------------------------------------------------------------------------------------------------------------

typedef void (AKSOUNDENGINE_CALL* iplWwiseGetVersion_t)(unsigned int* Major, unsigned int* Minor, unsigned int* Patch);
typedef void (AKSOUNDENGINE_CALL* iplWwiseInitialize_t)(IPLContext Context, IPLWwiseSettings* Settings);
typedef void (AKSOUNDENGINE_CALL* iplWwiseTerminate_t)();
typedef void (AKSOUNDENGINE_CALL* iplWwiseSetHRTF_t)(IPLHRTF HRTF);
typedef void (AKSOUNDENGINE_CALL* iplWwiseSetSimulationSettings_t)(IPLSimulationSettings SimulationSettings);
typedef void (AKSOUNDENGINE_CALL* iplWwiseSetReverbSource_t)(IPLSource ReverbSource);
typedef void (AKSOUNDENGINE_CALL* iplWwiseAddSource_t)(AkGameObjectID GameObjectID, IPLSource Source);
typedef void (AKSOUNDENGINE_CALL* iplWwiseRemoveSource_t)(AkGameObjectID GameObjectID);

class FSteamAudioWwiseModule : public IAudioEngineStateFactory
{
public:
    /** Handle to the Steam Audio Wwise plugin (SteamAudioWwise.dll or similar). */
    void* Library;

    /** Function pointers for the game engine / audio engine communication API. */
    iplWwiseGetVersion_t iplWwiseGetVersion;
    iplWwiseInitialize_t iplWwiseInitialize;
    iplWwiseTerminate_t iplWwiseTerminate;
    iplWwiseSetHRTF_t iplWwiseSetHRTF;
    iplWwiseSetSimulationSettings_t iplWwiseSetSimulationSettings;
    iplWwiseSetReverbSource_t iplWwiseSetReverbSource;
    iplWwiseAddSource_t iplWwiseAddSource;
    iplWwiseRemoveSource_t iplWwiseRemoveSource;

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
    static FSteamAudioWwiseModule& Get() { return FModuleManager::GetModuleChecked<FSteamAudioWwiseModule>("SteamAudioWwise"); }
private:
    /** Returns the absolute path for the dynamic library. */
    FString GetDynamicLibraryPath(FString LibName);
};


// ---------------------------------------------------------------------------------------------------------------------
// FWwiseStudioAudioEngineState
// ---------------------------------------------------------------------------------------------------------------------

/**
 * Communicates between the game engine plugin and Wwise's audio engine.
 */
class FWwiseAudioEngineState : public IAudioEngineState
{
public:
    FWwiseAudioEngineState();

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

    /** Returns the audio settings for Wwise. */
    virtual IPLAudioSettings GetAudioSettings() override;

    /** Creates an interface object for communicating with a spatializer effect instance in the audio engine plugin. */
    virtual TSharedPtr<IAudioEngineSource> CreateAudioEngineSource() override;

private:
    /** Converts a vector from Wwise's coordinate system to Unreal's coordinate system. */
    FVector ConvertVectorFromWwise(const AkVector& WwiseVector);
};


// ---------------------------------------------------------------------------------------------------------------------
// FWwiseAudioEngineSource
// ---------------------------------------------------------------------------------------------------------------------

/**
 * Communicates between the game engine plugin and the spatialize effect on a single Wwise event instance.
 */
class FWwiseAudioEngineSource : public IAudioEngineSource
{
public:
    FWwiseAudioEngineSource();

    /**
     * Inherited from IAudioEngineState
     */

    /** Initializes communication with the spatializer effect associated with the given actor. */
    virtual void Initialize(AActor* Actor) override;

    /** Shuts down communication. */
    virtual void Destroy() override;

    /** Sends simulation parameters from the given source component to the spatializer effect instance. */
    virtual void UpdateParameters(USteamAudioSourceComponent* SteamAudioSourceComponent) override;

private:
    /** The Wwise AkComponent corresponding to this source. */
    UAkComponent* AkComponent;

    /** The Wwise GameObjectID for this source. */
    AkGameObjectID GameObjectID;

    /** The Steam Audio Source component corresponding to this source. */
    USteamAudioSourceComponent* SourceComponent;
};

}
