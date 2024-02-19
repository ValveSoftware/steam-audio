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

namespace ReverbEffect {

/**
 *  DSP parameters for the "Steam Audio Reverb" effect.
 */
enum Params
{
    /**
     *  **Type**: `FMOD_DSP_PARAMETER_TYPE_BOOL`
     *
     *  If true, applies HRTF-based 3D audio rendering to reverb. Results in an improvement in spatialization quality
     *  when using convolution or hybrid reverb, at the cost of slightly increased CPU usage.
     */
    BINAURAL,

    /** The number of parameters in this effect. */
    NUM_PARAMS
};

FMOD_DSP_PARAMETER_DESC gParams[] = {
    { FMOD_DSP_PARAMETER_TYPE_BOOL, "Binaural", "", "Spatialize reflected sound using HRTF." }
};

FMOD_DSP_PARAMETER_DESC* gParamsArray[NUM_PARAMS];

void initParamDescs()
{
    for (auto i = 0; i < NUM_PARAMS; ++i)
    {
        gParamsArray[i] = &gParams[i];
    }

    gParams[BINAURAL].booldesc = {false};
}

struct State
{
    bool binaural;

    IPLAudioBuffer inBuffer;
    IPLAudioBuffer monoBuffer;
    IPLAudioBuffer reflectionsBuffer;
    IPLAudioBuffer outBuffer;

    IPLReflectionEffect reflectionEffect;
    IPLAmbisonicsDecodeEffect ambisonicsEffect;
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

        if (!effect->reflectionEffect)
        {
            IPLReflectionEffectSettings effectSettings;
            effectSettings.type = gSimulationSettings.reflectionType;
            effectSettings.irSize = numSamplesForDuration(gSimulationSettings.maxDuration, audioSettings.samplingRate);
            effectSettings.numChannels = numChannelsForOrder(gSimulationSettings.maxOrder);

            status = iplReflectionEffectCreate(gContext, &audioSettings, &effectSettings, &effect->reflectionEffect);
        }

        if (status == IPL_STATUS_SUCCESS)
            initFlags = static_cast<InitFlags>(initFlags | INIT_REFLECTIONEFFECT);
    }

    if (numChannelsOut > 0 && gIsSimulationSettingsValid)
    {
        status = IPL_STATUS_SUCCESS;

        if (!effect->ambisonicsEffect)
        {
            IPLAmbisonicsDecodeEffectSettings effectSettings;
            effectSettings.speakerLayout = speakerLayoutForNumChannels(numChannelsOut);
            effectSettings.hrtf = gHRTF[1];
            effectSettings.maxOrder = gSimulationSettings.maxOrder;

            status = iplAmbisonicsDecodeEffectCreate(gContext, &audioSettings, &effectSettings, &effect->ambisonicsEffect);
        }

        if (status == IPL_STATUS_SUCCESS)
            initFlags = static_cast<InitFlags>(initFlags | INIT_AMBISONICSEFFECT);
    }

    if (numChannelsIn > 0 && numChannelsOut > 0)
    {
        auto numAmbisonicChannels = numChannelsForOrder(gSimulationSettings.maxOrder);

        if (!effect->inBuffer.data)
            iplAudioBufferAllocate(gContext, numChannelsIn, audioSettings.frameSize, &effect->inBuffer);

        if (!effect->monoBuffer.data)
            iplAudioBufferAllocate(gContext, 1, audioSettings.frameSize, &effect->monoBuffer);

        if (!effect->reflectionsBuffer.data)
            iplAudioBufferAllocate(gContext, numAmbisonicChannels, audioSettings.frameSize, &effect->reflectionsBuffer);

        if (!effect->outBuffer.data)
            iplAudioBufferAllocate(gContext, numChannelsOut, audioSettings.frameSize, &effect->outBuffer);

        initFlags = static_cast<InitFlags>(initFlags | INIT_AUDIOBUFFERS);
    }

    return initFlags;
}

