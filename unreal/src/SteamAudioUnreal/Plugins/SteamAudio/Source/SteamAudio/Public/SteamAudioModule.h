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

#include <atomic>

#include <phonon.h>

#include "Runtime/Launch/Resources/Version.h"
#if ((ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 0) || (ENGINE_MAJOR_VERSION > 5))
#include "CoreFwd.h"
#endif
#include "CoreMinimal.h"
#include "IAudioExtensionPlugin.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSteamAudio, Log, All);

namespace SteamAudio {

class FSteamAudioManager;
class FSteamAudioSpatializationPluginFactory;
class FSteamAudioOcclusionPluginFactory;
class FSteamAudioReverbPluginFactory;
class IAudioEngineState;


// ---------------------------------------------------------------------------------------------------------------------
// IAudioEngineStateFactory
// ---------------------------------------------------------------------------------------------------------------------

/**
 * Any module that implements this interface can be used to communicate to some audio engine.
 */
class IAudioEngineStateFactory : public IModuleInterface
{
public:
    /** Create the object that we can use to communicate to the audio engine supported by this module. */
    virtual TSharedPtr<IAudioEngineState> CreateAudioEngineState() = 0;
};


// ---------------------------------------------------------------------------------------------------------------------
// FSteamAudioModule
// ---------------------------------------------------------------------------------------------------------------------

/**
 * Singleton class that contains all the global state related to the Steam Audio runtime module.
 */
class FSteamAudioModule : public IAudioEngineStateFactory
{
public:
    /**
     * Inherited from IModuleInterface
     */

    /** Called when the module is being loaded. */
    virtual void StartupModule() override;

    /** Called when the module is being unloaded. */
    virtual void ShutdownModule() override;

    /** Returns a factory object that can be used to instantiate a plugin of the given type. */
    IAudioPluginFactory* GetPluginFactory(EAudioPlugin PluginType);

    /** Registers an audio device as being used for rendering. */
    void RegisterAudioDevice(FAudioDevice* AudioDevice);

    /** Unregisters an audio device from being used for rendering. */
    void UnregisterAudioDevice(FAudioDevice* AudioDevice);

    /** Create an object that we can use to communicate with Unreal's built-in audio engine. */
    virtual TSharedPtr<IAudioEngineState> CreateAudioEngineState() override;

    /** Returns the module singleton object. */
    static FSteamAudioModule& Get() { return FModuleManager::GetModuleChecked<FSteamAudioModule>("SteamAudio"); }

    /** Returns the manager singleton object, which is in turn owned by the module singleton. */
    static FSteamAudioManager& GetManager() { return *Get().Manager; }

    /** Returns true if we're currently playing (i.e., in a standalone game or in play-in-editor mode. */
    static bool IsPlaying();

    /** Returns the audio engine interface. */
    static IAudioEngineState* GetAudioEngineState();

    /** Sets the audio engine interface. This should be called by audio-engine-specific plugins upon module startup. */
    static STEAMAUDIO_API void SetAudioEngineState(TSharedPtr<IAudioEngineState> State);

private:
    /** Handle to the Steam Audio dynamic library (phonon.dll or similar). */
    void* Library;

    /** Manager object that maintains global Steam Audio state. */
    TSharedPtr<FSteamAudioManager> Manager;

    /** Audio devices being used for rendering. */
    TArray<FAudioDevice*> AudioDevices;

    /** Factory object used to instantiate the spatialization plugin. */
    TUniquePtr<FSteamAudioSpatializationPluginFactory> SpatializationPluginFactory;

    /** Factory object used to instantiate the occlusion plugin. */
    TUniquePtr<FSteamAudioOcclusionPluginFactory> OcclusionPluginFactory;

	/** Factory object used to instantiate the reverb plugin. */
	TUniquePtr<FSteamAudioReverbPluginFactory> ReverbPluginFactory;

    /** Incremented once for each game or PIE session. */
    static TAtomic<int> PIEInitCount;

    static FCriticalSection PIEInitCountMutex;

    /** Called when the game is initialized (only in standalone builds). */
    void OnEngineLoopInitComplete();

    /** Called when the game is shut down (only in standalone builds). */
    void OnEnginePreExit();

#if WITH_EDITOR
    /** Called when PIE mode starts (only in editor builds). */
    void OnPIEStarted(bool bSimulating);

    /** Called when PIE mode ends (only in editor builds). */
    void OnEndPIE(bool bSimulating);
#endif
};

}
