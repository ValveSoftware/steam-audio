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

#include "SteamAudioSpatialization.h"
#include "Components/AudioComponent.h"
#include "HAL/UnrealMemory.h"
#include "SteamAudioCommon.h"
#include "SteamAudioManager.h"
#include "SteamAudioSourceComponent.h"
#include "SteamAudioSpatializationSettings.h"

namespace SteamAudio {

// ---------------------------------------------------------------------------------------------------------------------
// FSteamAudioSpatializationSource
// ---------------------------------------------------------------------------------------------------------------------

FSteamAudioSpatializationSource::FSteamAudioSpatializationSource()
    : bBinaural(true)
    , Interpolation(EHRTFInterpolation::NEAREST)
    , bApplyPathing(false)
    , bApplyHRTFToPathing(false)
    , PathingMixLevel(1.0f)
    , HRTF(nullptr)
    , PanningEffect(nullptr)
    , BinauralEffect(nullptr)
    , PathEffect(nullptr)
    , AmbisonicsDecodeEffect(nullptr)
    , PathingInputBuffer()
    , PathingBuffer()
    , SpatializedPathingBuffer()
    , OutBuffer()
    , PrevOrder(-1)
{}

FSteamAudioSpatializationSource::~FSteamAudioSpatializationSource()
{
    IPLContext Context = FSteamAudioModule::GetManager().GetContext();

    iplAudioBufferFree(Context, &PathingInputBuffer);
    iplAudioBufferFree(Context, &PathingBuffer);
    iplAudioBufferFree(Context, &SpatializedPathingBuffer);
    iplAudioBufferFree(Context, &OutBuffer);

    iplAmbisonicsDecodeEffectRelease(&AmbisonicsDecodeEffect);
    iplPathEffectRelease(&PathEffect);
    iplBinauralEffectRelease(&BinauralEffect);
    iplPanningEffectRelease(&PanningEffect);
    iplHRTFRelease(&HRTF);
}

void FSteamAudioSpatializationSource::Reset()
{
    if (PanningEffect)
    {
        iplPanningEffectReset(PanningEffect);
    }

    if (BinauralEffect)
    {
        iplBinauralEffectReset(BinauralEffect);
    }

    if (PathEffect)
    {
        iplPathEffectReset(PathEffect);
    }

    if (AmbisonicsDecodeEffect)
    {
        iplAmbisonicsDecodeEffectReset(AmbisonicsDecodeEffect);
    }

    ClearBuffers();
}

void FSteamAudioSpatializationSource::ClearBuffers()
{
    if (PathingInputBuffer.data)
    {
        for (int i = 0; i < PathingInputBuffer.numChannels; ++i)
        {
            FMemory::Memzero(PathingInputBuffer.data[i], PathingInputBuffer.numSamples * sizeof(float));
        }
    }

    if (PathingBuffer.data)
    {
        for (int i = 0; i < PathingBuffer.numChannels; ++i)
        {
            FMemory::Memzero(PathingBuffer.data[i], PathingBuffer.numSamples * sizeof(float));
        }
    }

    if (SpatializedPathingBuffer.data)
    {
        for (int i = 0; i < SpatializedPathingBuffer.numChannels; ++i)
        {
            FMemory::Memzero(SpatializedPathingBuffer.data[i], SpatializedPathingBuffer.numSamples * sizeof(float));
        }
    }

    if (OutBuffer.data)
    {
        for (int i = 0; i < OutBuffer.numChannels; ++i)
        {
            FMemory::Memzero(OutBuffer.data[i], OutBuffer.numSamples * sizeof(float));
        }
    }
}


// ---------------------------------------------------------------------------------------------------------------------
// FSteamAudioSpatializationPlugin
// ---------------------------------------------------------------------------------------------------------------------

void FSteamAudioSpatializationPlugin::Initialize(const FAudioPluginInitializationParams InitializationParams)
{
    AudioSettings.samplingRate = InitializationParams.SampleRate;
    AudioSettings.frameSize = InitializationParams.BufferLength;

    Sources.AddDefaulted(InitializationParams.NumSources);
}

bool FSteamAudioSpatializationPlugin::IsSpatializationEffectInitialized() const
{
    return true;
}

void FSteamAudioSpatializationPlugin::OnInitSource(const uint32 SourceId, const FName& AudioComponentUserId, USpatializationPluginSourceSettingsBase* InSettings)
{
    // Make sure we're initialized, so real-time audio can work.
    SteamAudio::RunInGameThread<void>([&]()
    {
        FSteamAudioModule::GetManager().InitializeSteamAudio(EManagerInitReason::PLAYING);
    });

    FSteamAudioSpatializationSource& Source = Sources[SourceId];

    // If a settings asset was provided, use that to configure the source. Otherwise, use defaults.
    USteamAudioSpatializationSettings* Settings = Cast<USteamAudioSpatializationSettings>(InSettings);
    Source.bBinaural = (Settings) ? Settings->bBinaural : true;
    Source.Interpolation = (Settings) ? Settings->Interpolation : EHRTFInterpolation::NEAREST;
    Source.bApplyPathing = (Settings) ? Settings->bApplyPathing : false;
    Source.bApplyHRTFToPathing = (Settings) ? Settings->bApplyHRTFToPathing : false;
    Source.PathingMixLevel = (Settings) ? Settings->PathingMixLevel : 1.0f;

    IPLContext Context = FSteamAudioModule::GetManager().GetContext();

    if (!Source.HRTF)
    {
        if (FSteamAudioModule::GetManager().InitHRTF(AudioSettings))
        {
            Source.HRTF = iplHRTFRetain(FSteamAudioModule::GetManager().GetHRTF());
        }
    }

    if (!Source.PanningEffect)
    {
        IPLPanningEffectSettings PanningSettings{};
        PanningSettings.speakerLayout.type = IPL_SPEAKERLAYOUTTYPE_STEREO;

        IPLerror Status = iplPanningEffectCreate(Context, &AudioSettings, &PanningSettings, &Source.PanningEffect);
        if (Status != IPL_STATUS_SUCCESS)
        {
            UE_LOG(LogSteamAudio, Error, TEXT("Unable to create panning effect. [%d]"), Status);
        }
    }

    if (!Source.BinauralEffect && Source.HRTF)
    {
        IPLBinauralEffectSettings BinauralSettings{};
        BinauralSettings.hrtf = Source.HRTF;

        IPLerror Status = iplBinauralEffectCreate(Context, &AudioSettings, &BinauralSettings, &Source.BinauralEffect);
        if (Status != IPL_STATUS_SUCCESS)
        {
            UE_LOG(LogSteamAudio, Error, TEXT("Unable to create binaural effect. [%d]"), Status);
        }
    }

    IPLSimulationSettings SimulationSettings = FSteamAudioModule::GetManager().GetRealTimeSettings(static_cast<IPLSimulationFlags>(IPL_SIMULATIONFLAGS_REFLECTIONS | IPL_SIMULATIONFLAGS_PATHING));

    if (!Source.PathEffect || Source.PrevOrder != SimulationSettings.maxOrder)
    {
        if (Source.PathEffect)
        {
            iplPathEffectRelease(&Source.PathEffect);
        }

        IPLPathEffectSettings PathingSettings{};
        PathingSettings.maxOrder = SimulationSettings.maxOrder;
        PathingSettings.spatialize = IPL_TRUE;
        PathingSettings.speakerLayout.type = IPL_SPEAKERLAYOUTTYPE_STEREO;
        PathingSettings.hrtf = Source.HRTF;

        IPLerror Status = iplPathEffectCreate(Context, &AudioSettings, &PathingSettings, &Source.PathEffect);
        if (Status != IPL_STATUS_SUCCESS)
        {
            UE_LOG(LogSteamAudio, Error, TEXT("Unable to create pathing effect. [%d]"), Status);
        }
    }

    if ((!Source.AmbisonicsDecodeEffect || Source.PrevOrder != SimulationSettings.maxOrder) && Source.HRTF)
    {
        if (Source.AmbisonicsDecodeEffect)
        {
            iplAmbisonicsDecodeEffectRelease(&Source.AmbisonicsDecodeEffect);
        }

        IPLAmbisonicsDecodeEffectSettings AmbisonicsDecodeSettings{};
        AmbisonicsDecodeSettings.speakerLayout.type = IPL_SPEAKERLAYOUTTYPE_STEREO;
        AmbisonicsDecodeSettings.hrtf = Source.HRTF;
        AmbisonicsDecodeSettings.maxOrder = SimulationSettings.maxOrder;

        IPLerror Status = iplAmbisonicsDecodeEffectCreate(Context, &AudioSettings, &AmbisonicsDecodeSettings, &Source.AmbisonicsDecodeEffect);
        if (Status != IPL_STATUS_SUCCESS)
        {
            UE_LOG(LogSteamAudio, Error, TEXT("Unable to create Ambisonics decode effect. [%d]"), Status);
        }
    }

    if (!Source.PathingInputBuffer.data)
    {
        IPLerror Status = iplAudioBufferAllocate(Context, 1, AudioSettings.frameSize, &Source.PathingInputBuffer);
        if (Status != IPL_STATUS_SUCCESS)
        {
            UE_LOG(LogSteamAudio, Error, TEXT("Unable to create pathing input buffer for spatialization effect. [%d]"), Status);
        }
    }

    if (!Source.PathingBuffer.data || Source.PrevOrder != SimulationSettings.maxOrder)
    {
        if (Source.PathingBuffer.data)
        {
            iplAudioBufferFree(Context, &Source.PathingBuffer);
        }

        IPLerror Status = iplAudioBufferAllocate(Context, CalcNumChannelsForAmbisonicOrder(SimulationSettings.maxOrder), AudioSettings.frameSize, &Source.PathingBuffer);
        if (Status != IPL_STATUS_SUCCESS)
        {
            UE_LOG(LogSteamAudio, Error, TEXT("Unable to create pathing buffer for spatialization effect. [%d]"), Status);
        }
    }

    if (!Source.SpatializedPathingBuffer.data)
    {
        IPLerror Status = iplAudioBufferAllocate(Context, 2, AudioSettings.frameSize, &Source.SpatializedPathingBuffer);
        if (Status != IPL_STATUS_SUCCESS)
        {
            UE_LOG(LogSteamAudio, Error, TEXT("Unable to create spatialized pathing buffer for spatialization effect. [%d]"), Status);
        }
    }

    if (!Source.OutBuffer.data)
    {
        IPLerror Status = iplAudioBufferAllocate(Context, 2, AudioSettings.frameSize, &Source.OutBuffer);
        if (Status != IPL_STATUS_SUCCESS)
        {
            UE_LOG(LogSteamAudio, Error, TEXT("Unable to create output buffer for spatialization effect. [%d]"), Status);
        }
    }

    Source.PrevOrder = SimulationSettings.maxOrder;
    Source.Reset();
}

void FSteamAudioSpatializationPlugin::OnReleaseSource(const uint32 SourceId)
{
    FSteamAudioSpatializationSource& Source = Sources[SourceId];
    Source.Reset();
    iplHRTFRelease(&Source.HRTF);
}

void FSteamAudioSpatializationPlugin::ProcessAudio(const FAudioPluginSourceInputData& InputData, FAudioPluginSourceOutputData& OutputData)
{
    FSteamAudioSpatializationSource& Source = Sources[InputData.SourceId];

    float* InBufferData = InputData.AudioBuffer->GetData();
    float* OutBufferData = OutputData.AudioBuffer.GetData();

    IPLContext Context = FSteamAudioModule::GetManager().GetContext();

    Source.ClearBuffers();

    // The input buffer is always mono, so we don't need to deinterleave it into a temporary buffer.
    IPLAudioBuffer InBuffer{};
    InBuffer.numChannels = 1;
    InBuffer.numSamples = AudioSettings.frameSize;
    InBuffer.data = &InBufferData;

    if (Source.HRTF && Source.PanningEffect && Source.BinauralEffect && Source.OutBuffer.data)
    {
        // Workaround. The directions passed to spatializer is not consistent with the coordinate system of UE4, therefore
        // special tranformation is performed here. Review this change if further changes are made to the direction passed
        // to the spatializer.
        IPLVector3 RelativeDirection;
        RelativeDirection.x = InputData.SpatializationParams->EmitterPosition.Y;
        RelativeDirection.y = InputData.SpatializationParams->EmitterPosition.X;
        RelativeDirection.z = InputData.SpatializationParams->EmitterPosition.Z;

        // Apply panning or binaural effect to the input, storing the result in OutBuffer.
        if (Source.bBinaural)
        {
            IPLBinauralEffectParams Params{};
            Params.direction = RelativeDirection;
            Params.interpolation = static_cast<IPLHRTFInterpolation>(Source.Interpolation);
            Params.spatialBlend = 1.0f;
            Params.hrtf = Source.HRTF;

            iplBinauralEffectApply(Source.BinauralEffect, &Params, &InBuffer, &Source.OutBuffer);
        }
        else
        {
            IPLPanningEffectParams Params{};
            Params.direction = RelativeDirection;

            iplPanningEffectApply(Source.PanningEffect, &Params, &InBuffer, &Source.OutBuffer);
        }
    }

    // Apply pathing if specified.
    if (Source.bApplyPathing && Source.HRTF && Source.PathEffect && Source.AmbisonicsDecodeEffect &&
        Source.PathingInputBuffer.data && Source.PathingBuffer.data && Source.SpatializedPathingBuffer.data && Source.OutBuffer.data)
    {
        // FIXME: Unreal 4.27 does not pass the audio component id correctly to the spatializer plugin. It does this
        // correctly for the occlusion and reverb plugins.
        UAudioComponent* AudioComponent = UAudioComponent::GetAudioComponentFromID(InputData.AudioComponentId);
        USteamAudioSourceComponent* SteamAudioSourceComponent = AudioComponent && AudioComponent->GetOwner() ? AudioComponent->GetOwner()->FindComponentByClass<USteamAudioSourceComponent>() : nullptr;

        if (SteamAudioSourceComponent && FSteamAudioModule::IsPlaying())
        {
            IPLSimulationSettings SimulationSettings = FSteamAudioModule::GetManager().GetRealTimeSettings(static_cast<IPLSimulationFlags>(IPL_SIMULATIONFLAGS_REFLECTIONS | IPL_SIMULATIONFLAGS_PATHING));

            IPLSimulationOutputs Outputs = SteamAudioSourceComponent->GetOutputs(static_cast<IPLSimulationFlags>(IPL_SIMULATIONFLAGS_REFLECTIONS | IPL_SIMULATIONFLAGS_PATHING));

            for (int i = 0; i < InBuffer.numSamples; ++i)
            {
                Source.PathingInputBuffer.data[0][i] = Source.PathingMixLevel * InBuffer.data[0][i];
            }

            IPLPathEffectParams PathingParams = Outputs.pathing;
            PathingParams.order = SimulationSettings.maxOrder;
            PathingParams.binaural = Source.bApplyHRTFToPathing ? IPL_TRUE : IPL_FALSE;
            PathingParams.hrtf = Source.HRTF;
            PathingParams.listener = FSteamAudioModule::GetManager().GetListenerCoordinates();

            iplPathEffectApply(Source.PathEffect, &PathingParams, &Source.PathingInputBuffer, &Source.SpatializedPathingBuffer);

            iplAudioBufferMix(Context, &Source.SpatializedPathingBuffer, &Source.OutBuffer);
        }
    }

    // Interleave OutBuffer into the actual output buffer.
    if (Source.OutBuffer.data)
    {
        iplAudioBufferInterleave(Context, &Source.OutBuffer, OutBufferData);
    }
}


// ---------------------------------------------------------------------------------------------------------------------
// FSteamAudioSpatializationPluginFactory
// ---------------------------------------------------------------------------------------------------------------------

FString FSteamAudioSpatializationPluginFactory::GetDisplayName()
{
    static FString DisplayName = FString(TEXT("Steam Audio Spatialization"));
    return DisplayName;
}

bool FSteamAudioSpatializationPluginFactory::SupportsPlatform(const FString& PlatformName)
{
    return PlatformName == FString(TEXT("Windows")) ||
        PlatformName == FString(TEXT("Linux")) ||
        PlatformName == FString(TEXT("Mac")) ||
        PlatformName == FString(TEXT("Android")) ||
        PlatformName == FString(TEXT("IOS"));
}

UClass* FSteamAudioSpatializationPluginFactory::GetCustomSpatializationSettingsClass() const
{
    return USteamAudioSpatializationSettings::StaticClass();
}

TAudioSpatializationPtr FSteamAudioSpatializationPluginFactory::CreateNewSpatializationPlugin(FAudioDevice* OwningDevice)
{
    return TAudioSpatializationPtr(new FSteamAudioSpatializationPlugin());
}

}