void reset(FMOD_DSP_STATE* state)
{
    auto effect = reinterpret_cast<State*>(state->plugindata);
    if (!effect)
        return;

    effect->binaural = false;
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

    iplAudioBufferFree(gContext, &effect->inBuffer);
    iplAudioBufferFree(gContext, &effect->monoBuffer);
    iplAudioBufferFree(gContext, &effect->reflectionsBuffer);
    iplAudioBufferFree(gContext, &effect->outBuffer);

    iplReflectionEffectRelease(&effect->reflectionEffect);
    iplAmbisonicsDecodeEffectRelease(&effect->ambisonicsEffect);

    gNewReverbSourceWritten = false;
    iplSourceRelease(&gReverbSource[0]);
    iplSourceRelease(&gReverbSource[1]);

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
    case BINAURAL:
        *value = effect->binaural;
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
    case BINAURAL:
        effect->binaural = value;
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
            return FMOD_OK;

        if (gNewHRTFWritten)
        {
            iplHRTFRelease(&gHRTF[0]);
            gHRTF[0] = iplHRTFRetain(gHRTF[1]);

            gNewHRTFWritten = false;
        }

        if (gNewReverbSourceWritten)
        {
            iplSourceRelease(&gReverbSource[0]);
            gReverbSource[0] = iplSourceRetain(gReverbSource[1]);

            gNewReverbSourceWritten = false;
        }

        if (!gReverbSource[0])
            return FMOD_OK;

        auto listenerCoordinates = calcListenerCoordinates(state);

        iplAudioBufferDeinterleave(gContext, in, &effect->inBuffer);
        iplAudioBufferDownmix(gContext, &effect->inBuffer, &effect->monoBuffer);

        IPLSimulationOutputs reverbOutputs{};
        iplSourceGetOutputs(gReverbSource[0], IPL_SIMULATIONFLAGS_REFLECTIONS, &reverbOutputs);

        IPLReflectionEffectParams reflectionParams = reverbOutputs.reflections;
        reflectionParams.type = gSimulationSettings.reflectionType;
        reflectionParams.numChannels = numChannelsForOrder(gSimulationSettings.maxOrder);
        reflectionParams.irSize = numSamplesForDuration(gSimulationSettings.maxDuration, samplingRate);
        reflectionParams.tanDevice = gSimulationSettings.tanDevice;

        if (gNewReflectionMixerWritten)
        {
            iplReflectionMixerRelease(&gReflectionMixer[0]);
            gReflectionMixer[0] = iplReflectionMixerRetain(gReflectionMixer[1]);

            gNewReflectionMixerWritten = false;
        }

        iplReflectionEffectApply(effect->reflectionEffect, &reflectionParams, &effect->monoBuffer, &effect->reflectionsBuffer, gReflectionMixer[0]);

        if (gSimulationSettings.reflectionType != IPL_REFLECTIONEFFECTTYPE_TAN && !gReflectionMixer[0])
        {
            IPLAmbisonicsDecodeEffectParams ambisonicsParams;
            ambisonicsParams.order = gSimulationSettings.maxOrder;
            ambisonicsParams.hrtf = gHRTF[0];
            ambisonicsParams.orientation = listenerCoordinates;
            ambisonicsParams.binaural = (effect->binaural) ? IPL_TRUE : IPL_FALSE;

            iplAmbisonicsDecodeEffectApply(effect->ambisonicsEffect, &ambisonicsParams, &effect->reflectionsBuffer, &effect->outBuffer);

            iplAudioBufferInterleave(gContext, &effect->outBuffer, out);
        }

        return FMOD_OK;
    }

    return FMOD_OK;
}

}

/** Descriptor for the Reverb effect. */
FMOD_DSP_DESCRIPTION gReverbEffect
{
    FMOD_PLUGIN_SDK_VERSION,
    "Steam Audio Reverb",
    STEAMAUDIO_FMOD_VERSION,
    1,
    1,
    ReverbEffect::create,
    ReverbEffect::release,
    nullptr,
    nullptr,
    ReverbEffect::process,
    nullptr,
    ReverbEffect::NUM_PARAMS,
    ReverbEffect::gParamsArray,
    nullptr,
    nullptr,
    ReverbEffect::setBool,
    nullptr,
    nullptr,
    nullptr,
    ReverbEffect::getBool,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr
};

}
