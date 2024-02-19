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
#include "Sound/SoundEffectSubmix.h"
#include "Sound/SoundEffectPreset.h"
#include "SteamAudioReverb.generated.h"

namespace SteamAudio {

// ---------------------------------------------------------------------------------------------------------------------
// FSteamAudioReverbSource
// ---------------------------------------------------------------------------------------------------------------------

/**
 * Rendering state for a single reverb voice.
 */
struct FSteamAudioReverbSource
{
	FSteamAudioReverbSource();

	~FSteamAudioReverbSource();

	bool bApplyReflections;
	bool bApplyHRTFToReflections;
	float ReflectionsMixLevel;

	/** Retained reference to the HRTF. */
	IPLHRTF HRTF;

	/** Used when bApplyReflections is true. */
	IPLReflectionEffect ReflectionEffect;

    /** Used when bApplyReflections is true. */
	IPLAmbisonicsDecodeEffect AmbisonicsDecodeEffect;

	/** Deinterleaved input buffer. */
	IPLAudioBuffer InBuffer;

	/** Downmixed input buffer. */
	IPLAudioBuffer MonoBuffer;

	/** Ambisonic buffer with reflections applied. */
	IPLAudioBuffer IndirectBuffer;

	/** Spatialized reflections for output. */
	IPLAudioBuffer OutBuffer;

    IPLReflectionEffectType PrevReflectionEffectType;
    float PrevDuration;
    int PrevOrder;

	void Reset();

	void ClearBuffers();
};


// ---------------------------------------------------------------------------------------------------------------------
// FSteamAudioReverbPlugin
// ---------------------------------------------------------------------------------------------------------------------

/**
 * Singleton object containing shared state for the reverb plugin.
 */
class FSteamAudioReverbPlugin : public IAudioReverb
{
public:
	FSteamAudioReverbPlugin();

	~FSteamAudioReverbPlugin();

	/**
	 * Inherited from IAudioReverb.
	 */

    /** Called to initialize the plugin. */
	virtual void Initialize(const FAudioPluginInitializationParams InitializationParams) override;

    /** Called when a given source voice is assigned for rendering a given Audio Component. */
	virtual void OnInitSource(const uint32 SourceId, const FName& AudioComponentUserId, const uint32 NumChannels, UReverbPluginSourceSettingsBase* InSettings) override;

    /** Called when a given source voice will no longer be used to render an Audio Component. */
	virtual void OnReleaseSource(const uint32 SourceId) override;

	/** Returns the submix plugin effect. */
	virtual FSoundEffectSubmixPtr GetEffectSubmix() override;

	/** Returns the submix node used by the submix plugin. */
	virtual USoundSubmix* GetSubmix() override;

    /** Called to process a single source. */
	virtual void ProcessSourceAudio(const FAudioPluginSourceInputData& InputData, FAudioPluginSourceOutputData& OutputData) override;

	IPLAudioSettings GetAudioSettings() { return AudioSettings; }
	IPLReflectionMixer GetReflectionMixer() { return ReflectionMixer; }

	/** Ensures that the reflection mixer is initialized. */
	void LazyInitMixer();

	/** Destroys the reflection mixer. */
	void ShutDownMixer();

private:
    /** Audio pipeline settings. */
	IPLAudioSettings AudioSettings;

    /** Lazy-initialized state for as many sources as we can render simultaneously. */
	TArray<FSteamAudioReverbSource> Sources;

	/** The submix node containing the submix plugin. */
	TWeakObjectPtr<USoundSubmix> ReverbSubmix;

	/** The submix plugin. */
	FSoundEffectSubmixPtr ReverbSubmixEffect;

	/** The reflection mixer object used by the submix plugin. */
	IPLReflectionMixer ReflectionMixer;

	IPLReflectionEffectType PrevReflectionEffectType;
	float PrevDuration;
	int PrevOrder;
};


// ---------------------------------------------------------------------------------------------------------------------
// FSteamAudioReverbPluginFactory
// ---------------------------------------------------------------------------------------------------------------------

/**
 * Provides metadata about the reverb plugin, and a factory method for instantiating it.
 */
class FSteamAudioReverbPluginFactory : public IAudioReverbFactory
{
public:
	/**
	 * Inherited from IAudioPluginFactory
	 */

    /** Returns the name that should be shown in the platform settings. */
	virtual FString GetDisplayName() override;

    /** Returns true if the plugin supports the given platform. */
	virtual bool SupportsPlatform(const FString& PlatformName) override;

