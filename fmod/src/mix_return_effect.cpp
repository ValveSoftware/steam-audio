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

#include "steamaudio_fmod.h"

namespace SteamAudioFMOD {

namespace MixerReturnEffect {



FMOD_DSP_PARAMETER_DESC gParams[] = {
    { FMOD_DSP_PARAMETER_TYPE_BOOL, "Binaural", "", "Spatialize reflected sound using HRTF." },
    { FMOD_DSP_PARAMETER_TYPE_INT, "OutputFormat", "", "Output Format" },
};

FMOD_DSP_PARAMETER_DESC* gParamsArray[IPL_MIXRETURN_NUM_PARAMS];

const char* gOutputFormatValues[] = { "From Mixer", "From Final Out", "From Input" };

void initParamDescs()
{
    for (auto i = 0; i < IPL_MIXRETURN_NUM_PARAMS; ++i)
    {
        gParamsArray[i] = &gParams[i];
    }

    gParams[IPL_MIXRETURN_BINAURAL].booldesc = {false};
    gParams[IPL_MIXRETURN_OUTPUT_FORMAT].intdesc = { 0, 2, 0, false, gOutputFormatValues };
}

struct State
{
    bool binaural;
    ParameterSpeakerFormatType outputFormat;

    IPLAudioBuffer reflectionsBuffer;
    IPLAudioBuffer inBuffer;
    IPLAudioBuffer outBuffer;

