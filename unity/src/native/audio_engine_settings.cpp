//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

#include "audio_engine_settings.h"
#include "auto_load_library.h"

std::mutex                              AudioEngineSettings::sMutex{};
std::shared_ptr<AudioEngineSettings>    AudioEngineSettings::sAudioEngineSettings{ nullptr };

AudioEngineSettings::AudioEngineSettings(const IPLRenderingSettings& renderingSettings, 
    const IPLAudioFormat& outputFormat)
{
    mRenderingSettings = renderingSettings;
    mOutputFormat = outputFormat;

    IPLContext context{ nullptr, nullptr, nullptr };
    IPLHrtfParams hrtfParams{ IPL_HRTFDATABASETYPE_DEFAULT, nullptr, 0, nullptr, nullptr, nullptr };

    if (gApi.iplCreateBinauralRenderer(context, renderingSettings, hrtfParams, &mBinauralRenderer) != IPL_STATUS_SUCCESS)
        throw std::exception();
}

AudioEngineSettings::~AudioEngineSettings()
{
    if (mBinauralRenderer)
        gApi.iplDestroyBinauralRenderer(&mBinauralRenderer);
}

IPLRenderingSettings AudioEngineSettings::renderingSettings() const
{
    std::lock_guard<std::mutex> lock(sMutex);
    return mRenderingSettings;
}

IPLAudioFormat AudioEngineSettings::outputFormat() const
{
    std::lock_guard<std::mutex> lock(sMutex);
    return mOutputFormat;
}

IPLhandle AudioEngineSettings::binauralRenderer() const
{
    std::lock_guard<std::mutex> lock(sMutex);
    return mBinauralRenderer;
}

std::shared_ptr<AudioEngineSettings> AudioEngineSettings::get()
{
    std::lock_guard<std::mutex> lock(sMutex);
    return sAudioEngineSettings;
}

void AudioEngineSettings::create(const IPLRenderingSettings& renderingSettings, const IPLAudioFormat& outputFormat)
{
    std::lock_guard<std::mutex> lock(sMutex);
    sAudioEngineSettings = std::make_shared<AudioEngineSettings>(renderingSettings, outputFormat);
}

void AudioEngineSettings::destroy()
{
    std::lock_guard<std::mutex> lock(sMutex);
    sAudioEngineSettings = nullptr;
}

void UNITY_AUDIODSP_CALLBACK iplUnityResetAudioEngine()
{
    AudioEngineSettings::destroy();
}