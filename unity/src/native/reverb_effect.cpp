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

#include "steamaudio_unity_native.h"

namespace SteamAudioUnity {

namespace ReverbEffect {

enum Params
{
    BINAURAL,
    NUM_PARAMS
};

UnityAudioParameterDefinition gParamDefinitions[] =
{
    { "Binaural", "", "Apply HRTF.", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f },
};

#if !defined(IPL_OS_UNSUPPORTED)

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

void reset(UnityAudioEffectState* state)
{
    assert(state);

    auto effect = state->GetEffectData<State>();
    if (!effect)
        return;

    effect->binaural = false;
}

InitFlags lazyInit(UnityAudioEffectState* state,
                   int numChannelsIn,
                   int numChannelsOut)
{
    assert(state);

    auto initFlags = INIT_NONE;

    if (!gContext)
        return initFlags;

    if (!gHRTF[1])
        return initFlags;

    if (!state->effectdata)
    {
        state->effectdata = new State();
        reset(state);
    }

    auto effect = state->GetEffectData<State>();
    if (!effect)
        return initFlags;

    IPLAudioSettings audioSettings;
    audioSettings.samplingRate = state->samplerate;
    audioSettings.frameSize = state->dspbuffersize;

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

UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK create(UnityAudioEffectState* state)
{
    assert(state);

    state->effectdata = new State();
    reset(state);
    lazyInit(state, 0, 0);
    return UNITY_AUDIODSP_OK;
}

UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK release(UnityAudioEffectState* state)
{
    assert(state);

    if (!state->effectdata)
        return UNITY_AUDIODSP_OK;

    auto effect = state->GetEffectData<State>();
    if (!effect)
        return UNITY_AUDIODSP_OK;

    iplAudioBufferFree(gContext, &effect->inBuffer);
    iplAudioBufferFree(gContext, &effect->monoBuffer);
    iplAudioBufferFree(gContext, &effect->reflectionsBuffer);
    iplAudioBufferFree(gContext, &effect->outBuffer);

    iplReflectionEffectRelease(&effect->reflectionEffect);
    iplAmbisonicsDecodeEffectRelease(&effect->ambisonicsEffect);

    gNewReverbSourceWritten = false;
    iplSourceRelease(&gReverbSource[0]);
    iplSourceRelease(&gReverbSource[1]);

    delete state->effectdata;
    state->effectdata = nullptr;

    return UNITY_AUDIODSP_OK;
}

UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK getParam(UnityAudioEffectState* state, int index, float* value, char* valueStr)
{
    assert(state);

    auto effect = state->GetEffectData<State>();
    if (!effect)
        return UNITY_AUDIODSP_OK;

    switch (index)
    {
    case BINAURAL:
        *value = (effect->binaural) ? 1.0f : 0.0f;
        break;
    }

    return UNITY_AUDIODSP_OK;
}

UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK setParam(UnityAudioEffectState* state, int index, float value)
{
    assert(state);

    auto effect = state->GetEffectData<State>();
    if (!effect)
        return UNITY_AUDIODSP_OK;

    switch (index)
    {
    case BINAURAL:
        effect->binaural = (value == 1.0f);
        break;
    }

    return UNITY_AUDIODSP_OK;
}

void getLatestSource()
{
    if (gNewReverbSourceWritten)
    {
        iplSourceRelease(&gReverbSource[0]);
        gReverbSource[0] = iplSourceRetain(gReverbSource[1]);

        gNewReverbSourceWritten = false;
    }
}

UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK process(UnityAudioEffectState* state, float* in, float* out, unsigned int numSamples, int numChannelsIn, int numChannelsOut)
{
    assert(state);
    assert(in);
    assert(out);

    // Assume that the number of input and output channels are the same.
    assert(numChannelsIn == numChannelsOut);

    // Start by clearing the output buffer.
    memset(out, 0, numChannelsOut * numSamples * sizeof(float));

    // Unity can call the process callback even when not in play mode. In this case, emit silence.
    if (!(state->flags & UnityAudioEffectStateFlags_IsPlaying))
    {
        release(state);
        return UNITY_AUDIODSP_OK;
    }

    // Make sure that audio processing state has been initialized. If initialization fails, stop and emit silence.
    auto initFlags = lazyInit(state, numChannelsIn, numChannelsOut);
    if (!(initFlags & INIT_AUDIOBUFFERS) || !(initFlags & INIT_REFLECTIONEFFECT) || !(initFlags & INIT_AMBISONICSEFFECT))
        return UNITY_AUDIODSP_OK;

    getLatestHRTF();
    getLatestSource();

    if (!gReverbSource[0])
        return UNITY_AUDIODSP_OK;

    auto effect = state->GetEffectData<State>();
    if (!effect)
        return UNITY_AUDIODSP_OK;

    // TODO: Need to deprecate Unity versions that don't support spatializerdata on mixer effects!
    if (!state->spatializerdata)
        return UNITY_AUDIODSP_OK;

    // World-to-local transform matrix for the listener.
    auto L = state->spatializerdata->listenermatrix;

    auto listenerCoordinates = calcListenerCoordinates(L);

    iplAudioBufferDeinterleave(gContext, in, &effect->inBuffer);
    iplAudioBufferDownmix(gContext, &effect->inBuffer, &effect->monoBuffer);

    IPLSimulationOutputs reverbOutputs{};
    iplSourceGetOutputs(gReverbSource[0], IPL_SIMULATIONFLAGS_REFLECTIONS, &reverbOutputs);

    IPLReflectionEffectParams reflectionParams;
    reflectionParams.type = gSimulationSettings.reflectionType;
    reflectionParams.ir = reverbOutputs.reflections.ir;
    reflectionParams.reverbTimes[0] = reverbOutputs.reflections.reverbTimes[0];
    reflectionParams.reverbTimes[1] = reverbOutputs.reflections.reverbTimes[1];
    reflectionParams.reverbTimes[2] = reverbOutputs.reflections.reverbTimes[2];
    reflectionParams.eq[0] = reverbOutputs.reflections.eq[0];
    reflectionParams.eq[1] = reverbOutputs.reflections.eq[1];
    reflectionParams.eq[2] = reverbOutputs.reflections.eq[2];
    reflectionParams.delay = reverbOutputs.reflections.delay;
    reflectionParams.numChannels = numChannelsForOrder(gSimulationSettings.maxOrder);
    reflectionParams.irSize = numSamplesForDuration(gSimulationSettings.maxDuration, static_cast<int>(state->samplerate));
    reflectionParams.tanDevice = gSimulationSettings.tanDevice;
    reflectionParams.tanSlot = reverbOutputs.reflections.tanSlot;

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
        ambisonicsParams.binaural = numChannelsOut == 2 && !gHRTFDisabled && (effect->binaural) ? IPL_TRUE : IPL_FALSE;

        iplAmbisonicsDecodeEffectApply(effect->ambisonicsEffect, &ambisonicsParams, &effect->reflectionsBuffer, &effect->outBuffer);

        iplAudioBufferInterleave(gContext, &effect->outBuffer, out);
    }

    return UNITY_AUDIODSP_OK;
}

#else

UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK create(UnityAudioEffectState* state)
{
    return UNITY_AUDIODSP_OK;
}

UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK release(UnityAudioEffectState* state)
{
    return UNITY_AUDIODSP_OK;
}

UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK getParam(UnityAudioEffectState* state, int index, float* value, char* valueStr)
{
    *value = 0.0f;
    return UNITY_AUDIODSP_OK;
}

UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK setParam(UnityAudioEffectState* state, int index, float value)
{
    return UNITY_AUDIODSP_OK;
}

UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK process(UnityAudioEffectState* state, float* in, float* out, unsigned int numSamples, int numChannelsIn, int numChannelsOut)
{
    memset(out, 0, numChannelsOut * numSamples * sizeof(float));
    return UNITY_AUDIODSP_OK;
}

#endif

}

UnityAudioEffectDefinition gReverbEffectDefinition
{
    sizeof(UnityAudioEffectDefinition),
    sizeof(UnityAudioParameterDefinition),
    UNITY_AUDIO_PLUGIN_API_VERSION,
    STEAMAUDIO_UNITY_VERSION,
    0,
    ReverbEffect::NUM_PARAMS,
    UnityAudioEffectDefinitionFlags_NeedsSpatializerData,
    "Steam Audio Reverb",
    ReverbEffect::create,
    ReverbEffect::release,
    nullptr,
    ReverbEffect::process,
    nullptr,
    ReverbEffect::gParamDefinitions,
    ReverbEffect::setParam,
    ReverbEffect::getParam,
    nullptr
};

}
