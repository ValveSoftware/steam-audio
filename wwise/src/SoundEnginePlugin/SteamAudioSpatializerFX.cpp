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

#include "SteamAudioSpatializerFX.h"
#include "../SteamAudioWwiseConfig.h"

#include <algorithm>
#include <AK/AkWwiseSDKVersion.h>

#include "SteamAudioCommon.h"

// --------------------------------------------------------------------------------------------------------------------
// SteamAudioSpatializerFX
// --------------------------------------------------------------------------------------------------------------------

SteamAudioSpatializerFX::SteamAudioSpatializerFX()
    : m_params(nullptr)
    , m_allocator(nullptr)
    , m_context(nullptr)
    , m_inputChannelCount(0)
    , m_directEffect(nullptr)
    , m_panningEffect(nullptr)
    , m_binauralEffect(nullptr)
    , m_reflectionEffect(nullptr)
    , m_pathingEffect(nullptr)
    , m_ambisonicsDecodeEffect(nullptr)
    , m_inBuffer()
    , m_outBuffer()
    , m_directBuffer()
    , m_monoBuffer()
    , m_ambisonicsBuffer()
    , m_ambisonicsOutBuffer()
    , m_prevGameObjectID(AK_INVALID_GAME_OBJECT)
    , m_prevDirectMixLevel(1.0f)
    , m_prevReflectionsMixLevel(0.0f)
    , m_prevPathingMixLevel(0.0f)
{}


SteamAudioSpatializerFX::~SteamAudioSpatializerFX() = default;


bool SteamAudioSpatializerFX::IsAudioFormatSupported(const AkAudioFormat& in_rFormat)
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


AKRESULT SteamAudioSpatializerFX::Init(AK::IAkPluginMemAlloc* in_pAllocator, AK::IAkEffectPluginContext* in_pEffectPluginContext, AK::IAkPluginParam* in_pParams, AkAudioFormat& io_rFormat)
{
    if (!IsAudioFormatSupported(io_rFormat))
        return AK_UnsupportedChannelConfig;

    m_params = (SteamAudioSpatializerFXParams*) in_pParams;
    m_allocator = in_pAllocator;
    m_context = in_pEffectPluginContext;

    m_inputChannelCount = io_rFormat.GetNumChannels();

    // This sets the output buffer to stero
    if (io_rFormat.channelConfig.uNumChannels == 1)
    {
        io_rFormat.channelConfig.SetStandard(AK_SPEAKER_SETUP_STEREO);
    }

    // Only support stereo output for now
    if (io_rFormat.channelConfig.uNumChannels != 2)
        return AK_Fail;

    m_format = io_rFormat;

    auto& globalState = SteamAudioWwise::GlobalState::Get();
    globalState.Retain();
    
    m_prevGameObjectID = AK_INVALID_GAME_OBJECT;

    LazyInit();

    // We want to consider this effect instance as initialized even if the Steam Audio initialization didn't succeed.
    // We'll keep trying to initialize in subsequent frames.
    return AK_Success;
}

