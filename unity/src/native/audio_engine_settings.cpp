//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

#include "audio_engine_settings.h"
#include "auto_load_library.h"

std::mutex                                          AudioEngineSettings::sMutex{};
std::shared_ptr<AudioEngineSettings>                AudioEngineSettings::sAudioEngineSettings{ nullptr };
std::future<std::shared_ptr<AudioEngineSettings>>   AudioEngineSettings::sAudioEngineSettingsFuture{};

AudioEngineSettings::AudioEngineSettings(const IPLRenderingSettings& renderingSettings, 
    const IPLAudioFormat& outputFormat)
{
    mRenderingSettings = renderingSettings;
    mOutputFormat = outputFormat;

    gApi.iplCreateContext(nullptr, nullptr, nullptr, &mContext);
    
    IPLHrtfParams hrtfParams{ IPL_HRTFDATABASETYPE_DEFAULT, nullptr, 0, nullptr, nullptr, nullptr };

    if (gApi.iplCreateBinauralRenderer(mContext, renderingSettings, hrtfParams, &mBinauralRenderer) != IPL_STATUS_SUCCESS)
        throw std::exception();
}

AudioEngineSettings::~AudioEngineSettings()
{
    if (mBinauralRenderer)
        gApi.iplDestroyBinauralRenderer(&mBinauralRenderer);
    
    if (mContext)
        gApi.iplDestroyContext(&mContext);
}

IPLhandle AudioEngineSettings::context() const
{
    return mContext;
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

    if (sAudioEngineSettingsFuture.valid() &&
        sAudioEngineSettingsFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
    {
        sAudioEngineSettings = sAudioEngineSettingsFuture.get();
    }

    return sAudioEngineSettings;
}

void AudioEngineSettings::create(const IPLRenderingSettings& renderingSettings, const IPLAudioFormat& outputFormat)
{
    std::lock_guard<std::mutex> lock(sMutex);
    
    if (!sAudioEngineSettingsFuture.valid())
    {
        auto constructAudioEngineSettings = [](IPLRenderingSettings renderingSettings, IPLAudioFormat outputFormat)
        {
            return std::make_shared<AudioEngineSettings>(renderingSettings, outputFormat);
        };

        sAudioEngineSettingsFuture = std::async(std::launch::async, constructAudioEngineSettings, renderingSettings,
            outputFormat);
    }
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
