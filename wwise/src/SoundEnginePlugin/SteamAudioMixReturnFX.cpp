/*******************************************************************************
The content of this file includes portions of the AUDIOKINETIC Wwise Technology
released in source code form as part of the SDK installer package.

Commercial License Usage

Licensees holding valid commercial licenses to the AUDIOKINETIC Wwise Technology
may use this file in accordance with the end user license agreement provided
with the software or, alternatively, in accordance with the terms contained in a
written agreement between you and Audiokinetic Inc.

Apache License Usage

Alternatively, this file may be used under the Apache License, Version 2.0 (the
"Apache License"); you may not use this file except in compliance with the
Apache License. You may obtain a copy of the Apache License at
http://www.apache.org/licenses/LICENSE-2.0.

Unless required by applicable law or agreed to in writing, software distributed
under the Apache License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES
OR CONDITIONS OF ANY KIND, either express or implied. See the Apache License for
the specific language governing permissions and limitations under the License.

  Copyright (c) 2024 Audiokinetic Inc.
*******************************************************************************/

#include "SteamAudioMixReturnFX.h"
#include "../SteamAudioWwiseConfig.h"

#include <algorithm>
#include <AK/AkWwiseSDKVersion.h>

#include "SteamAudioCommon.h"

// --------------------------------------------------------------------------------------------------------------------
// SteamAudioMixReturnFX
// --------------------------------------------------------------------------------------------------------------------

SteamAudioMixReturnFX::SteamAudioMixReturnFX()
    : m_params(nullptr)
    , m_allocator(nullptr)
    , m_context(nullptr)
    , m_reflectionMixer(nullptr)
    , m_ambisonicsDecodeEffect(nullptr)
    , m_inBuffer()
    , m_monoBuffer()
    , m_outBuffer()
    , m_ambisonicsBuffer()
{}


SteamAudioMixReturnFX::~SteamAudioMixReturnFX() = default;


bool SteamAudioMixReturnFX::IsAudioFormatSupported(const AkAudioFormat& in_rFormat)
{
    AkChannelConfig channelConfig = in_rFormat.channelConfig;
    if (!channelConfig.IsValid())
        return false;

    if (channelConfig.eConfigType != AK_ChannelConfigType_Standard)
        return false;

    if (channelConfig.uChannelMask != AK_SPEAKER_SETUP_MONO &&
        channelConfig.uChannelMask != AK_SPEAKER_SETUP_STEREO &&
        channelConfig.uChannelMask != AK_SPEAKER_SETUP_4 &&
        channelConfig.uChannelMask != AK_SPEAKER_SETUP_5POINT1 &&
        channelConfig.uChannelMask != AK_SPEAKER_SETUP_7POINT1)
    {
        return false;
    }

    return true;
}


AKRESULT SteamAudioMixReturnFX::Init(AK::IAkPluginMemAlloc* in_pAllocator, AK::IAkEffectPluginContext* in_pEffectPluginContext, AK::IAkPluginParam* in_pParams, AkAudioFormat& io_rFormat)
{
    if (!IsAudioFormatSupported(io_rFormat))
        return AK_UnsupportedChannelConfig;

    m_params = (SteamAudioMixReturnFXParams*) in_pParams;
    m_allocator = in_pAllocator;
    m_context = in_pEffectPluginContext;

    m_format = io_rFormat;

    auto& globalState = SteamAudioWwise::GlobalState::Get();
    globalState.Retain();
    
    LazyInit();

    // We want to consider this effect instance as initialized even if the Steam Audio initialization didn't succeed.
    // We'll keep trying to initialize in subsequent frames.
    return AK_Success;
}