AKRESULT SteamAudioSpatializerFX::LazyInit()
{
    if (m_directEffect 
        && m_panningEffect
        && m_binauralEffect
        && m_inBuffer.data
        && m_outBuffer.data
        && m_directBuffer.data
        && m_monoBuffer.data)
    {
        return AK_Success;
    }

	AkAudioSettings wwiseAudioSettings{};
	m_context->GlobalContext()->GetAudioSettings(wwiseAudioSettings);

    if (m_context->GetGameObjectInfo()->GetNumGameObjectPositions() > 1)
        return AK_NotInitialized;

    AkUInt32 nListeners;
    if (m_context->GetGameObjectInfo()->GetListeners(nullptr, nListeners) != AK_Success)
        return AK_NotInitialized;

    if (nListeners != 1)
        return AK_NotInitialized;

	IPLAudioSettings audioSettings{};
	audioSettings.samplingRate = wwiseAudioSettings.uNumSamplesPerSecond;
	audioSettings.frameSize = wwiseAudioSettings.uNumSamplesPerFrame;

    if (!SteamAudioWwise::EnsureSteamAudioContextExists(audioSettings, m_context->GlobalContext()))
        return AK_NotInitialized;

    auto& globalState = SteamAudioWwise::GlobalState::Get();

    auto context = globalState.context.Read();
    auto hrtf = globalState.hrtf.Read();

    if (!m_directEffect)
    {
        IPLDirectEffectSettings directEffectSettings{};
        directEffectSettings.numChannels = m_inputChannelCount;

        if (iplDirectEffectCreate(context, &audioSettings, &directEffectSettings, &m_directEffect) != IPL_STATUS_SUCCESS)
            return AK_NotInitialized;
    }
    
    if (!m_panningEffect)
    {
        IPLPanningEffectSettings panningSettings{};
        panningSettings.speakerLayout = SteamAudioWwise::SpeakerLayoutForNumChannels(m_format.GetNumChannels());

        if (iplPanningEffectCreate(context, &audioSettings, &panningSettings, &m_panningEffect) != IPL_STATUS_SUCCESS)
            return AK_NotInitialized;
    }

    if (!m_binauralEffect)
    {
		IPLBinauralEffectSettings binauralEffectSettings;
        binauralEffectSettings.hrtf = hrtf;

        if (iplBinauralEffectCreate(context, &audioSettings, &binauralEffectSettings, &m_binauralEffect) != IPL_STATUS_SUCCESS)
            return AK_NotInitialized;
    }

    if (!m_reflectionEffect && m_params->NonRTPC.reflections && globalState.simulationSettingsValid)
    {
        IPLReflectionEffectSettings reflectionEffectSettings{};
        reflectionEffectSettings.type = globalState.simulationSettings.reflectionType;
        reflectionEffectSettings.numChannels = SteamAudioWwise::NumChannelsForOrder(globalState.simulationSettings.maxOrder);
        reflectionEffectSettings.irSize = SteamAudioWwise::NumSamplesForDuration(globalState.simulationSettings.maxDuration, audioSettings.samplingRate);

        if (iplReflectionEffectCreate(context, &audioSettings, &reflectionEffectSettings, &m_reflectionEffect) != IPL_STATUS_SUCCESS)
            return AK_NotInitialized;
    }

    if (!m_pathingEffect && m_params->NonRTPC.pathing && globalState.simulationSettingsValid)
    {
        IPLPathEffectSettings pathingEffectSettings{};
        pathingEffectSettings.maxOrder = globalState.simulationSettings.maxOrder;
        pathingEffectSettings.spatialize = IPL_TRUE;
        pathingEffectSettings.speakerLayout = SteamAudioWwise::SpeakerLayoutForNumChannels(2);
        pathingEffectSettings.hrtf = hrtf;

        if (iplPathEffectCreate(context, &audioSettings, &pathingEffectSettings, &m_pathingEffect) != IPL_STATUS_SUCCESS)
            return AK_NotInitialized;
    }

    if (!m_ambisonicsDecodeEffect && (m_params->NonRTPC.reflections || m_params->NonRTPC.pathing) && globalState.simulationSettingsValid)
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
        if (iplAudioBufferAllocate(context, m_inputChannelCount, audioSettings.frameSize, &m_inBuffer) != IPL_STATUS_SUCCESS)
            return AK_NotInitialized;
    }

    if (!m_outBuffer.data)
    {
        if (iplAudioBufferAllocate(context, m_format.GetNumChannels(), audioSettings.frameSize, &m_outBuffer) != IPL_STATUS_SUCCESS)
            return AK_NotInitialized;
    }

	if (!m_directBuffer.data)
	{
		if (iplAudioBufferAllocate(context, m_inputChannelCount, audioSettings.frameSize, &m_directBuffer) != IPL_STATUS_SUCCESS)
			return AK_NotInitialized;
	}

	if (!m_monoBuffer.data)
	{
		if (iplAudioBufferAllocate(context, 1, audioSettings.frameSize, &m_monoBuffer) != IPL_STATUS_SUCCESS)
			return AK_NotInitialized;
	}

    if (!m_ambisonicsBuffer.data && (m_params->NonRTPC.reflections || m_params->NonRTPC.pathing) && globalState.simulationSettingsValid)
    {
        if (iplAudioBufferAllocate(context, SteamAudioWwise::NumChannelsForOrder(globalState.simulationSettings.maxOrder), audioSettings.frameSize, &m_ambisonicsBuffer) != IPL_STATUS_SUCCESS)
            return AK_NotInitialized;
    }

    if (!m_ambisonicsOutBuffer.data && (m_params->NonRTPC.reflections || m_params->NonRTPC.pathing) && globalState.simulationSettingsValid)
    {
        if (iplAudioBufferAllocate(context, m_format.GetNumChannels(), audioSettings.frameSize, &m_ambisonicsOutBuffer) != IPL_STATUS_SUCCESS)
            return AK_NotInitialized;
    }

    return AK_Success;
}


