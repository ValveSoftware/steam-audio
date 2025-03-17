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

#include "SteamAudioOcclusion.h"
#include "Components/AudioComponent.h"
#include "GameFramework/Actor.h"
#include "HAL/UnrealMemory.h"
#include "SteamAudioCommon.h"
#include "SteamAudioManager.h"
#include "SteamAudioOcclusionSettings.h"
#include "SteamAudioSourceComponent.h"

namespace SteamAudio {

// ---------------------------------------------------------------------------------------------------------------------
// FSteamAudioOcclusionSource
// ---------------------------------------------------------------------------------------------------------------------

FSteamAudioOcclusionSource::FSteamAudioOcclusionSource()
    : bApplyDistanceAttenuation(false)
    , bApplyAirAbsorption(false)
    , bApplyDirectivity(false)
    , DipoleWeight(0.0f)
    , DipolePower(0.0f)
    , bApplyOcclusion(false)
    , bApplyTransmission(false)
    , TransmissionType(ETransmissionType::FREQUENCY_DEPENDENT)
    , DirectEffect(nullptr)
    , InBuffer()
    , OutBuffer()
    , PrevNumChannels(0)
{}

FSteamAudioOcclusionSource::~FSteamAudioOcclusionSource()
{
    IPLContext Context = FSteamAudioModule::GetManager().GetContext();

    iplAudioBufferFree(Context, &InBuffer);
    iplAudioBufferFree(Context, &OutBuffer);

    iplDirectEffectRelease(&DirectEffect);
}

void FSteamAudioOcclusionSource::Reset()
{
    if (DirectEffect)
    {
        iplDirectEffectReset(DirectEffect);
    }

    ClearBuffers();
}

void FSteamAudioOcclusionSource::ClearBuffers()
{
    if (InBuffer.data)
    {
        for (int i = 0; i < InBuffer.numChannels; ++i)
        {
            FMemory::Memzero(InBuffer.data[i], InBuffer.numSamples * sizeof(float));
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
// FSteamAudioOcclusionPlugin
// ---------------------------------------------------------------------------------------------------------------------

void FSteamAudioOcclusionPlugin::Initialize(const FAudioPluginInitializationParams InitializationParams)
{
    AudioSettings.samplingRate = InitializationParams.SampleRate;
    AudioSettings.frameSize = InitializationParams.BufferLength;

    Sources.AddDefaulted(InitializationParams.NumSources);
}

void FSteamAudioOcclusionPlugin::OnInitSource(const uint32 SourceId, const FName& AudioComponentUserId, const uint32 NumChannels, UOcclusionPluginSourceSettingsBase* InSettings)
{
    // Make sure we're initialized, so real-time audio can work.
    SteamAudio::RunInGameThread<void>([&]()
    {
        FSteamAudioModule::GetManager().InitializeSteamAudio(EManagerInitReason::PLAYING);
    });

    FSteamAudioOcclusionSource& Source = Sources[SourceId];

    // If a settings asset was provided, use that to configure the source. Otherwise, use defaults.
    USteamAudioOcclusionSettings* Settings = Cast<USteamAudioOcclusionSettings>(InSettings);
    Source.bApplyDistanceAttenuation = (Settings) ? Settings->bApplyDistanceAttenuation : false;
    Source.bApplyAirAbsorption = (Settings) ? Settings->bApplyAirAbsorption : false;
    Source.bApplyDirectivity = (Settings) ? Settings->bApplyDirectivity : false;
    Source.DipoleWeight = (Settings) ? Settings->DipoleWeight : 0.0f;
    Source.DipolePower = (Settings) ? Settings->DipolePower : 0.0f;
    Source.bApplyOcclusion = (Settings) ? Settings->bApplyOcclusion : false;
    Source.bApplyTransmission = (Settings) ? Settings->bApplyTransmission : false;
    Source.TransmissionType = (Settings) ? Settings->TransmissionType : ETransmissionType::FREQUENCY_DEPENDENT;

    IPLContext Context = FSteamAudioModule::GetManager().GetContext();

    if (!Source.DirectEffect || Source.PrevNumChannels != NumChannels)
    {
        if (Source.DirectEffect)
        {
            iplDirectEffectRelease(&Source.DirectEffect);
        }

        IPLDirectEffectSettings DirectSettings{};
        DirectSettings.numChannels = NumChannels;

        IPLerror Status = iplDirectEffectCreate(Context, &AudioSettings, &DirectSettings, &Source.DirectEffect);
        if (Status != IPL_STATUS_SUCCESS)
        {
            UE_LOG(LogSteamAudio, Error, TEXT("Unable to create direct effect. [%d]"), Status);
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
            UE_LOG(LogSteamAudio, Error, TEXT("Unable to create input buffer for occlusion effect. [%d]"), Status);
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
            UE_LOG(LogSteamAudio, Error, TEXT("Unable to create output buffer for occlusion effect. [%d]"), Status);
        }
    }

    Source.PrevNumChannels = NumChannels;

    Source.Reset();
}

void FSteamAudioOcclusionPlugin::OnReleaseSource(const uint32 SourceId)
{
    FSteamAudioOcclusionSource& Source = Sources[SourceId];
    Source.Reset();
}

void FSteamAudioOcclusionPlugin::ProcessAudio(const FAudioPluginSourceInputData& InputData, FAudioPluginSourceOutputData& OutputData)
{
    FSteamAudioOcclusionSource& Source = Sources[InputData.SourceId];

    float* InBufferData = InputData.AudioBuffer->GetData();
    float* OutBufferData = OutputData.AudioBuffer.GetData();

    IPLContext Context = FSteamAudioModule::GetManager().GetContext();

    Source.ClearBuffers();

    if (Source.DirectEffect && Source.InBuffer.data && Source.OutBuffer.data)
    {
        // Deinterleave the input buffer.
        iplAudioBufferDeinterleave(Context, InBufferData, &Source.InBuffer);

        // We are given the source's position and orientation.
        IPLCoordinateSpace3 SourceCoordinates{};
        SourceCoordinates.origin = SteamAudio::ConvertVector(InputData.SpatializationParams->EmitterWorldPosition);
        SourceCoordinates.ahead = SteamAudio::ConvertVector(InputData.SpatializationParams->EmitterWorldRotation * FVector::ForwardVector, false);
        SourceCoordinates.right = SteamAudio::ConvertVector(InputData.SpatializationParams->EmitterWorldRotation * FVector::RightVector, false);
        SourceCoordinates.up = SteamAudio::ConvertVector(InputData.SpatializationParams->EmitterWorldRotation * FVector::UpVector, false);

        // Get the listener's position and orientation from the global audio plugin listener.
        IPLCoordinateSpace3 ListenerCoordinates = FSteamAudioModule::GetManager().GetListenerCoordinates();

        IPLDirectEffectParams Params{};

        // Figure out which features of the direct effect we want to use.
        if (Source.bApplyDistanceAttenuation)
            Params.flags = static_cast<IPLDirectEffectFlags>(Params.flags | IPL_DIRECTEFFECTFLAGS_APPLYDISTANCEATTENUATION);
        if (Source.bApplyAirAbsorption)
            Params.flags = static_cast<IPLDirectEffectFlags>(Params.flags | IPL_DIRECTEFFECTFLAGS_APPLYAIRABSORPTION);
        if (Source.bApplyDirectivity)
            Params.flags = static_cast<IPLDirectEffectFlags>(Params.flags | IPL_DIRECTEFFECTFLAGS_APPLYDIRECTIVITY);
        if (Source.bApplyOcclusion)
            Params.flags = static_cast<IPLDirectEffectFlags>(Params.flags | IPL_DIRECTEFFECTFLAGS_APPLYOCCLUSION);
        if (Source.bApplyTransmission)
            Params.flags = static_cast<IPLDirectEffectFlags>(Params.flags | IPL_DIRECTEFFECTFLAGS_APPLYTRANSMISSION);

        // If enabled, calculate physics-based distance attenuation using the default model.
        if (Source.bApplyDistanceAttenuation)
        {
            IPLDistanceAttenuationModel DistanceAttenuationModel{};
            DistanceAttenuationModel.type = IPL_DISTANCEATTENUATIONTYPE_DEFAULT;

            Params.distanceAttenuation = iplDistanceAttenuationCalculate(Context, SourceCoordinates.origin, ListenerCoordinates.origin, &DistanceAttenuationModel);
        }

        // If enabled, calculate frequency-dependent air absorption using the default model.
        if (Source.bApplyAirAbsorption)
        {
            IPLAirAbsorptionModel AirAbsorptionModel{};
            AirAbsorptionModel.type = IPL_AIRABSORPTIONTYPE_DEFAULT;

            iplAirAbsorptionCalculate(Context, SourceCoordinates.origin, ListenerCoordinates.origin, &AirAbsorptionModel, Params.airAbsorption);
        }

        // If enabled, calculate directivity using the configured dipole model.
        if (Source.bApplyDirectivity)
        {
            IPLDirectivity DirectivityModel{};
            DirectivityModel.dipoleWeight = Source.DipoleWeight;
            DirectivityModel.dipolePower = Source.DipolePower;

            Params.directivity = iplDirectivityCalculate(Context, SourceCoordinates, ListenerCoordinates.origin, &DirectivityModel);
        }

        // If enabled, retrieve occlusion (and optionally transmission) values from the actor's Steam Audio Source
        // component.
        if (Source.bApplyOcclusion)
        {
            UAudioComponent* AudioComponent = UAudioComponent::GetAudioComponentFromID(InputData.AudioComponentId);
            USteamAudioSourceComponent* SteamAudioSourceComponent = (AudioComponent) ? AudioComponent->GetOwner()->FindComponentByClass<USteamAudioSourceComponent>() : nullptr;

            Params.occlusion = (SteamAudioSourceComponent) ? SteamAudioSourceComponent->OcclusionValue : 1.0f;

            if (Source.bApplyTransmission)
            {
                Params.transmissionType = static_cast<IPLTransmissionType>(Source.TransmissionType);

                Params.transmission[0] = (SteamAudioSourceComponent) ? SteamAudioSourceComponent->TransmissionLowValue : 1.0f;
                Params.transmission[1] = (SteamAudioSourceComponent) ? SteamAudioSourceComponent->TransmissionMidValue : 1.0f;
                Params.transmission[2] = (SteamAudioSourceComponent) ? SteamAudioSourceComponent->TransmissionHighValue : 1.0f;
            }
        }

        // Apply the direct effect.
        iplDirectEffectApply(Source.DirectEffect, &Params, &Source.InBuffer, &Source.OutBuffer);

        // Interleave the output buffer.
        iplAudioBufferInterleave(Context, &Source.OutBuffer, OutBufferData);
    }
}


// ---------------------------------------------------------------------------------------------------------------------
// FSteamAudioOcclusionPluginFactory
// ---------------------------------------------------------------------------------------------------------------------

FString FSteamAudioOcclusionPluginFactory::GetDisplayName()
{
    static FString DisplayName = FString(TEXT("Steam Audio Occlusion"));
    return DisplayName;
}

bool FSteamAudioOcclusionPluginFactory::SupportsPlatform(const FString& PlatformName)
{
    return PlatformName == FString(TEXT("Windows")) ||
        PlatformName == FString(TEXT("Linux")) ||
        PlatformName == FString(TEXT("Mac")) ||
        PlatformName == FString(TEXT("Android")) ||
        PlatformName == FString(TEXT("IOS"));
}

UClass* FSteamAudioOcclusionPluginFactory::GetCustomOcclusionSettingsClass() const
{
    return USteamAudioOcclusionSettings::StaticClass();
}

TAudioOcclusionPtr FSteamAudioOcclusionPluginFactory::CreateNewOcclusionPlugin(FAudioDevice* OwningDevice)
{
    FSteamAudioModule::Get().RegisterAudioDevice(OwningDevice);
    return TAudioOcclusionPtr(new FSteamAudioOcclusionPlugin());
}

}
