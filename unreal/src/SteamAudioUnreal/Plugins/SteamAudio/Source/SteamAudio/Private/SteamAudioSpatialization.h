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
#include "SteamAudioSpatializationSettings.h"

namespace SteamAudio {

// ---------------------------------------------------------------------------------------------------------------------
// FSteamAudioSpatializationSource
// ---------------------------------------------------------------------------------------------------------------------

/**
 * Rendering state for a single spatialized source voice.
 */
struct FSteamAudioSpatializationSource
{
    FSteamAudioSpatializationSource();

    ~FSteamAudioSpatializationSource();

    bool bBinaural;
    EHRTFInterpolation Interpolation;
    bool bApplyPathing;
    bool bApplyHRTFToPathing;
    float PathingMixLevel;

    /** Retained reference to the HRTF. */
    IPLHRTF HRTF;

    /** Used when bBinaural is false. */
    IPLPanningEffect PanningEffect;

    /** Used when bBinaural is true. */
    IPLBinauralEffect BinauralEffect;

    /** Used when bApplyPathing is true. */
    IPLPathEffect PathEffect;

    /** Used when bApplyPathing is true. */
    IPLAmbisonicsDecodeEffect AmbisonicsDecodeEffect;

    /** Used to apply a send level to the pathing effect. */
    IPLAudioBuffer PathingInputBuffer;

    /** Ambisonic buffer containing the output of the pathing effect. */
    IPLAudioBuffer PathingBuffer;

    /** Spatialized buffer containing the results of decoding the Ambisonic pathing output. */
    IPLAudioBuffer SpatializedPathingBuffer;

    /** Spatialized output, in deinterleaved format. */
    IPLAudioBuffer OutBuffer;

    int PrevOrder;

    void Reset();

    void ClearBuffers();
};


// ---------------------------------------------------------------------------------------------------------------------
// FSteamAudioSpatializationPlugin
// ---------------------------------------------------------------------------------------------------------------------

/**
 * Singleton object containing shared state for the spatialization plugin.
 */
class FSteamAudioSpatializationPlugin : public IAudioSpatialization
{
public:
    /**
     * Inherited from IAudioSpatialization
     */

    /** Called to initialize the plugin. */
    virtual void Initialize(const FAudioPluginInitializationParams InitializationParams) override;

    /** Called to find out if the plugin is initialized. */
    virtual bool IsSpatializationEffectInitialized() const override;

    /** Called when a given source voice is assigned for rendering a given Audio Component. */
    virtual void OnInitSource(const uint32 SourceId, const FName& AudioComponentUserId, USpatializationPluginSourceSettingsBase* InSettings) override;

    /** Called when a given source voice will no longer be used to render an Audio Component. */
    virtual void OnReleaseSource(const uint32 SourceId) override;

    /** Called to process a single source. */
    virtual void ProcessAudio(const FAudioPluginSourceInputData& InputData, FAudioPluginSourceOutputData& OutputData) override;

private:
    /** Audio pipeline settings. */
    IPLAudioSettings AudioSettings;

    /** Lazy-initialized state for as many sources as we can render simultaneously. */
    TArray<FSteamAudioSpatializationSource> Sources;
};


// ---------------------------------------------------------------------------------------------------------------------
// FSteamAudioSpatializationPluginFactory
// ---------------------------------------------------------------------------------------------------------------------

/**
 * Provides metadata about the spatialization plugin, and a factory method for instantiating it.
 */
class FSteamAudioSpatializationPluginFactory : public IAudioSpatializationFactory
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
     * Inherited from IAudioSpatializationFactory
     */

    /** Returns the class object for the spatialization settings data. */
    virtual UClass* GetCustomSpatializationSettingsClass() const override;

    /** Instantiates the spatialization plugin. */
    virtual TAudioSpatializationPtr CreateNewSpatializationPlugin(FAudioDevice* OwningDevice) override;
};

}
