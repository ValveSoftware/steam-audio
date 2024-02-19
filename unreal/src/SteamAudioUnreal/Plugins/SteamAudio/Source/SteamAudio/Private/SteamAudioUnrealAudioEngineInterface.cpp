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

#include "SteamAudioUnrealAudioEngineInterface.h"
#include "AudioDevice.h"
#include "SteamAudioReverb.h"

namespace SteamAudio {

// ---------------------------------------------------------------------------------------------------------------------
// FUnrealAudioEngineState
// ---------------------------------------------------------------------------------------------------------------------

void FUnrealAudioEngineState::Initialize(IPLContext Context, IPLHRTF HRTF, const IPLSimulationSettings& SimulationSettings)
{}

void FUnrealAudioEngineState::Destroy()
{}

void FUnrealAudioEngineState::SetHRTF(IPLHRTF HRTF)
{}

void FUnrealAudioEngineState::SetReverbSource(IPLSource Source)
{
    FSteamAudioReverbSubmixPlugin::SetReverbSource(Source);
}

FTransform FUnrealAudioEngineState::GetListenerTransform()
{
    FTransform Transform;

    FAudioDeviceManager* AudioDeviceManager = FAudioDeviceManager::Get();
    if (AudioDeviceManager)
    {
        FAudioDeviceHandle AudioDevice = AudioDeviceManager->GetActiveAudioDevice();
        if (AudioDevice.IsValid())
        {
            AudioDevice->GetListenerTransform(0, Transform);
        }
    }

    return Transform;
}

IPLAudioSettings FUnrealAudioEngineState::GetAudioSettings()
{
    IPLAudioSettings AudioSettings{};
    AudioSettings.samplingRate = 48000;
    AudioSettings.frameSize = 1024;

    FAudioDeviceManager* AudioDeviceManager = FAudioDeviceManager::Get();
    if (AudioDeviceManager)
    {
        FAudioDeviceHandle AudioDevice = AudioDeviceManager->GetActiveAudioDevice();
        if (AudioDevice.IsValid())
        {
            FAudioPlatformSettings AudioPlatformSettings = AudioDevice->PlatformSettings;
            AudioSettings.samplingRate = AudioPlatformSettings.SampleRate;
            AudioSettings.frameSize = AudioPlatformSettings.CallbackBufferFrameSize;
        }
    }

    return AudioSettings;
}

TSharedPtr<IAudioEngineSource> FUnrealAudioEngineState::CreateAudioEngineSource()
{
    return MakeShared<FUnrealAudioEngineSource>();
}


// ---------------------------------------------------------------------------------------------------------------------
// FUnrealAudioEngineSource
// ---------------------------------------------------------------------------------------------------------------------

void FUnrealAudioEngineSource::Initialize(AActor* Actor)
{}

void FUnrealAudioEngineSource::Destroy()
{}

void FUnrealAudioEngineSource::UpdateParameters(USteamAudioSourceComponent* Source)
{}

}