AKRESULT SteamAudioSpatializerFX::Term(AK::IAkPluginMemAlloc* in_pAllocator)
{
    auto& globalState = SteamAudioWwise::GlobalState::Get();

    auto context = globalState.context.Read();

    iplAudioBufferFree(context, &m_outBuffer);
    iplAudioBufferFree(context, &m_inBuffer);
    iplAudioBufferFree(context, &m_directBuffer);
    iplAudioBufferFree(context, &m_monoBuffer);
    iplAudioBufferFree(context, &m_ambisonicsBuffer);
    iplAudioBufferFree(context, &m_ambisonicsOutBuffer);

    iplDirectEffectRelease(&m_directEffect);
    iplPanningEffectRelease(&m_panningEffect);
    iplBinauralEffectRelease(&m_binauralEffect);
    iplReflectionEffectRelease(&m_reflectionEffect);
    iplPathEffectRelease(&m_pathingEffect);
    iplAmbisonicsDecodeEffectRelease(&m_ambisonicsDecodeEffect);

    globalState.Release();
    
    AK_PLUGIN_DELETE(in_pAllocator, this);

    return AK_Success;
}


AKRESULT SteamAudioSpatializerFX::Reset()
{
    iplDirectEffectReset(m_directEffect);
    iplPanningEffectReset(m_panningEffect);
    iplBinauralEffectReset(m_binauralEffect);
    iplReflectionEffectReset(m_reflectionEffect);
    iplPathEffectReset(m_pathingEffect);
    iplAmbisonicsDecodeEffectReset(m_ambisonicsDecodeEffect);

    return AK_Success;
}


AKRESULT SteamAudioSpatializerFX::GetPluginInfo(AkPluginInfo& out_rPluginInfo)
{
    out_rPluginInfo.eType = AkPluginTypeEffect;
    out_rPluginInfo.bIsInPlace = false;
    out_rPluginInfo.bCanProcessObjects = false;
    out_rPluginInfo.uBuildVersion = AK_WWISESDK_VERSION_COMBINED;

    return AK_Success;
}