    IPLReflectionMixer reflectionMixer;
    IPLReflectionEffectSettings reflectionMixerSettingsBackup;
    IPLAmbisonicsDecodeEffect ambisonicsEffect;
    IPLAmbisonicsDecodeEffectSettings ambisonicsEffectSettingsBackup;
};

enum InitFlags
{
    INIT_NONE = 0,
    INIT_AUDIOBUFFERS = 1 << 0,
    INIT_REFLECTIONEFFECT = 1 << 1,
    INIT_AMBISONICSEFFECT = 1 << 2
};

InitFlags lazyInit(FMOD_DSP_STATE* state,
                   int numChannelsIn,
                   int numChannelsOut)
{
    auto initFlags = INIT_NONE;

    IPLAudioSettings audioSettings;
    state->functions->getsamplerate(state, &audioSettings.samplingRate);
    state->functions->getblocksize(state, reinterpret_cast<unsigned int*>(&audioSettings.frameSize));

    if (!gContext && isRunningInEditor())
    {
        initContextAndDefaultHRTF(audioSettings);
    }

    if (!gContext)
        return initFlags;

    if (!gHRTF[1])
        return initFlags;

    auto effect = reinterpret_cast<State*>(state->plugindata);

    auto status = IPL_STATUS_SUCCESS;

    if (gIsSimulationSettingsValid)
    {
        status = IPL_STATUS_SUCCESS;

        if (effect->reflectionMixer && effect->reflectionMixerSettingsBackup.numChannels != numChannelsForOrder(gSimulationSettings.maxOrder))
        {
            iplReflectionMixerReset(effect->reflectionMixer);
            iplReflectionMixerRelease(&effect->reflectionMixer);
        }

        if (!effect->reflectionMixer)
        {
            IPLReflectionEffectSettings effectSettings;
            effectSettings.type = gSimulationSettings.reflectionType;
            effectSettings.numChannels = numChannelsForOrder(gSimulationSettings.maxOrder);

            status = iplReflectionMixerCreate(gContext, &audioSettings, &effectSettings, &effect->reflectionMixer);

            effect->reflectionMixerSettingsBackup = effectSettings;

            if (!gNewReflectionMixerWritten)
            {
                iplReflectionMixerRelease(&gReflectionMixer[1]);
                gReflectionMixer[1] = iplReflectionMixerRetain(effect->reflectionMixer);

                gNewReflectionMixerWritten = true;
            }
        }

        if (status == IPL_STATUS_SUCCESS)
            initFlags = static_cast<InitFlags>(initFlags | INIT_REFLECTIONEFFECT);
    }

    if (numChannelsOut > 0 && gIsSimulationSettingsValid)
    {
        status = IPL_STATUS_SUCCESS;

        if (effect->ambisonicsEffect && effect->ambisonicsEffectSettingsBackup.speakerLayout.type != speakerLayoutForNumChannels(numChannelsOut).type)
        {
            iplAmbisonicsDecodeEffectReset(effect->ambisonicsEffect);
            iplAmbisonicsDecodeEffectRelease(&effect->ambisonicsEffect);
        }

        if (!effect->ambisonicsEffect)
        {
            IPLAmbisonicsDecodeEffectSettings effectSettings;
            effectSettings.speakerLayout = speakerLayoutForNumChannels(numChannelsOut);
            effectSettings.hrtf = gHRTF[1];
            effectSettings.maxOrder = gSimulationSettings.maxOrder;

            status = iplAmbisonicsDecodeEffectCreate(gContext, &audioSettings, &effectSettings, &effect->ambisonicsEffect);

            effect->ambisonicsEffectSettingsBackup = effectSettings;
        }

        if (status == IPL_STATUS_SUCCESS)
            initFlags = static_cast<InitFlags>(initFlags | INIT_AMBISONICSEFFECT);
    }

    if (numChannelsIn > 0 && numChannelsOut > 0)
    {
        int success = IPL_STATUS_SUCCESS;

        auto numAmbisonicChannels = numChannelsForOrder(gSimulationSettings.maxOrder);

        if (effect->reflectionsBuffer.data && effect->reflectionsBuffer.numChannels != numAmbisonicChannels)
            iplAudioBufferFree(gContext, &effect->reflectionsBuffer);

        if (!effect->reflectionsBuffer.data)
            success |= iplAudioBufferAllocate(gContext, numAmbisonicChannels, audioSettings.frameSize, &effect->reflectionsBuffer);

        if (effect->inBuffer.data && effect->inBuffer.numChannels != numChannelsIn)
            iplAudioBufferFree(gContext, &effect->inBuffer);

        if (!effect->inBuffer.data)
            success |= iplAudioBufferAllocate(gContext, numChannelsIn, audioSettings.frameSize, &effect->inBuffer);

        if (effect->outBuffer.data && effect->outBuffer.numChannels != numChannelsOut)
            iplAudioBufferFree(gContext, &effect->outBuffer);

        if (!effect->outBuffer.data)
            success |= iplAudioBufferAllocate(gContext, numChannelsOut, audioSettings.frameSize, &effect->outBuffer);

        initFlags = success == IPL_STATUS_SUCCESS ? static_cast<InitFlags>(initFlags | INIT_AUDIOBUFFERS) : initFlags;
    }

    return initFlags;
}

void reset(FMOD_DSP_STATE* state)
{
    auto effect = reinterpret_cast<State*>(state->plugindata);
    if (!effect)
        return;

    effect->binaural = false;
    effect->outputFormat = ParameterSpeakerFormatType::PARAMETER_FROM_MIXER;
}

FMOD_RESULT F_CALL create(FMOD_DSP_STATE* state)
{
    state->plugindata = new State();
    reset(state);
    lazyInit(state, 0, 0);
    return FMOD_OK;
}

FMOD_RESULT F_CALL release(FMOD_DSP_STATE* state)
{
    auto effect = reinterpret_cast<State*>(state->plugindata);

    iplAudioBufferFree(gContext, &effect->reflectionsBuffer);
    iplAudioBufferFree(gContext, &effect->inBuffer);
    iplAudioBufferFree(gContext, &effect->outBuffer);

    iplReflectionMixerRelease(&effect->reflectionMixer);
    iplAmbisonicsDecodeEffectRelease(&effect->ambisonicsEffect);

    delete state->plugindata;

    return FMOD_OK;
}

FMOD_RESULT F_CALL getBool(FMOD_DSP_STATE* state,
                           int index,
                           FMOD_BOOL* value,
                           char*)
{
    auto effect = reinterpret_cast<State*>(state->plugindata);

    switch (index)
    {
    case IPL_MIXRETURN_BINAURAL:
        *value = effect->binaural;
        break;
    default:
        return FMOD_ERR_INVALID_PARAM;
    }

    return FMOD_OK;
}

FMOD_RESULT F_CALL getInt(FMOD_DSP_STATE* state,
    int index,
    int* value,
    char*)
{
    auto effect = reinterpret_cast<State*>(state->plugindata);

    switch (index)
    {
    case IPL_MIXRETURN_OUTPUT_FORMAT:
        *value = static_cast<int>(effect->outputFormat);
        break;
    default:
        return FMOD_ERR_INVALID_PARAM;
    }

    return FMOD_OK;
}

FMOD_RESULT F_CALL setBool(FMOD_DSP_STATE* state,
                           int index,
                           FMOD_BOOL value)
{
    auto effect = reinterpret_cast<State*>(state->plugindata);

    switch (index)
    {
    case IPL_MIXRETURN_BINAURAL:
        effect->binaural = value;
        break;
    default:
        return FMOD_ERR_INVALID_PARAM;
    }

    return FMOD_OK;
}

FMOD_RESULT F_CALL setInt(FMOD_DSP_STATE* state,
    int index,
    int value)
{
    auto effect = reinterpret_cast<State*>(state->plugindata);

    switch (index)
    {
    case IPL_MIXRETURN_OUTPUT_FORMAT:
        effect->outputFormat = static_cast<ParameterSpeakerFormatType>(value);
        break;
    default:
        return FMOD_ERR_INVALID_PARAM;
    }

    return FMOD_OK;
}

FMOD_RESULT F_CALL process(FMOD_DSP_STATE* state,
                           unsigned int length,
                           const FMOD_DSP_BUFFER_ARRAY* inBuffers,
                           FMOD_DSP_BUFFER_ARRAY* outBuffers,
                           FMOD_BOOL inputsIdle,
                           FMOD_DSP_PROCESS_OPERATION operation)
{
    if (operation == FMOD_DSP_PROCESS_QUERY)
    {
        auto effect = reinterpret_cast<State*>(state->plugindata);
        if (!initFmodOutBufferFormat(inBuffers, outBuffers, state, effect->outputFormat))
            return FMOD_ERR_DSP_DONTPROCESS;

        if (inputsIdle)
            return FMOD_ERR_DSP_DONTPROCESS;
    }
    else if (operation == FMOD_DSP_PROCESS_PERFORM)
    {
        auto effect = reinterpret_cast<State*>(state->plugindata);

        auto samplingRate = 0;
        auto frameSize = 0u;
        state->functions->getsamplerate(state, &samplingRate);
        state->functions->getblocksize(state, &frameSize);

        auto numChannelsIn = inBuffers->buffernumchannels[0];
        auto numChannelsOut = outBuffers->buffernumchannels[0];
        auto in = inBuffers->buffers[0];
        auto out = outBuffers->buffers[0];

        // Start by clearing the output buffer.
        memset(out, 0, numChannelsOut * frameSize * sizeof(float));

        // Make sure that audio processing state has been initialized. If initialization fails, stop and emit silence.
        auto initFlags = lazyInit(state, numChannelsIn, numChannelsOut);
        if (!(initFlags & INIT_AUDIOBUFFERS) || !(initFlags & INIT_REFLECTIONEFFECT) || !(initFlags & INIT_AMBISONICSEFFECT))
            return FMOD_ERR_DSP_SILENCE;

        if (gNewHRTFWritten)
        {
            iplHRTFRelease(&gHRTF[0]);
            gHRTF[0] = iplHRTFRetain(gHRTF[1]);

            gNewHRTFWritten = false;
        }

        auto listenerCoordinates = calcListenerCoordinates(state);

        IPLReflectionEffectParams reflectionParams;
        reflectionParams.numChannels = numChannelsForOrder(gSimulationSettings.maxOrder);
        reflectionParams.tanDevice = gSimulationSettings.tanDevice;

        iplReflectionMixerApply(effect->reflectionMixer, &reflectionParams, &effect->reflectionsBuffer);

        IPLAmbisonicsDecodeEffectParams ambisonicsParams;
        ambisonicsParams.order = gSimulationSettings.maxOrder;
        ambisonicsParams.hrtf = gHRTF[0];
        ambisonicsParams.orientation = listenerCoordinates;
        ambisonicsParams.binaural = numChannelsOut == 2 && !gHRTFDisabled && (effect->binaural) ? IPL_TRUE : IPL_FALSE;

        iplAmbisonicsDecodeEffectApply(effect->ambisonicsEffect, &ambisonicsParams, &effect->reflectionsBuffer, &effect->outBuffer);

        iplAudioBufferDeinterleave(gContext, in, &effect->inBuffer);
        iplAudioBufferMix(gContext, &effect->inBuffer, &effect->outBuffer);

        iplAudioBufferInterleave(gContext, &effect->outBuffer, out);

        return FMOD_OK;
    }

    return FMOD_OK;
}

}

/** Descriptor for the Mixer Return effect. */
FMOD_DSP_DESCRIPTION gMixerReturnEffect
{
    FMOD_PLUGIN_SDK_VERSION,
    "Steam Audio Mixer Return",
    STEAMAUDIO_FMOD_VERSION,
    1,
    1,
    MixerReturnEffect::create,
    MixerReturnEffect::release,
    nullptr,
    nullptr,
    MixerReturnEffect::process,
    nullptr,
    IPL_MIXRETURN_NUM_PARAMS,
    MixerReturnEffect::gParamsArray,
    nullptr,
    MixerReturnEffect::setInt,
    MixerReturnEffect::setBool,
    nullptr,
    nullptr,
    MixerReturnEffect::getInt,
    MixerReturnEffect::getBool,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr
};

}
