//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

#include <algorithm>
#include <future>
#include <queue>
#include <string>
#include <vector>
#include "audio_engine_settings.h"
#include "auto_load_library.h"

// --------------------------------------------------------------------------------------------------------------------
// Class Data
// --------------------------------------------------------------------------------------------------------------------

std::mutex                                              AudioEngineSettings::sMutex{};
std::vector<std::string>                                AudioEngineSettings::sSOFAFileNames{};
std::unordered_map<std::string, BinauralRendererInfo>   AudioEngineSettings::sBinauralRenderers{};
int                                                     AudioEngineSettings::sCurrentSOFAFileIndex{ 0 };
std::shared_ptr<AudioEngineSettings>                    AudioEngineSettings::sAudioEngineSettings{ nullptr };

// --------------------------------------------------------------------------------------------------------------------
// Instance Methods
// --------------------------------------------------------------------------------------------------------------------

AudioEngineSettings::AudioEngineSettings(
    const IPLRenderingSettings& renderingSettings, 
    const IPLAudioFormat&       outputFormat)
{
    mRenderingSettings = renderingSettings;
    mOutputFormat = outputFormat;

	auto status = gApi.iplCreateContext(nullptr, nullptr, nullptr, &mContext);
	if (status != IPL_STATUS_SUCCESS) {
		throw std::exception();
	}
}

AudioEngineSettings::~AudioEngineSettings()
{
    if (mContext) {
        gApi.iplDestroyContext(&mContext);
    }
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

IPLhandle AudioEngineSettings::binauralRenderer(const int index) const
{
    std::lock_guard<std::mutex> lock(sMutex);
    
    if (index < 0 || sSOFAFileNames.size() <= index) {
        return nullptr;
    }

    const auto& fileName = sSOFAFileNames[index];
    if (sBinauralRenderers.find(fileName) == sBinauralRenderers.end()) {
        return nullptr;
    }

    auto& binauralRendererInfo = sBinauralRenderers[fileName];
    auto& future = binauralRendererInfo.future;
    if (future.valid() && future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        binauralRendererInfo.binauralRenderer = binauralRendererInfo.future.get();
    }

    return binauralRendererInfo.binauralRenderer;
}

IPLhandle AudioEngineSettings::binauralRenderer()
{
	return binauralRenderer(sCurrentSOFAFileIndex);
}

// --------------------------------------------------------------------------------------------------------------------
// Class Methods
// --------------------------------------------------------------------------------------------------------------------

void AudioEngineSettings::create(
    const IPLRenderingSettings& renderingSettings,
    const IPLAudioFormat&       outputFormat)
{
    std::lock_guard<std::mutex> lock(sMutex);
    sAudioEngineSettings = std::make_shared<AudioEngineSettings>(renderingSettings, outputFormat);
    
	queueSOFAFile("");
}

void AudioEngineSettings::destroy()
{
    std::lock_guard<std::mutex> lock(sMutex);

    for (auto& sofaFileName : sSOFAFileNames) {
        removeSOFAFile(sofaFileName.c_str());
    }
    sBinauralRenderers.clear();
    sSOFAFileNames.clear();

    sAudioEngineSettings = nullptr;
}

std::shared_ptr<AudioEngineSettings> AudioEngineSettings::get()
{
    std::lock_guard<std::mutex> lock(sMutex);
    return sAudioEngineSettings;
}

void AudioEngineSettings::queueSOFAFile(const char* sofaFileName)
{
    if (sBinauralRenderers.find(sofaFileName) == sBinauralRenderers.end()) {
        sSOFAFileNames.push_back(sofaFileName);
        sBinauralRenderers[sofaFileName] = BinauralRendererInfo{};
        sBinauralRenderers[sofaFileName].pending = true;
    }

    createPendingBinauralRenderers();
}

int AudioEngineSettings::addSOFAFile(const char* sofaFileName)
{
	std::lock_guard<std::mutex> lock(sMutex);

	queueSOFAFile(sofaFileName);
	return sofaFileIndex(sofaFileName);
}

void AudioEngineSettings::removeSOFAFile(const char* sofaFileName)
{
    if (sBinauralRenderers.find(sofaFileName) != sBinauralRenderers.end()) {
        sBinauralRenderers[sofaFileName].pending = false;
        gApi.iplDestroyBinauralRenderer(&sBinauralRenderers[sofaFileName].binauralRenderer);
    }
}

void AudioEngineSettings::setCurrentSOFAFile(const int index)
{
    std::lock_guard<std::mutex> lock(sMutex);
    sCurrentSOFAFileIndex = index;
}

int AudioEngineSettings::sofaFileIndex(const char* sofaFileName)
{
    return (int) std::distance(sSOFAFileNames.begin(), 
                               std::find(sSOFAFileNames.begin(), sSOFAFileNames.end(), sofaFileName));
}

void AudioEngineSettings::createPendingBinauralRenderers()
{
    if (!sAudioEngineSettings) {
        return;
    }

    for (auto& keyValue : sBinauralRenderers) {
        const auto& sofaFileName = keyValue.first;
        auto& binauralRendererInfo = keyValue.second;

        if (binauralRendererInfo.pending) {
            binauralRendererInfo.pending = false;

            auto context = sAudioEngineSettings->mContext;
            auto renderingSettings = sAudioEngineSettings->mRenderingSettings;
            auto hrtfParams = IPLHrtfParams{};
            hrtfParams.type = (sofaFileName == "") ? IPL_HRTFDATABASETYPE_DEFAULT : IPL_HRTFDATABASETYPE_SOFA;
            hrtfParams.sofaFileName = (char*) sofaFileName.c_str();

            //auto& sofaMutex = sAudioEngineSettings->mSOFAMutex;

            binauralRendererInfo.future = 
                sAudioEngineSettings->mWorkerThread.addTask([/*&sofaMutex, */context, renderingSettings, hrtfParams]() 
            {
                //std::lock_guard<std::mutex> lock(sofaMutex);
                auto binauralRenderer = IPLhandle{ nullptr };
                auto status = gApi.iplCreateBinauralRenderer(context, renderingSettings, hrtfParams, &binauralRenderer);
                if (status != IPL_STATUS_SUCCESS) {
                    throw std::exception();
                }
                return binauralRenderer;
            });
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------
// Exported Functions
// --------------------------------------------------------------------------------------------------------------------

void F_CALL iplFmodResetAudioEngine()
{
	AudioEngineSettings::destroy();
}

int F_CALL iplFmodAddSOFAFileName(char* sofaFileName)
{
    return AudioEngineSettings::addSOFAFile(sofaFileName);
}

void F_CALL iplFmodRemoveSOFAFileName(char* sofaFileName)
{
    AudioEngineSettings::removeSOFAFile(sofaFileName);
}

void F_CALL iplFmodSetCurrentSOFAFile(int index)
{
    AudioEngineSettings::setCurrentSOFAFile(index);
}