void SteamAudioSpatializerFX::Execute(AkAudioBuffer* in_pBuffer, AkUInt32 in_uInOffset, AkAudioBuffer* out_pBuffer)
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

    // -- update source for simulation reasons

    AkGameObjectID gameObjectID = m_context->GetGameObjectInfo()->GetGameObjectID();
    if (gameObjectID != m_prevGameObjectID)
    {
        m_source = globalState.sourceMap.Get(gameObjectID);
        m_prevGameObjectID = gameObjectID;
    }

    auto source = m_source ? m_source->Read() : nullptr;

    IPLSimulationOutputs sourceOutputs{};
    iplSourceGetOutputs(source, (IPLSimulationFlags) (IPL_SIMULATIONFLAGS_DIRECT | IPL_SIMULATIONFLAGS_REFLECTIONS | IPL_SIMULATIONFLAGS_PATHING), &sourceOutputs);

    // -- clear input

    for (IPLint32 i = 0; i < m_inBuffer.numChannels; ++i)
    {
        memset(m_inBuffer.data[i], 0, m_inBuffer.numSamples * sizeof(IPLfloat32));
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
    IPLCoordinateSpace3 sourceCoords{};

    if (!bUseGamePosition)
    {
        listenerCoords.origin = { 0, 0, 0 };
        listenerCoords.right = { 1, 0, 0 };
        listenerCoords.up = { 0, 1, 0 };
        listenerCoords.ahead = { 0, 0, -1 };

        sourceCoords.origin = { m_params->RTPC.pos[0], m_params->RTPC.pos[1], m_params->RTPC.pos[2] };
        sourceCoords.right = { 1, 0, 0 };
        sourceCoords.up = { 0, 1, 0 };
        sourceCoords.ahead = { 0, 0, -1 };
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

        AkSoundPosition sourceTransform;
        {
            int nPositions = m_context->GetGameObjectInfo()->GetNumGameObjectPositions();
            if (nPositions > 1)
            {
                out_pBuffer->eState = AK_Fail;
                return;
            }

            if (m_context->GetGameObjectInfo()->GetGameObjectPosition(0, sourceTransform) != AK_Success)
            {
                out_pBuffer->eState = AK_Fail;
                return;
            }
        }

        listenerCoords = SteamAudioWwise::CalculateCoordinates(listenerTransform);
        sourceCoords = SteamAudioWwise::CalculateCoordinates(sourceTransform);
    }

    IPLVector3 direction = iplCalculateRelativeDirection(context, sourceCoords.origin, listenerCoords.origin, listenerCoords.ahead, listenerCoords.up);
    if (SteamAudioWwise::Dot(direction, direction) < 1e-6f)
        direction = IPLVector3{ 0.0f, 1.0f, 0.0f };


    // -- apply direct effect

    IPLDirectEffectParams directEffectParams{};
    if (m_params->NonRTPC.distanceAttenuation)
    {
        directEffectParams.flags = (IPLDirectEffectFlags) (directEffectParams.flags | IPL_DIRECTEFFECTFLAGS_APPLYDISTANCEATTENUATION);

        IPLDistanceAttenuationModel distanceAttenuationModel{};
        directEffectParams.distanceAttenuation = iplDistanceAttenuationCalculate(context, sourceCoords.origin, listenerCoords.origin, &distanceAttenuationModel);
    }
    if (m_params->NonRTPC.airAbsorption)
    {
        directEffectParams.flags = (IPLDirectEffectFlags) (directEffectParams.flags | IPL_DIRECTEFFECTFLAGS_APPLYAIRABSORPTION);

        IPLAirAbsorptionModel airAbsorptionModel{};
        iplAirAbsorptionCalculate(context, sourceCoords.origin, listenerCoords.origin, &airAbsorptionModel, directEffectParams.airAbsorption);
    }
    if (m_params->NonRTPC.directivity)
    {
        directEffectParams.flags = (IPLDirectEffectFlags) (directEffectParams.flags | IPL_DIRECTEFFECTFLAGS_APPLYDIRECTIVITY);

        IPLDirectivity directivity{};
        directivity.dipoleWeight = m_params->RTPC.dipoleWeight;
        directivity.dipolePower = m_params->RTPC.dipolePower;
        directEffectParams.directivity = iplDirectivityCalculate(context, sourceCoords, listenerCoords.origin, &directivity);
    }
    if (m_params->NonRTPC.occlusion != STEAMAUDIO_PARAM_OFF)
    {
        directEffectParams.flags = (IPLDirectEffectFlags) (directEffectParams.flags | IPL_DIRECTEFFECTFLAGS_APPLYOCCLUSION);

        if (m_params->NonRTPC.occlusion == STEAMAUDIO_PARAM_USERDEFINED)
        {
            directEffectParams.occlusion = m_params->RTPC.occlusionValue;
        }
        else
        {
            directEffectParams.occlusion = sourceOutputs.direct.occlusion;
        }

        if (m_params->NonRTPC.transmission != STEAMAUDIO_PARAM_OFF)
        {
            directEffectParams.flags = (IPLDirectEffectFlags) (directEffectParams.flags | IPL_DIRECTEFFECTFLAGS_APPLYTRANSMISSION);
            directEffectParams.transmissionType = m_params->NonRTPC.transmissionType;

            if (m_params->NonRTPC.transmission == STEAMAUDIO_PARAM_USERDEFINED)
            {
                directEffectParams.transmission[0] = m_params->RTPC.transmissionValue[0];
                directEffectParams.transmission[1] = m_params->RTPC.transmissionValue[1];
                directEffectParams.transmission[2] = m_params->RTPC.transmissionValue[2];
            }
            else
            {
                directEffectParams.transmission[0] = sourceOutputs.direct.transmission[0];
                directEffectParams.transmission[1] = sourceOutputs.direct.transmission[1];
                directEffectParams.transmission[2] = sourceOutputs.direct.transmission[2];
            }
        }
    }

    iplDirectEffectApply(m_directEffect, &directEffectParams, &m_inBuffer, &m_directBuffer);

    // -- apply binaural / panning

	if (m_params->NonRTPC.directBinaural)
    {
        IPLBinauralEffectParams binauralParams{};
        binauralParams.direction = direction;
        binauralParams.interpolation = static_cast<IPLHRTFInterpolation>(m_params->NonRTPC.hrtfInterpolation);
        binauralParams.spatialBlend = 1.0f;
        binauralParams.hrtf = hrtf;

        if (binauralParams.hrtf)
        {
            iplBinauralEffectApply(m_binauralEffect, &binauralParams, &m_directBuffer, &m_outBuffer);
        }
    }
	else
    {
        iplAudioBufferDownmix(context, &m_directBuffer, &m_monoBuffer);

        IPLPanningEffectParams panningParams{};
        panningParams.direction = direction;

        iplPanningEffectApply(m_panningEffect, &panningParams, &m_monoBuffer, &m_outBuffer);
    }

    // -- apply direct mix level

    SteamAudioWwise::ApplyVolumeRamp(m_params->RTPC.directMixLevel, m_prevDirectMixLevel, m_outBuffer);

    // -- apply reflections

    if (m_params->NonRTPC.reflections && globalState.simulationSettingsValid)
    {
        iplAudioBufferDownmix(context, &m_inBuffer, &m_monoBuffer);

        SteamAudioWwise::ApplyVolumeRamp(m_params->RTPC.reflectionsMixLevel, m_prevReflectionsMixLevel, m_monoBuffer);

        IPLReflectionEffectParams reflectionParams = sourceOutputs.reflections;
        reflectionParams.type = globalState.simulationSettings.reflectionType;
        reflectionParams.numChannels = SteamAudioWwise::NumChannelsForOrder(globalState.simulationSettings.maxOrder);
        reflectionParams.irSize = SteamAudioWwise::NumSamplesForDuration(globalState.simulationSettings.maxDuration, m_format.uSampleRate);
        reflectionParams.tanDevice = globalState.simulationSettings.tanDevice;

        auto reflectionMixer = globalState.reflectionMixer.Read();

        iplReflectionEffectApply(m_reflectionEffect, &reflectionParams, &m_monoBuffer, &m_ambisonicsBuffer, reflectionMixer);

        if (reflectionParams.type != IPL_REFLECTIONEFFECTTYPE_TAN && !reflectionMixer)
        {
            IPLAmbisonicsDecodeEffectParams ambisonicsDecodeParams{};
            ambisonicsDecodeParams.order = globalState.simulationSettings.maxOrder;
            ambisonicsDecodeParams.binaural = m_params->NonRTPC.reflectionsBinaural ? IPL_TRUE : IPL_FALSE;
            ambisonicsDecodeParams.hrtf = hrtf;
            ambisonicsDecodeParams.orientation = listenerCoords;

            iplAmbisonicsDecodeEffectApply(m_ambisonicsDecodeEffect, &ambisonicsDecodeParams, &m_ambisonicsBuffer, &m_ambisonicsOutBuffer);

            iplAudioBufferMix(context, &m_ambisonicsOutBuffer, &m_outBuffer);
        }
    }

    // -- apply pathing

    if (m_params->NonRTPC.pathing && globalState.simulationSettingsValid)
    {
        iplAudioBufferDownmix(context, &m_inBuffer, &m_monoBuffer);

        SteamAudioWwise::ApplyVolumeRamp(m_params->RTPC.pathingMixLevel, m_prevPathingMixLevel, m_monoBuffer);

        IPLPathEffectParams pathingParams = sourceOutputs.pathing;
        pathingParams.order = globalState.simulationSettings.maxOrder;
        pathingParams.binaural = m_params->NonRTPC.pathingBinaural ? IPL_TRUE : IPL_FALSE;
        pathingParams.hrtf = hrtf;
        pathingParams.listener = listenerCoords;

        iplPathEffectApply(m_pathingEffect, &pathingParams, &m_monoBuffer, &m_ambisonicsOutBuffer);

        iplAudioBufferMix(context, &m_ambisonicsOutBuffer, &m_outBuffer);
    }

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


AKRESULT SteamAudioSpatializerFX::TimeSkip(AkUInt32& io_uFrames)
{
    return AK_DataReady;
}


// --------------------------------------------------------------------------------------------------------------------
// Factory Functions
// --------------------------------------------------------------------------------------------------------------------

AK::IAkPlugin* CreateSteamAudioSpatializerFX(AK::IAkPluginMemAlloc* in_pAllocator)
{
    return AK_PLUGIN_NEW(in_pAllocator, SteamAudioSpatializerFX());
}


AK::IAkPluginParam* CreateSteamAudioSpatializerFXParams(AK::IAkPluginMemAlloc* in_pAllocator)
{
    return AK_PLUGIN_NEW(in_pAllocator, SteamAudioSpatializerFXParams());
}


AK_IMPLEMENT_PLUGIN_FACTORY(SteamAudioSpatializerFX, AkPluginTypeEffect, SteamAudioSpatializerConfig::CompanyID, SteamAudioSpatializerConfig::PluginID)
