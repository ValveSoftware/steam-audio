//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

#include "audio_engine_settings.h"
#include "auto_load_library.h"

std::mutex                      GlobalState::sMutex{};
std::shared_ptr<GlobalState>    GlobalState::sAudioEngineSettings{ nullptr };

GlobalState::GlobalState(const IPLRenderingSettings& renderingSettings, 
                         const IPLAudioFormat& outputFormat)
{
    mRenderingSettings = renderingSettings;
    mOutputFormat = outputFormat;

    if (gApi.iplCreateContext(nullptr, nullptr, nullptr, &mContext) != IPL_STATUS_SUCCESS)
        throw std::exception();

    IPLHrtfParams hrtfParams{ IPL_HRTFDATABASETYPE_DEFAULT, nullptr, 0, nullptr, nullptr, nullptr };

    if (gApi.iplCreateBinauralRenderer(mContext, renderingSettings, hrtfParams, &mBinauralRenderer) != IPL_STATUS_SUCCESS)
        throw std::exception();
}

GlobalState::~GlobalState()
{
    if (mBinauralRenderer)
        gApi.iplDestroyBinauralRenderer(&mBinauralRenderer);

    if (mContext)
        gApi.iplDestroyContext(&mContext);
}

IPLRenderingSettings GlobalState::renderingSettings() const
{
    return mRenderingSettings;
}

IPLAudioFormat GlobalState::outputFormat() const
{
    return mOutputFormat;
}

IPLhandle GlobalState::context() const
{
    return mContext;
}

IPLhandle GlobalState::binauralRenderer() const
{
    return mBinauralRenderer;
}

std::shared_ptr<GlobalState> GlobalState::get()
{
    std::lock_guard<std::mutex> lock(sMutex);
    return sAudioEngineSettings;
}

void GlobalState::create(const IPLRenderingSettings& renderingSettings, 
                         const IPLAudioFormat& outputFormat)
{
    std::lock_guard<std::mutex> lock(sMutex);
    sAudioEngineSettings = std::make_shared<GlobalState>(renderingSettings, outputFormat);
}

void GlobalState::destroy()
{
    std::lock_guard<std::mutex> lock(sMutex);
    sAudioEngineSettings = nullptr;
}

void F_CALL iplFmodResetAudioEngine()
{
    GlobalState::destroy();
}