	/**
	 * Inherited from IAudioOcclusionFactory
	 */

    /** Returns the class object for the reverb settings data. */
	virtual UClass* GetCustomReverbSettingsClass() const override;

    /** Instantiates the reverb plugin. */
	virtual TAudioReverbPtr CreateNewReverbPlugin(FAudioDevice* OwningDevice) override;
};

}


// ---------------------------------------------------------------------------------------------------------------------
// FSteamAudioReverbSubmixPlugin
// ---------------------------------------------------------------------------------------------------------------------

/**
 * A submix plugin that optionally a) applies listener-centric reverb to its input, and/or b) adds mixed
 * source-centric reflections into its output.
 */
class FSteamAudioReverbSubmixPlugin : public FSoundEffectSubmix
{
public:
	FSteamAudioReverbSubmixPlugin();

	~FSteamAudioReverbSubmixPlugin();

	/** Returns the number of channels to use for input and output. */
	virtual uint32 GetDesiredInputChannelCountOverride() const override;

	/** Processes the audio flowing through the submix. */
	virtual void OnProcessAudio(const FSoundEffectSubmixInputData& InData, FSoundEffectSubmixOutputData& OutData) override;

	/** Called to specify the singleton reverb plugin instance. */
	void SetReverbPlugin(SteamAudio::FSteamAudioReverbPlugin* Plugin);

	/** Returns the Steam Audio simulation source used for listener-centric reverb. */
	static IPLSource GetReverbSource();

	/** Sets the Steam Audio simulation source used for listener-centric reverb. */
	static void SetReverbSource(IPLSource Source);

private:
	/** The singleton reverb plugin. */
	SteamAudio::FSteamAudioReverbPlugin* ReverbPlugin;

	/** Retained reference to the Steam Audio context. */
	IPLContext Context;

	/** Retained reference to the HRTF. */
	IPLHRTF HRTF;

	/** Used for rendering reverb. */
	IPLReflectionEffect ReflectionEffect;

    /** Used for rendering reverb. */
    IPLAmbisonicsDecodeEffect AmbisonicsDecodeEffect;

	/** Deinterleaved input buffer. */
	IPLAudioBuffer InBuffer;

	/** Downmixed input buffer. */
	IPLAudioBuffer MonoBuffer;

	/** Buffer containing Ambisonic reverb. */
	IPLAudioBuffer ReverbBuffer;

	/** Buffer containing Ambisonic source-centric reflections. */
	IPLAudioBuffer IndirectBuffer;

	/** Spatialized output buffer. */
	IPLAudioBuffer OutBuffer;

    IPLReflectionEffectType PrevReflectionEffectType;
    float PrevDuration;
    int PrevOrder;

	/** Double-buffered reference to the Steam Audio simulation source. */
	static IPLSource ReverbSource[2];

	/** True if the double buffers need to be swapped. */
	static std::atomic<bool> bNewReverbSourceWritten;

	/** Ensures that the Steam Audio effects are initialized. */
	void LazyInit();

	/** Destroys Steam Audio effects. */
	void ShutDown();

	void Reset();

	void ClearBuffers();
};


// ---------------------------------------------------------------------------------------------------------------------
// FSteamAudioReverbSubmixPluginSettings
// ---------------------------------------------------------------------------------------------------------------------

/**
 * Actual settings for the submix plugin.
 */
USTRUCT(BlueprintType)
struct FSteamAudioReverbSubmixPluginSettings
{
	GENERATED_USTRUCT_BODY()

	/** If true, listener-centric reverb will be applied to the audio received as input to this submix. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SubmixSettings)
	bool bApplyReverb;

	/** If true, the reverb and mixed reflections will be rendered using binaural rendering. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SubmixSettings)
	bool bApplyHRTF;

	FSteamAudioReverbSubmixPluginSettings();
};


// ---------------------------------------------------------------------------------------------------------------------
// USteamAudioReverbSubmixPluginPreset
// ---------------------------------------------------------------------------------------------------------------------

/**
 * Settings object for the submix plugin.
 */
UCLASS()
class USteamAudioReverbSubmixPluginPreset : public USoundEffectSubmixPreset
{
	GENERATED_BODY()

public:
	EFFECT_PRESET_METHODS(SteamAudioReverbSubmixPlugin);

	UPROPERTY(EditAnywhere, Category = SubmixPreset)
	FSteamAudioReverbSubmixPluginSettings Settings;
};