AKRESULT SteamAudioMixReturnFX::LazyInit()
{
    if (m_reflectionMixer && 
        m_ambisonicsDecodeEffect && 
        m_inBuffer.data && 
        m_monoBuffer.data && 
        m_outBuffer.data &&
        m_ambisonicsBuffer.data)
    {
        return AK_Success;
    }

	AkAudioSettings wwiseAudioSettings{};
	m_context->GlobalContext()->GetAudioSettings(wwiseAudioSettings);

	IPLAudioSettings audioSettings{};
	audioSettings.samplingRate = wwiseAudioSettings.uNumSamplesPerSecond;
	audioSettings.frameSize = wwiseAudioSettings.uNumSamplesPerFrame;

    if (!SteamAudioWwise::EnsureSteamAudioContextExists(audioSettings, m_context->GlobalContext()))
        return AK_NotInitialized;

    auto& globalState = SteamAudioWwise::GlobalState::Get();

    auto context = globalState.context.Read();
    auto hrtf = globalState.hrtf.Read();

    if (!m_reflectionMixer && globalState.simulationSettingsValid)
    {
        IPLReflectionEffectSettings reflectionEffectSettings{};
        reflectionEffectSettings.type = globalState.simulationSettings.reflectionType;
        reflectionEffectSettings.numChannels = SteamAudioWwise::NumChannelsForOrder(globalState.simulationSettings.maxOrder);
        reflectionEffectSettings.irSize = SteamAudioWwise::NumSamplesForDuration(globalState.simulationSettings.maxDuration, audioSettings.samplingRate);

        if (iplReflectionMixerCreate(context, &audioSettings, &reflectionEffectSettings, &m_reflectionMixer) != IPL_STATUS_SUCCESS)
            return AK_NotInitialized;

        globalState.reflectionMixer.Write(m_reflectionMixer);
    }

    if (!m_ambisonicsDecodeEffect && globalState.simulationSettingsValid)
    {
        IPLAmbisonicsDecodeEffectSettings ambisonicsDecodeEffectSettings{};
        ambisonicsDecodeEffectSettings.maxOrder = globalState.simulationSettings.maxOrder;
        ambisonicsDecodeEffectSettings.speakerLayout = SteamAudioWwise::SpeakerLayoutForNumChannels(m_format.GetNumChannels());
        ambisonicsDecodeEffectSettings.hrtf = hrtf;

        if (iplAmbisonicsDecodeEffectCreate(context, &audioSettings, &ambisonicsDecodeEffectSettings, &m_ambisonicsDecodeEffect) != IPL_STATUS_SUCCESS)
            return AK_NotInitialized;
    }

    if (!m_inBuffer.data)
    {
        if (iplAudioBufferAllocate(context, m_format.GetNumChannels(), audioSettings.frameSize, &m_inBuffer) != IPL_STATUS_SUCCESS)
            return AK_NotInitialized;
    }

    if (!m_monoBuffer.data)
    {
        if (iplAudioBufferAllocate(context, 1, audioSettings.frameSize, &m_monoBuffer) != IPL_STATUS_SUCCESS)
            return AK_NotInitialized;
    }

    if (!m_outBuffer.data)
    {
        if (iplAudioBufferAllocate(context, m_format.GetNumChannels(), audioSettings.frameSize, &m_outBuffer) != IPL_STATUS_SUCCESS)
            return AK_NotInitialized;
    }

    if (!m_ambisonicsBuffer.data && globalState.simulationSettingsValid)
    {
        if (iplAudioBufferAllocate(context, SteamAudioWwise::NumChannelsForOrder(globalState.simulationSettings.maxOrder), audioSettings.frameSize, &m_ambisonicsBuffer) != IPL_STATUS_SUCCESS)
            return AK_NotInitialized;
    }

    return AK_Success;
}


AKRESULT SteamAudioMixReturnFX::Term(AK::IAkPluginMemAlloc* in_pAllocator)
{
    auto& globalState = SteamAudioWwise::GlobalState::Get();

    auto context = globalState.context.Read();

    iplAudioBufferFree(context, &m_outBuffer);
    iplAudioBufferFree(context, &m_inBuffer);
    iplAudioBufferFree(context, &m_monoBuffer);
    iplAudioBufferFree(context, &m_ambisonicsBuffer);

    iplReflectionMixerRelease(&m_reflectionMixer);
    iplAmbisonicsDecodeEffectRelease(&m_ambisonicsDecodeEffect);

    globalState.Release();
    
    AK_PLUGIN_DELETE(in_pAllocator, this);

    return AK_Success;
}


AKRESULT SteamAudioMixReturnFX::Reset()
{
    iplReflectionMixerReset(m_reflectionMixer);
    iplAmbisonicsDecodeEffectReset(m_ambisonicsDecodeEffect);

    return AK_Success;
}


AKRESULT SteamAudioMixReturnFX::GetPluginInfo(AkPluginInfo& out_rPluginInfo)
{
    out_rPluginInfo.eType = AkPluginTypeEffect;
    out_rPluginInfo.bIsInPlace = false;
    out_rPluginInfo.bCanProcessObjects = false;
    out_rPluginInfo.uBuildVersion = AK_WWISESDK_VERSION_COMBINED;

    return AK_Success;
}

