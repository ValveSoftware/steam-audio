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
#include "SteamAudioOcclusionSettings.h"

namespace SteamAudio {

// ---------------------------------------------------------------------------------------------------------------------
// FSteamAudioOcclusionSource
// ---------------------------------------------------------------------------------------------------------------------

/**
 * Rendering state for a single voice that uses the occlusion plugin.
 */
struct FSteamAudioOcclusionSource
{
    FSteamAudioOcclusionSource();

    ~FSteamAudioOcclusionSource();

    bool bApplyDistanceAttenuation;
    bool bApplyAirAbsorption;
    bool bApplyDirectivity;
    float DipoleWeight;
    float DipolePower;
    bool bApplyOcclusion;
    bool bApplyTransmission;
    ETransmissionType TransmissionType;

    IPLDirectEffect DirectEffect;

    /** Deinterleaved input buffer. */
    IPLAudioBuffer InBuffer;

    /** Deinterleaved output buffer. */
    IPLAudioBuffer OutBuffer;

    int PrevNumChannels;

    void Reset();

    void ClearBuffers();
};


// ---------------------------------------------------------------------------------------------------------------------
// FSteamAudioOcclusionPlugin
// ---------------------------------------------------------------------------------------------------------------------

/**
 * Singleton object containing shared state for the occlusion plugin.
 */
class FSteamAudioOcclusionPlugin : public IAudioOcclusion
{
public:
    /**
     * Inherited from IAudioOcclusion
     */

    /** Called to initialize the plugin. */
    virtual void Initialize(const FAudioPluginInitializationParams InitializationParams) override;

    /** Called when a given source voice is assigned for rendering a given Audio Component. */
    virtual void OnInitSource(const uint32 SourceId, const FName& AudioComponentUserId, const uint32 NumChannels, UOcclusionPluginSourceSettingsBase* InSettings) override;

    /** Called when a given source voice will no longer be used to render an Audio Component. */
    virtual void OnReleaseSource(const uint32 SourceId) override;

    /** Called to process a single source. */
    virtual void ProcessAudio(const FAudioPluginSourceInputData& InputData, FAudioPluginSourceOutputData& OutputData) override;

private:
    /** Audio pipeline settings. */
    IPLAudioSettings AudioSettings;

    /** Lazy-initialized state for as many sources as we can render simultaneously. */
    TArray<FSteamAudioOcclusionSource> Sources;
};


// ---------------------------------------------------------------------------------------------------------------------
// FSteamAudioOcclusionPluginFactory
// ---------------------------------------------------------------------------------------------------------------------

/**
 * Provides metadata about the occlusion plugin, and a factory method for instantiating it.
 */
class FSteamAudioOcclusionPluginFactory : public IAudioOcclusionFactory
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

    /** Returns the class object for the occlusion settings data. */
    virtual UClass* GetCustomOcclusionSettingsClass() const;

    /** Instantiates the occlusion plugin. */
    virtual TAudioOcclusionPtr CreateNewOcclusionPlugin(FAudioDevice* OwningDevice) override;
};

}
