//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

#include "environment_proxy.h"
#include "auto_load_library.h"

std::mutex                          EnvironmentProxy::sMutex{};
std::shared_ptr<EnvironmentProxy>   EnvironmentProxy::sEnvironmentProxy{ nullptr };
bool                                EnvironmentProxy::sEnvironmentHasReset{ false };

EnvironmentProxy::EnvironmentProxy(const IPLSimulationSettings& simulationSettings, const IPLhandle environment,
    const IPLConvolutionType convolutionType) :
    mSimulationSettings(simulationSettings),
    mEnvironment(environment),
    mEnvironmentCopy(nullptr),
    mConvolutionType(convolutionType),
    mEnvironmentalRenderer(nullptr),
    mUsingAcceleratedMixing(false)
{
    mListenerPosition = IPLVector3{ 0.0f, 0.0f, 0.0f };
    mListenerAhead = IPLVector3{ 0.0f, 0.0f, -1.0f };
    mListenerUp = IPLVector3{ 0.0f, 1.0f, 0.0f };
}

EnvironmentProxy::~EnvironmentProxy()
{
    if (mEnvironmentalRendererFuture.valid()) {
        mEnvironmentalRendererFuture.get();
    }

    if (mEnvironmentCopy)
        gApi.iplDestroyEnvironment(&mEnvironmentCopy);

    if (mEnvironmentalRenderer)
        gApi.iplDestroyEnvironmentalRenderer(&mEnvironmentalRenderer);
}

IPLSimulationSettings EnvironmentProxy::simulationSettings() const
{
    return mSimulationSettings;
}

IPLhandle EnvironmentProxy::environment() const
{
    if (mEnvironmentalRenderer && !mEnvironmentCopy)
        mEnvironmentCopy = gApi.iplGetEnvironmentForRenderer(mEnvironmentalRenderer);

    return (mEnvironmentCopy) ? mEnvironmentCopy : mEnvironment;
}

IPLhandle EnvironmentProxy::environmentalRenderer()
{
    if (!mEnvironment)
        return nullptr;

    auto audioEngineSettings = AudioEngineSettings::get();

    if (!audioEngineSettings)
        return nullptr;

    if (!mEnvironmentalRenderer)
    {
        if (!mEnvironmentalRendererFuture.valid())
        {
            auto ambisonicsOrder = mSimulationSettings.ambisonicsOrder;
            auto numChannels = (ambisonicsOrder + 1) * (ambisonicsOrder + 1);

            IPLAudioFormat outputFormat{ IPL_CHANNELLAYOUTTYPE_AMBISONICS, IPL_CHANNELLAYOUT_CUSTOM, numChannels, 
                nullptr, ambisonicsOrder, IPL_AMBISONICSORDERING_ACN, IPL_AMBISONICSNORMALIZATION_N3D,
                IPL_CHANNELORDER_DEINTERLEAVED };

            auto renderingSettings = audioEngineSettings->renderingSettings();
            renderingSettings.convolutionType = mConvolutionType;

            auto iplCreateEnvironmentalRenderer = [](IPLhandle context, IPLhandle environment,
                                                     IPLRenderingSettings renderingSettings, 
                                                     IPLAudioFormat outputFormat)
            {
                IPLhandle renderer = nullptr;
                gApi.iplCreateEnvironmentalRenderer(context, environment, renderingSettings, outputFormat, nullptr,
                                                    nullptr, &renderer);
                return renderer;
            };

            mEnvironmentalRendererFuture = std::async(std::launch::async, iplCreateEnvironmentalRenderer, 
                                                      audioEngineSettings->context(), mEnvironment, renderingSettings, 
                                                      outputFormat);
        }

        if (mEnvironmentalRendererFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            mEnvironmentalRenderer = mEnvironmentalRendererFuture.get();
        }
        else
        {
            return nullptr;
        }
    }

    return mEnvironmentalRenderer;
}

IPLConvolutionType EnvironmentProxy::convolutionType() const
{
    return mConvolutionType;
}

bool EnvironmentProxy::isUsingAcceleratedMixing() const
{
    return mUsingAcceleratedMixing;
}

void EnvironmentProxy::setUsingAcceleratedMixing(const bool usingAcceleratedMixing)
{
    mUsingAcceleratedMixing = usingAcceleratedMixing;
}

IPLVector3 EnvironmentProxy::listenerPosition() const
{
    return mListenerPosition;
}

IPLVector3 EnvironmentProxy::listenerAhead() const
{
    return mListenerAhead;
}

IPLVector3 EnvironmentProxy::listenerUp() const
{
    return mListenerUp;
}

void EnvironmentProxy::setListener(const IPLVector3& position, const IPLVector3& ahead, const IPLVector3& up)
{
    mListenerPosition = position;
    mListenerAhead = ahead;
    mListenerUp = up;
}

void EnvironmentProxy::setEnvironment(const IPLSimulationSettings& simulationSettings, const IPLhandle environment,
    const IPLConvolutionType convolutionType)
{
    std::lock_guard<std::mutex> lock(sMutex);
    sEnvironmentProxy = std::make_shared<EnvironmentProxy>(simulationSettings, environment, convolutionType);
}

void EnvironmentProxy::resetEnvironment()
{
    std::lock_guard<std::mutex> lock(sMutex);
    sEnvironmentProxy = nullptr;
    sEnvironmentHasReset = true;
}

bool EnvironmentProxy::hasEnvironmentReset()
{
    std::lock_guard<std::mutex> lock(sMutex);
    return sEnvironmentHasReset;
}

void EnvironmentProxy::acknowledgeEnvironmentReset()
{
    std::lock_guard<std::mutex> lock(sMutex);
    sEnvironmentHasReset = false;
}

void EnvironmentProxy::setListenerGlobal(const IPLVector3& position, const IPLVector3& ahead, const IPLVector3& up)
{
    std::lock_guard<std::mutex> lock(sMutex);
    if (sEnvironmentProxy)
        sEnvironmentProxy->setListener(position, ahead, up);
}

std::shared_ptr<EnvironmentProxy> EnvironmentProxy::get()
{
    std::lock_guard<std::mutex> lock(sMutex);
    return sEnvironmentProxy;
}

void UNITY_AUDIODSP_CALLBACK iplUnitySetEnvironment(IPLSimulationSettings simulationSettings, IPLhandle environment,
    IPLConvolutionType convolutionType)
{
    EnvironmentProxy::setEnvironment(simulationSettings, environment, convolutionType);
}

void UNITY_AUDIODSP_CALLBACK iplUnityResetEnvironment()
{
    EnvironmentProxy::resetEnvironment();
}

void UNITY_AUDIODSP_CALLBACK iplUnitySetListener(IPLVector3 position, IPLVector3 ahead, IPLVector3 up)
{
    EnvironmentProxy::setListenerGlobal(position, ahead, up);
}