void SteamAudioMixReturnFX::Execute(AkAudioBuffer* in_pBuffer, AkUInt32 in_uInOffset, AkAudioBuffer* out_pBuffer)
{
    AKASSERT(in_pBuffer->uValidFrames == out_pBuffer->MaxFrames());
    AKASSERT(in_uInOffset == 0);

    // -- clear actual output

    for (AkUInt32 i = 0; i < out_pBuffer->NumChannels(); ++i)
    {
        memset(out_pBuffer->GetChannel(i), 0, out_pBuffer->MaxFrames() * sizeof(float));
    }

    // -- ensure everything is initialized

    if (LazyInit() != AK_Success)
    {
        out_pBuffer->eState = AK_Fail;
        return;
    }

    auto& globalState = SteamAudioWwise::GlobalState::Get();

    auto context = globalState.context.Read();
    auto hrtf = globalState.hrtf.Read();

    // -- clear input and output

    for (IPLint32 i = 0; i < m_inBuffer.numChannels; ++i)
    {
        memset(m_inBuffer.data[i], 0, m_inBuffer.numSamples * sizeof(IPLfloat32));
    }

    for (IPLint32 i = 0; i < m_outBuffer.numChannels; ++i)
    {
        memset(m_outBuffer.data[i], 0, m_outBuffer.numSamples * sizeof(IPLfloat32));
    }

    // -- copy input

    auto numSamplesConsumed = std::min(in_pBuffer->uValidFrames, (AkUInt16) m_inBuffer.numSamples);
    for (AkUInt32 i = 0; i < std::min(in_pBuffer->NumChannels(), (AkUInt32) m_inBuffer.numChannels); ++i)
    {
        memcpy(m_inBuffer.data[i], in_pBuffer->GetChannel(i), numSamplesConsumed * sizeof(IPLfloat32));
    }

    // -- calculate source and listener positions

    bool bUseGamePosition = !SteamAudioWwise::IsRunningInEditor();
    IPLCoordinateSpace3 listenerCoords{};

    if (!bUseGamePosition)
    {
        listenerCoords.origin = { 0, 0, 0 };
        listenerCoords.right = { 1, 0, 0 };
        listenerCoords.up = { 0, 1, 0 };
        listenerCoords.ahead = { 0, 0, -1 };
    }
    else
    {
        AkSoundPosition listenerTransform;
        {
            AkListener listenerObject;
            AkGameObjectID firstListener;
            AkUInt32 nListeners = 1;
            if (m_context->GetGameObjectInfo()->GetListeners(&firstListener, nListeners) != AK_Success)
            {
                out_pBuffer->eState = AK_Fail;
                return;
            }

            if (nListeners > 1)
            {
                out_pBuffer->eState = AK_Fail;
                return;
            }

            if (m_context->GetGameObjectInfo()->GetListenerData(firstListener, listenerObject) != AK_Success)
            {
                out_pBuffer->eState = AK_Fail;
                return;
            }

            listenerTransform = listenerObject.position;
        }

        listenerCoords = SteamAudioWwise::CalculateCoordinates(listenerTransform);
    }

    // -- apply reflections

    if (globalState.simulationSettingsValid)
    {
        iplAudioBufferDownmix(context, &m_inBuffer, &m_monoBuffer);

        IPLReflectionEffectParams reflectionParams{};
        reflectionParams.numChannels = SteamAudioWwise::NumChannelsForOrder(globalState.simulationSettings.maxOrder);
        reflectionParams.tanDevice = globalState.simulationSettings.tanDevice;

        iplReflectionMixerApply(m_reflectionMixer, &reflectionParams, &m_ambisonicsBuffer);

        IPLAmbisonicsDecodeEffectParams ambisonicsDecodeParams{};
        ambisonicsDecodeParams.order = globalState.simulationSettings.maxOrder;
        ambisonicsDecodeParams.binaural = m_params->NonRTPC.binaural ? IPL_TRUE : IPL_FALSE;
        ambisonicsDecodeParams.hrtf = hrtf;
        ambisonicsDecodeParams.orientation = listenerCoords;

        iplAmbisonicsDecodeEffectApply(m_ambisonicsDecodeEffect, &ambisonicsDecodeParams, &m_ambisonicsBuffer, &m_outBuffer);
    }

    // -- mix input to output

    iplAudioBufferMix(context, &m_inBuffer, &m_outBuffer);

    // -- copy output

    auto numSamplesProduced = std::min(out_pBuffer->MaxFrames(), (AkUInt16) m_outBuffer.numSamples);
    for (AkUInt32 i = 0; i < std::min(out_pBuffer->NumChannels(), (AkUInt32) m_outBuffer.numChannels); ++i)
    {
        memcpy(out_pBuffer->GetChannel(i), m_outBuffer.data[i], numSamplesProduced * sizeof(IPLfloat32));
    }

    in_pBuffer->uValidFrames -= numSamplesConsumed;
    out_pBuffer->uValidFrames += numSamplesProduced;

    if (in_pBuffer->eState == AK_NoMoreData && in_pBuffer->uValidFrames == 0)
        out_pBuffer->eState = AK_NoMoreData;
    else if (out_pBuffer->uValidFrames == out_pBuffer->MaxFrames())
        out_pBuffer->eState = AK_DataReady;
    else
        out_pBuffer->eState = AK_DataNeeded;
}


AKRESULT SteamAudioMixReturnFX::TimeSkip(AkUInt32& io_uFrames)
{
    return AK_DataReady;
}


// --------------------------------------------------------------------------------------------------------------------
// Factory Functions
// --------------------------------------------------------------------------------------------------------------------

AK::IAkPlugin* CreateSteamAudioMixReturnFX(AK::IAkPluginMemAlloc* in_pAllocator)
{
    return AK_PLUGIN_NEW(in_pAllocator, SteamAudioMixReturnFX());
}


AK::IAkPluginParam* CreateSteamAudioMixReturnFXParams(AK::IAkPluginMemAlloc* in_pAllocator)
{
    return AK_PLUGIN_NEW(in_pAllocator, SteamAudioMixReturnFXParams());
}


AK_IMPLEMENT_PLUGIN_FACTORY(SteamAudioMixReturnFX, AkPluginTypeEffect, SteamAudioMixReturnConfig::CompanyID, SteamAudioMixReturnConfig::PluginID)
