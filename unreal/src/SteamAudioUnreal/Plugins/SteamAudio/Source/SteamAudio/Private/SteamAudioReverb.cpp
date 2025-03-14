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

#include "SteamAudioReverb.h"
#include "Components/AudioComponent.h"
#include "GameFramework/Actor.h"
#include "HAL/UnrealMemory.h"
#include "Sound/SoundSubmix.h"
#include "SteamAudioCommon.h"
#include "SteamAudioManager.h"
#include "SteamAudioReverbSettings.h"
#include "SteamAudioSettings.h"
#include "SteamAudioSourceComponent.h"

#include "Misc/AssertionMacros.h"

namespace SteamAudio {

// ---------------------------------------------------------------------------------------------------------------------
// FSteamAudioReverbSource
// ---------------------------------------------------------------------------------------------------------------------

FSteamAudioReverbSource::FSteamAudioReverbSource()
	: bApplyReflections(false)
	, bApplyHRTFToReflections(false)
	, ReflectionsMixLevel(1.0f)
    , HRTF(nullptr)
	, ReflectionEffect(nullptr)
	, AmbisonicsDecodeEffect(nullptr)
	, InBuffer()
	, MonoBuffer()
	, IndirectBuffer()
	, OutBuffer()
    , PrevReflectionEffectType(IPL_REFLECTIONEFFECTTYPE_CONVOLUTION)
    , PrevDuration(0.0f)
    , PrevOrder(-1)
{}

FSteamAudioReverbSource::~FSteamAudioReverbSource()
{
	IPLContext Context = FSteamAudioModule::GetManager().GetContext();

	iplAudioBufferFree(Context, &InBuffer);
	iplAudioBufferFree(Context, &MonoBuffer);
	iplAudioBufferFree(Context, &IndirectBuffer);
	iplAudioBufferFree(Context, &OutBuffer);

	iplReflectionEffectRelease(&ReflectionEffect);
	iplAmbisonicsDecodeEffectRelease(&AmbisonicsDecodeEffect);
    iplHRTFRelease(&HRTF);
}

void FSteamAudioReverbSource::Reset()
{
	if (ReflectionEffect)
	{
		iplReflectionEffectReset(ReflectionEffect);
	}

	if (AmbisonicsDecodeEffect)
	{
		iplAmbisonicsDecodeEffectReset(AmbisonicsDecodeEffect);
	}

	ClearBuffers();
}

void FSteamAudioReverbSource::ClearBuffers()
{
	if (InBuffer.data)
	{
		for (int i = 0; i < InBuffer.numChannels; ++i)
		{
			FMemory::Memzero(InBuffer.data[i], InBuffer.numSamples * sizeof(float));
		}
	}

    if (MonoBuffer.data)
    {
        for (int i = 0; i < MonoBuffer.numChannels; ++i)
        {
            FMemory::Memzero(MonoBuffer.data[i], MonoBuffer.numSamples * sizeof(float));
        }
    }

    if (IndirectBuffer.data)
    {
        for (int i = 0; i < IndirectBuffer.numChannels; ++i)
        {
            FMemory::Memzero(IndirectBuffer.data[i], IndirectBuffer.numSamples * sizeof(float));
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
// FSteamAudioReverbPlugin
// ---------------------------------------------------------------------------------------------------------------------

FSteamAudioReverbPlugin::FSteamAudioReverbPlugin()
	: ReverbSubmix(nullptr)
	, ReverbSubmixEffect(nullptr)
	, ReflectionMixer(nullptr)
    , PrevReflectionEffectType(IPL_REFLECTIONEFFECTTYPE_CONVOLUTION)
    , PrevDuration(0.0f)
    , PrevOrder(-1)
{}

FSteamAudioReverbPlugin::~FSteamAudioReverbPlugin()
{
    ShutDownMixer();
}

void FSteamAudioReverbPlugin::Initialize(const FAudioPluginInitializationParams InitializationParams)
{
	AudioSettings.samplingRate = InitializationParams.SampleRate;
	AudioSettings.frameSize = InitializationParams.BufferLength;

	Sources.AddDefaulted(InitializationParams.NumSources);
}

void FSteamAudioReverbPlugin::LazyInitMixer()
{
    IPLContext Context = FSteamAudioModule::GetManager().GetContext();
    IPLSimulationSettings SimulationSettings = FSteamAudioModule::GetManager().GetRealTimeSettings(static_cast<IPLSimulationFlags>(IPL_SIMULATIONFLAGS_REFLECTIONS | IPL_SIMULATIONFLAGS_PATHING));

    if (!ReflectionMixer || PrevReflectionEffectType != SimulationSettings.reflectionType ||
        PrevDuration != SimulationSettings.maxDuration || PrevOrder != SimulationSettings.maxOrder)
    {
        if (ReflectionMixer)
        {
            iplReflectionMixerRelease(&ReflectionMixer);
        }

        IPLReflectionEffectSettings ReflectionSettings{};
        ReflectionSettings.type = SimulationSettings.reflectionType;
        ReflectionSettings.irSize = CalcIRSizeForDuration(SimulationSettings.maxDuration, AudioSettings.samplingRate);
        ReflectionSettings.numChannels = CalcNumChannelsForAmbisonicOrder(SimulationSettings.maxOrder);

        IPLerror Status = iplReflectionMixerCreate(Context, &AudioSettings, &ReflectionSettings, &ReflectionMixer);
        if (Status != IPL_STATUS_SUCCESS)
        {
            UE_LOG(LogSteamAudio, Error, TEXT("Unable to create reflection mixer. [%d]"), Status);
        }
    }

    PrevReflectionEffectType = SimulationSettings.reflectionType;
    PrevDuration = SimulationSettings.maxDuration;
    PrevOrder = SimulationSettings.maxOrder;
}

void FSteamAudioReverbPlugin::ShutDownMixer()
{
    iplReflectionMixerRelease(&ReflectionMixer);
}

void FSteamAudioReverbPlugin::OnInitSource(const uint32 SourceId, const FName& AudioComponentUserId, const uint32 NumChannels, UReverbPluginSourceSettingsBase* InSettings)
{
    // Make sure we're initialized, so real-time audio can work.
    SteamAudio::RunInGameThread<void>([&]()
    {
        FSteamAudioModule::GetManager().InitializeSteamAudio(EManagerInitReason::PLAYING);
    });

	FSteamAudioReverbSource& Source = Sources[SourceId];

    // If a settings asset was provided, use that to configure the source. Otherwise, use defaults.
	USteamAudioReverbSettings* Settings = Cast<USteamAudioReverbSettings>(InSettings);
	Source.bApplyReflections = (Settings) ? Settings->bApplyReflections : false;
	Source.bApplyHRTFToReflections = (Settings) ? Settings->bApplyHRTFToReflections : false;
	Source.ReflectionsMixLevel = (Settings) ? Settings->ReflectionsMixLevel : 1.0f;

    IPLContext Context = FSteamAudioModule::GetManager().GetContext();

    if (!Source.HRTF)
    {
        if (FSteamAudioModule::GetManager().InitHRTF(AudioSettings))
        {
            Source.HRTF = iplHRTFRetain(FSteamAudioModule::GetManager().GetHRTF());
        }
    }

    IPLSimulationSettings SimulationSettings = FSteamAudioModule::GetManager().GetRealTimeSettings(static_cast<IPLSimulationFlags>(IPL_SIMULATIONFLAGS_REFLECTIONS | IPL_SIMULATIONFLAGS_PATHING));

    if (!Source.ReflectionEffect || Source.PrevReflectionEffectType != SimulationSettings.reflectionType ||
        Source.PrevDuration != SimulationSettings.maxDuration || Source.PrevOrder != SimulationSettings.maxOrder)
    {
        if (Source.ReflectionEffect)
        {
            iplReflectionEffectRelease(&Source.ReflectionEffect);
        }

        IPLReflectionEffectSettings ReflectionSettings{};
        ReflectionSettings.type = SimulationSettings.reflectionType;
        ReflectionSettings.irSize = CalcIRSizeForDuration(SimulationSettings.maxDuration, AudioSettings.samplingRate);
        ReflectionSettings.numChannels = CalcNumChannelsForAmbisonicOrder(SimulationSettings.maxOrder);

        IPLerror Status = iplReflectionEffectCreate(Context, &AudioSettings, &ReflectionSettings, &Source.ReflectionEffect);
        if (Status != IPL_STATUS_SUCCESS)
        {
            UE_LOG(LogSteamAudio, Error, TEXT("Unable to create reflection effect. [%d]"), Status);
        }
    }

    if ((!Source.AmbisonicsDecodeEffect || Source.PrevOrder != SimulationSettings.maxOrder) && Source.HRTF)
    {
        IPLAmbisonicsDecodeEffectSettings AmbisonicsDecodeSettings{};
        AmbisonicsDecodeSettings.speakerLayout = GetSpeakerLayoutForNumChannels(NumChannels);
        AmbisonicsDecodeSettings.hrtf = Source.HRTF;
        AmbisonicsDecodeSettings.maxOrder = SimulationSettings.maxOrder;

        if (Source.AmbisonicsDecodeEffect)
        {
            iplAmbisonicsDecodeEffectRelease(&Source.AmbisonicsDecodeEffect);
        }

        IPLerror Status = iplAmbisonicsDecodeEffectCreate(Context, &AudioSettings, &AmbisonicsDecodeSettings, &Source.AmbisonicsDecodeEffect);
        if (Status != IPL_STATUS_SUCCESS)
        {
            UE_LOG(LogSteamAudio, Error, TEXT("Unable to create Ambisonics decode effect. [%d]"), Status);
        }
    }

    if (!Source.InBuffer.data || Source.InBuffer.numChannels != NumChannels)
    {
        if (Source.InBuffer.data)
        {
            iplAudioBufferFree(Context, &Source.InBuffer);
        }

        IPLerror Status = iplAudioBufferAllocate(Context, NumChannels, AudioSettings.frameSize, &Source.InBuffer);
        if (Status != IPL_STATUS_SUCCESS)
        {
            UE_LOG(LogSteamAudio, Error, TEXT("Unable to create input buffer for reverb effect. [%d]"), Status);
        }
    }

    if (!Source.MonoBuffer.data)
    {
        IPLerror Status = iplAudioBufferAllocate(Context, 1, AudioSettings.frameSize, &Source.MonoBuffer);
        if (Status != IPL_STATUS_SUCCESS)
        {
            UE_LOG(LogSteamAudio, Error, TEXT("Unable to create downmix buffer for reverb effect. [%d]"), Status);
        }
    }

    if (!Source.IndirectBuffer.data || PrevOrder != SimulationSettings.maxOrder)
    {
        if (Source.IndirectBuffer.data)
        {
            iplAudioBufferFree(Context, &Source.IndirectBuffer);
        }

        IPLerror Status = iplAudioBufferAllocate(Context, CalcNumChannelsForAmbisonicOrder(SimulationSettings.maxOrder), AudioSettings.frameSize, &Source.IndirectBuffer);
        if (Status != IPL_STATUS_SUCCESS)
        {
            UE_LOG(LogSteamAudio, Error, TEXT("Unable to create indirect buffer for reverb effect. [%d]"), Status);
        }
    }

    if (!Source.OutBuffer.data || Source.OutBuffer.numChannels != NumChannels)
    {
        if (Source.OutBuffer.data)
        {
            iplAudioBufferFree(Context, &Source.OutBuffer);
        }

        IPLerror Status = iplAudioBufferAllocate(Context, NumChannels, AudioSettings.frameSize, &Source.OutBuffer);
        if (Status != IPL_STATUS_SUCCESS)
        {
            UE_LOG(LogSteamAudio, Error, TEXT("Unable to create output buffer for reverb effect. [%d]"), Status);
        }
    }

    Source.PrevReflectionEffectType = SimulationSettings.reflectionType;
    Source.PrevDuration = SimulationSettings.maxDuration;
    Source.PrevOrder = SimulationSettings.maxOrder;
    Source.Reset();
}

void FSteamAudioReverbPlugin::OnReleaseSource(const uint32 SourceId)
{
	FSteamAudioReverbSource& Source = Sources[SourceId];
    Source.Reset();
    iplHRTFRelease(&Source.HRTF);
}

FSoundEffectSubmixPtr FSteamAudioReverbPlugin::GetEffectSubmix()
{
	if (!ReverbSubmixEffect.IsValid())
	{
        USoundSubmix* Submix = GetSubmix();

        if (Submix)
        {
            USteamAudioReverbSubmixPluginPreset* Preset = nullptr;
            if (Submix->SubmixEffectChain.Num() > 0)
            {
                if (USteamAudioReverbSubmixPluginPreset* CurrentPreset = Cast<USteamAudioReverbSubmixPluginPreset>(Submix->SubmixEffectChain[0]))
                {
                    Preset = CurrentPreset;
                }
            }

            if (!Preset)
            {
                Preset = NewObject<USteamAudioReverbSubmixPluginPreset>(Submix, TEXT("Steam Audio Reverb Preset"));
            }

            if (Preset)
            {
                ReverbSubmixEffect = USoundEffectSubmixPreset::CreateInstance<FSoundEffectSubmixInitData, FSoundEffectSubmix>(FSoundEffectSubmixInitData(), *Preset);

                if (ReverbSubmixEffect)
                {
                    StaticCastSharedPtr<FSteamAudioReverbSubmixPlugin, FSoundEffectSubmix>(ReverbSubmixEffect)->SetReverbPlugin(this);
                    ReverbSubmixEffect->SetEnabled(true);
                }
            }
        }
	}

	return ReverbSubmixEffect;
}

USoundSubmix* FSteamAudioReverbPlugin::GetSubmix()
{
	const USteamAudioSettings* Settings = GetDefault<USteamAudioSettings>();
	check(Settings);

	if (!ReverbSubmix.IsValid())
	{
		ReverbSubmix = Cast<USoundSubmix>(Settings->ReverbSubmix.TryLoad());

        if (!ReverbSubmix.IsValid())
        {
            static const FString DefaultSubmixName = TEXT("Steam Audio Reverb Submix");
            ReverbSubmix = NewObject<USoundSubmix>(USoundSubmix::StaticClass(), *DefaultSubmixName);
        }
	}

    if (ReverbSubmix.IsValid())
    {
        ReverbSubmix->bMuteWhenBackgrounded = true;

        bool bFoundPreset = false;
        for (USoundEffectSubmixPreset* Preset : ReverbSubmix->SubmixEffectChain)
        {
            if (Cast<USteamAudioReverbSubmixPluginPreset>(Preset))
            {
                bFoundPreset = true;
                break;
            }
        }

        if (!bFoundPreset)
        {
            static const FString DefaultPresetName = TEXT("Steam Audio Reverb Preset");
            ReverbSubmix->SubmixEffectChain.Add(NewObject<USteamAudioReverbSubmixPluginPreset>(USteamAudioReverbSubmixPluginPreset::StaticClass(), *DefaultPresetName));
        }
    }

    return ReverbSubmix.Get();
}

void FSteamAudioReverbPlugin::ProcessSourceAudio(const FAudioPluginSourceInputData& InputData, FAudioPluginSourceOutputData& OutputData)
{
	FSteamAudioReverbSource& Source = Sources[InputData.SourceId];
    Source.ClearBuffers();

    if (!FSteamAudioModule::IsPlaying())
        return;

    float* InBufferData = InputData.AudioBuffer->GetData();
    float* OutBufferData = OutputData.AudioBuffer.GetData();

    IPLContext Context = FSteamAudioModule::GetManager().GetContext();
    IPLSimulationSettings SimulationSettings = FSteamAudioModule::GetManager().GetRealTimeSettings(static_cast<IPLSimulationFlags>(IPL_SIMULATIONFLAGS_REFLECTIONS | IPL_SIMULATIONFLAGS_PATHING));

    // Apply reflections if requested.
    if (Source.bApplyReflections && Source.HRTF && Source.ReflectionEffect && Source.AmbisonicsDecodeEffect &&
        Source.InBuffer.data && Source.MonoBuffer.data && Source.IndirectBuffer.data && Source.OutBuffer.data)
    {
        // Deinterleave and downmix the input buffer.
        iplAudioBufferDeinterleave(Context, InBufferData, &Source.InBuffer);
        iplAudioBufferDownmix(Context, &Source.InBuffer, &Source.MonoBuffer);

        UAudioComponent* AudioComponent = UAudioComponent::GetAudioComponentFromID(InputData.AudioComponentId);
        USteamAudioSourceComponent* SteamAudioSourceComponent = (AudioComponent) ? AudioComponent->GetOwner()->FindComponentByClass<USteamAudioSourceComponent>() : nullptr;

        if (SteamAudioSourceComponent)
        {
            // Apply reflection mix level to mono buffer.
            for (int i = 0; i < Source.MonoBuffer.numSamples; ++i)
            {
                Source.MonoBuffer.data[0][i] *= Source.ReflectionsMixLevel;
            }

            LazyInitMixer();

            IPLSimulationOutputs Outputs = SteamAudioSourceComponent->GetOutputs(static_cast<IPLSimulationFlags>(IPL_SIMULATIONFLAGS_REFLECTIONS | IPL_SIMULATIONFLAGS_PATHING));

            IPLReflectionEffectParams ReflectionParams = Outputs.reflections;
            ReflectionParams.type = SimulationSettings.reflectionType;
            ReflectionParams.numChannels = SteamAudio::CalcNumChannelsForAmbisonicOrder(SimulationSettings.maxOrder);
            ReflectionParams.irSize = SteamAudio::CalcIRSizeForDuration(SimulationSettings.maxDuration, AudioSettings.samplingRate);
            ReflectionParams.tanDevice = SimulationSettings.tanDevice;

            iplReflectionEffectApply(Source.ReflectionEffect, &ReflectionParams, &Source.MonoBuffer, &Source.IndirectBuffer, ReflectionMixer);

            // If we're not outputting to the mixer (i.e., the submix plugin), then spatialize the reflections here.
            // NOTE: This does not currently work given the signal flow in the audio engine plugins.
            bool bOutputToMixer = (SimulationSettings.reflectionType == IPL_REFLECTIONEFFECTTYPE_CONVOLUTION ||
                SimulationSettings.reflectionType == IPL_REFLECTIONEFFECTTYPE_TAN);

            if (!bOutputToMixer)
            {
                bool bBinaural = (Source.bApplyReflections && Source.bApplyHRTFToReflections);

                IPLAmbisonicsDecodeEffectParams AmbisonicsDecodeParams{};
                AmbisonicsDecodeParams.order = SimulationSettings.maxOrder;
                AmbisonicsDecodeParams.hrtf = Source.HRTF;
                AmbisonicsDecodeParams.orientation = FSteamAudioModule::GetManager().GetListenerCoordinates();
                AmbisonicsDecodeParams.binaural = bBinaural ? IPL_TRUE : IPL_FALSE;

                iplAmbisonicsDecodeEffectApply(Source.AmbisonicsDecodeEffect, &AmbisonicsDecodeParams, &Source.IndirectBuffer, &Source.OutBuffer);

                iplAudioBufferInterleave(Context, &Source.OutBuffer, OutBufferData);
            }
        }
    }
}


// ---------------------------------------------------------------------------------------------------------------------
// FSteamAudioReverbPluginFactory
// ---------------------------------------------------------------------------------------------------------------------

FString FSteamAudioReverbPluginFactory::GetDisplayName()
{
	static FString DisplayName = FString(TEXT("Steam Audio Reverb"));
	return DisplayName;
}

bool FSteamAudioReverbPluginFactory::SupportsPlatform(const FString& PlatformName)
{
	return PlatformName == FString(TEXT("Windows")) ||
		PlatformName == FString(TEXT("Linux")) ||
		PlatformName == FString(TEXT("Mac")) ||
		PlatformName == FString(TEXT("Android")) ||
        PlatformName == FString(TEXT("IOS"));
}

UClass* FSteamAudioReverbPluginFactory::GetCustomReverbSettingsClass() const
{
	return USteamAudioReverbSettings::StaticClass();
}

TAudioReverbPtr FSteamAudioReverbPluginFactory::CreateNewReverbPlugin(FAudioDevice* OwningDevice)
{
	FSteamAudioModule::Get().RegisterAudioDevice(OwningDevice);
	return TAudioReverbPtr(new FSteamAudioReverbPlugin());
}

}


// ---------------------------------------------------------------------------------------------------------------------
// FSteamAudioReverbSubmixPlugin
// ---------------------------------------------------------------------------------------------------------------------

IPLSource FSteamAudioReverbSubmixPlugin::ReverbSource[2] = { nullptr, nullptr };
std::atomic<bool> FSteamAudioReverbSubmixPlugin::bNewReverbSourceWritten(false);

FSteamAudioReverbSubmixPlugin::FSteamAudioReverbSubmixPlugin()
	: ReverbPlugin(nullptr)
	, Context(nullptr)
    , HRTF(nullptr)
	, ReflectionEffect(nullptr)
	, AmbisonicsDecodeEffect(nullptr)
	, InBuffer()
	, MonoBuffer()
	, ReverbBuffer()
	, IndirectBuffer()
	, OutBuffer()
    , PrevReflectionEffectType(IPL_REFLECTIONEFFECTTYPE_CONVOLUTION)
    , PrevDuration(0.0f)
    , PrevOrder(-1)
{}

FSteamAudioReverbSubmixPlugin::~FSteamAudioReverbSubmixPlugin()
{
    ShutDown();
}

uint32 FSteamAudioReverbSubmixPlugin::GetDesiredInputChannelCountOverride() const
{
	// Always use stereo input/output buffers.
	return 2;
}

void FSteamAudioReverbSubmixPlugin::SetReverbPlugin(SteamAudio::FSteamAudioReverbPlugin* Plugin)
{
	ReverbPlugin = Plugin;
}

void FSteamAudioReverbSubmixPlugin::LazyInit()
{
    if (!Context)
    {
        Context = iplContextRetain(SteamAudio::FSteamAudioModule::GetManager().GetContext());
    }

    IPLAudioSettings AudioSettings = ReverbPlugin->GetAudioSettings();

    if (!HRTF)
    {
        if (SteamAudio::FSteamAudioModule::GetManager().InitHRTF(AudioSettings))
        {
            HRTF = iplHRTFRetain(SteamAudio::FSteamAudioModule::GetManager().GetHRTF());
        }
    }

    IPLSimulationSettings SimulationSettings = SteamAudio::FSteamAudioModule::GetManager().GetRealTimeSettings(static_cast<IPLSimulationFlags>(IPL_SIMULATIONFLAGS_REFLECTIONS | IPL_SIMULATIONFLAGS_PATHING));

    if (!ReflectionEffect || PrevReflectionEffectType != SimulationSettings.reflectionType ||
        PrevDuration != SimulationSettings.maxDuration || PrevOrder != SimulationSettings.maxOrder)
    {
        if (ReflectionEffect)
        {
            iplReflectionEffectRelease(&ReflectionEffect);
        }

        IPLReflectionEffectSettings ReflectionSettings{};
        ReflectionSettings.type = SimulationSettings.reflectionType;
        ReflectionSettings.irSize = SteamAudio::CalcIRSizeForDuration(SimulationSettings.maxDuration, AudioSettings.samplingRate);
        ReflectionSettings.numChannels = SteamAudio::CalcNumChannelsForAmbisonicOrder(SimulationSettings.maxOrder);

        IPLerror Status = iplReflectionEffectCreate(Context, &AudioSettings, &ReflectionSettings, &ReflectionEffect);
        if (Status != IPL_STATUS_SUCCESS)
        {
            UE_LOG(LogSteamAudio, Error, TEXT("Unable to create reflection effect. [%d]"), Status);
        }
    }

    if ((!AmbisonicsDecodeEffect || PrevOrder != SimulationSettings.maxOrder) && HRTF)
    {
        if (AmbisonicsDecodeEffect)
        {
            iplAmbisonicsDecodeEffectRelease(&AmbisonicsDecodeEffect);
        }

        IPLAmbisonicsDecodeEffectSettings AmbisonicsDecodeSettings{};
        AmbisonicsDecodeSettings.speakerLayout = SteamAudio::GetSpeakerLayoutForNumChannels(2);
        AmbisonicsDecodeSettings.hrtf = HRTF;
        AmbisonicsDecodeSettings.maxOrder = SimulationSettings.maxOrder;

        IPLerror Status = iplAmbisonicsDecodeEffectCreate(Context, &AudioSettings, &AmbisonicsDecodeSettings, &AmbisonicsDecodeEffect);
        if (Status != IPL_STATUS_SUCCESS)
        {
            UE_LOG(LogSteamAudio, Error, TEXT("Unable to create Ambisonics decode effect. [%d]"), Status);
        }
    }

    if (!InBuffer.data)
    {
        IPLerror Status = iplAudioBufferAllocate(Context, 2, AudioSettings.frameSize, &InBuffer);
        if (Status != IPL_STATUS_SUCCESS)
        {
            UE_LOG(LogSteamAudio, Error, TEXT("Unable to create input buffer for reverb effect. [%d]"), Status);
        }
    }

    if (!MonoBuffer.data)
    {
        IPLerror Status = iplAudioBufferAllocate(Context, 1, AudioSettings.frameSize, &MonoBuffer);
        if (Status != IPL_STATUS_SUCCESS)
        {
            UE_LOG(LogSteamAudio, Error, TEXT("Unable to create downmix buffer for reverb effect. [%d]"), Status);
        }
    }

    if (!ReverbBuffer.data || PrevOrder != SimulationSettings.maxOrder)
    {
        if (ReverbBuffer.data)
        {
            iplAudioBufferFree(Context, &ReverbBuffer);
        }

        IPLerror Status = iplAudioBufferAllocate(Context, SteamAudio::CalcNumChannelsForAmbisonicOrder(SimulationSettings.maxOrder), AudioSettings.frameSize, &ReverbBuffer);
        if (Status != IPL_STATUS_SUCCESS)
        {
            UE_LOG(LogSteamAudio, Error, TEXT("Unable to create reverb buffer for reverb effect. [%d]"), Status);
        }
    }

    if (!IndirectBuffer.data)
    {
        if (IndirectBuffer.data)
        {
            iplAudioBufferFree(Context, &IndirectBuffer);
        }

        IPLerror Status = iplAudioBufferAllocate(Context, SteamAudio::CalcNumChannelsForAmbisonicOrder(SimulationSettings.maxOrder), AudioSettings.frameSize, &IndirectBuffer);
        if (Status != IPL_STATUS_SUCCESS)
        {
            UE_LOG(LogSteamAudio, Error, TEXT("Unable to create indirect buffer for reverb effect. [%d]"), Status);
        }
    }

    if (!OutBuffer.data)
    {
        IPLerror Status = iplAudioBufferAllocate(Context, 2, AudioSettings.frameSize, &OutBuffer);
        if (Status != IPL_STATUS_SUCCESS)
        {
            UE_LOG(LogSteamAudio, Error, TEXT("Unable to create output buffer for reverb effect. [%d]"), Status);
        }
    }

    PrevReflectionEffectType = SimulationSettings.reflectionType;
    PrevDuration = SimulationSettings.maxDuration;
    PrevOrder = SimulationSettings.maxOrder;
}

void FSteamAudioReverbSubmixPlugin::ShutDown()
{
    iplAudioBufferFree(Context, &InBuffer);
    iplAudioBufferFree(Context, &MonoBuffer);
    iplAudioBufferFree(Context, &ReverbBuffer);
    iplAudioBufferFree(Context, &IndirectBuffer);
    iplAudioBufferFree(Context, &OutBuffer);

    iplSourceRelease(&ReverbSource[0]);
    iplSourceRelease(&ReverbSource[1]);
    bNewReverbSourceWritten = false;

    iplAmbisonicsDecodeEffectRelease(&AmbisonicsDecodeEffect);
    iplReflectionEffectRelease(&ReflectionEffect);
    iplHRTFRelease(&HRTF);
    iplContextRelease(&Context);
}

void FSteamAudioReverbSubmixPlugin::Reset()
{
    if (ReflectionEffect)
    {
        iplReflectionEffectReset(ReflectionEffect);
    }

    if (AmbisonicsDecodeEffect)
    {
        iplAmbisonicsDecodeEffectReset(AmbisonicsDecodeEffect);
    }

    ClearBuffers();
}

void FSteamAudioReverbSubmixPlugin::ClearBuffers()
{
    if (InBuffer.data)
    {
        for (int i = 0; i < InBuffer.numChannels; ++i)
        {
            FMemory::Memzero(InBuffer.data[i], InBuffer.numSamples * sizeof(float));
        }
    }

    if (MonoBuffer.data)
    {
        for (int i = 0; i < MonoBuffer.numChannels; ++i)
        {
            FMemory::Memzero(MonoBuffer.data[i], MonoBuffer.numSamples * sizeof(float));
        }
    }

    if (ReverbBuffer.data)
    {
        for (int i = 0; i < ReverbBuffer.numChannels; ++i)
        {
            FMemory::Memzero(ReverbBuffer.data[i], ReverbBuffer.numSamples * sizeof(float));
        }
    }

    if (IndirectBuffer.data)
    {
        for (int i = 0; i < IndirectBuffer.numChannels; ++i)
        {
            FMemory::Memzero(IndirectBuffer.data[i], IndirectBuffer.numSamples * sizeof(float));
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

void FSteamAudioReverbSubmixPlugin::OnProcessAudio(const FSoundEffectSubmixInputData& InData, FSoundEffectSubmixOutputData& OutData)
{
	// The submix plugin can keep running in the editor when not in play mode. So don't do anything if Steam Audio
	// is not initialized.
    if (!SteamAudio::FSteamAudioModule::IsPlaying())
    {
        if (ReflectionEffect)
        {
            ShutDown();
        }

        if (ReverbPlugin && ReverbPlugin->GetReflectionMixer())
        {
            ReverbPlugin->ShutDownMixer();
        }

        return;
    }

	float* InBufferData = InData.AudioBuffer->GetData();
	float* OutBufferData = OutData.AudioBuffer->GetData();

    ClearBuffers();

    IPLSimulationSettings SimulationSettings = SteamAudio::FSteamAudioModule::GetManager().GetRealTimeSettings(static_cast<IPLSimulationFlags>(IPL_SIMULATIONFLAGS_REFLECTIONS | IPL_SIMULATIONFLAGS_PATHING));

    LazyInit();

    if (ReverbPlugin)
	{
        ReverbPlugin->LazyInitMixer();

        bool bHasOutput = false;

		// Grab source-centric reflections from the mixer.
		if (SimulationSettings.reflectionType == IPL_REFLECTIONEFFECTTYPE_CONVOLUTION || SimulationSettings.reflectionType == IPL_REFLECTIONEFFECTTYPE_TAN)
		{
            IPLReflectionMixer Mixer = ReverbPlugin->GetReflectionMixer();

            if (Mixer && IndirectBuffer.data)
            {
                IPLReflectionEffectParams ReflectionParams{};
                ReflectionParams.numChannels = SteamAudio::CalcNumChannelsForAmbisonicOrder(SimulationSettings.maxOrder);
                ReflectionParams.tanDevice = SimulationSettings.tanDevice;

                iplReflectionMixerApply(Mixer, &ReflectionParams, &IndirectBuffer);

                bHasOutput = true;
            }
		}

		// If requested, apply reverb to the input.
		USteamAudioReverbSubmixPluginPreset* ReverbPreset = Cast<USteamAudioReverbSubmixPluginPreset>(GetPreset());
		if (ReverbPreset && ReverbPreset->Settings.bApplyReverb)
		{
            // If a Steam Audio Listener component has not set the current reverb source, stop.
            IPLSource CurrentReverbSource = GetReverbSource();
			if (CurrentReverbSource && ReflectionEffect &&
                InBuffer.data && MonoBuffer.data && ReverbBuffer.data && IndirectBuffer.data)
			{
				iplAudioBufferDeinterleave(Context, InBufferData, &InBuffer);
				iplAudioBufferDownmix(Context, &InBuffer, &MonoBuffer);

				IPLSimulationOutputs Outputs{};
				iplSourceGetOutputs(CurrentReverbSource, IPL_SIMULATIONFLAGS_REFLECTIONS, &Outputs);

				IPLReflectionEffectParams ReverbParams = Outputs.reflections;
				ReverbParams.type = SimulationSettings.reflectionType;
				ReverbParams.numChannels = SteamAudio::CalcNumChannelsForAmbisonicOrder(SimulationSettings.maxOrder);
				ReverbParams.irSize = SteamAudio::CalcIRSizeForDuration(SimulationSettings.maxDuration, SimulationSettings.samplingRate);
				ReverbParams.tanDevice = SimulationSettings.tanDevice;

				if (SimulationSettings.reflectionType == IPL_REFLECTIONEFFECTTYPE_CONVOLUTION || SimulationSettings.reflectionType == IPL_REFLECTIONEFFECTTYPE_TAN)
				{
					// We might have mixed source-centric reflections, so render listener-centric reverb into a temp
					// buffer and mix it into the source-centric reflections.
					iplReflectionEffectApply(ReflectionEffect, &ReverbParams, &MonoBuffer, &ReverbBuffer, nullptr);
					iplAudioBufferMix(Context, &ReverbBuffer, &IndirectBuffer);
				}
				else
				{
					// We don't have source-centric reflections, so just render the listener-centric reverb into the buffer
					// that we'll spatialize in the next step.
					iplReflectionEffectApply(ReflectionEffect, &ReverbParams, &MonoBuffer, &IndirectBuffer, nullptr);
				}

                bHasOutput = true;
			}
		}

        if (bHasOutput && HRTF && AmbisonicsDecodeEffect && IndirectBuffer.data && OutBuffer.data)
        {
            USteamAudioReverbSubmixPluginPreset* CurrentPreset = Cast<USteamAudioReverbSubmixPluginPreset>(GetPreset());

            IPLAmbisonicsDecodeEffectParams AmbisonicsDecodeParams{};
            AmbisonicsDecodeParams.order = SimulationSettings.maxOrder;
            AmbisonicsDecodeParams.hrtf = HRTF;
            AmbisonicsDecodeParams.orientation = SteamAudio::FSteamAudioModule::GetManager().GetListenerCoordinates();
            AmbisonicsDecodeParams.binaural = (CurrentPreset && CurrentPreset->Settings.bApplyHRTF) ? IPL_TRUE : IPL_FALSE;

            iplAmbisonicsDecodeEffectApply(AmbisonicsDecodeEffect, &AmbisonicsDecodeParams, &IndirectBuffer, &OutBuffer);

            iplAudioBufferInterleave(Context, &OutBuffer, OutBufferData);
        }
	}
}

IPLSource FSteamAudioReverbSubmixPlugin::GetReverbSource()
{
    if (bNewReverbSourceWritten)
    {
        iplSourceRelease(&ReverbSource[0]);
        ReverbSource[0] = iplSourceRetain(ReverbSource[1]);

        bNewReverbSourceWritten = false;
    }

    return ReverbSource[0];
}

void FSteamAudioReverbSubmixPlugin::SetReverbSource(IPLSource Source)
{
    if (!bNewReverbSourceWritten)
    {
        iplSourceRelease(&ReverbSource[1]);
        ReverbSource[1] = iplSourceRetain(Source);

        bNewReverbSourceWritten = true;
    }
}


// ---------------------------------------------------------------------------------------------------------------------
// FSteamAudioReverbSubmixPluginSettings
// ---------------------------------------------------------------------------------------------------------------------

FSteamAudioReverbSubmixPluginSettings::FSteamAudioReverbSubmixPluginSettings()
	: bApplyReverb(false)
	, bApplyHRTF(false)
{}
