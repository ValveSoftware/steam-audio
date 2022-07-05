//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

#include "steamaudio_unity_native.h"

namespace MixerReturnEffect {

enum Params
{
    BINAURAL,
    NUM_PARAMS
};

UnityAudioParameterDefinition gParamDefinitions[] =
{
    { "Binaural", "", "Apply HRTF.", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f },
};

struct State
{
    bool binaural;

    IPLAudioBuffer reflectionsBuffer;
    IPLAudioBuffer inBuffer;
    IPLAudioBuffer outBuffer;

    IPLReflectionMixer reflectionMixer;
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

        if (!effect->reflectionMixer)
        {
            IPLReflectionEffectSettings effectSettings;
            effectSettings.type = gSimulationSettings.reflectionType;
            effectSettings.numChannels = numChannelsForOrder(gSimulationSettings.maxOrder);

            status = iplReflectionMixerCreate(gContext, &audioSettings, &effectSettings, &effect->reflectionMixer);

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

        if (!effect->reflectionsBuffer.data)
            iplAudioBufferAllocate(gContext, numAmbisonicChannels, audioSettings.frameSize, &effect->reflectionsBuffer);

        if (!effect->inBuffer.data)
            iplAudioBufferAllocate(gContext, numChannelsIn, audioSettings.frameSize, &effect->inBuffer);

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

    iplAudioBufferFree(gContext, &effect->reflectionsBuffer);
    iplAudioBufferFree(gContext, &effect->inBuffer);
    iplAudioBufferFree(gContext, &effect->outBuffer);

    iplReflectionMixerRelease(&effect->reflectionMixer);
    iplAmbisonicsDecodeEffectRelease(&effect->ambisonicsEffect);

    delete state->effectdata;
    state->effectdata = nullptr;

    return UNITY_AUDIODSP_OK;
}

UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK getParam(UnityAudioEffectState* state,
                                                       int index,
                                                       float* value,
                                                       char* valueStr)
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

UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK setParam(UnityAudioEffectState* state,
                                                       int index,
                                                       float value)
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

UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK process(UnityAudioEffectState* state,
                                                      float* in,
                                                      float* out,
                                                      unsigned int numSamples,
                                                      int numChannelsIn,
                                                      int numChannelsOut)
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

    auto effect = state->GetEffectData<State>();
    if (!effect)
        return UNITY_AUDIODSP_OK;

    // TODO: Need to deprecate Unity versions that don't support spatializerdata on mixer effects!
    if (!state->spatializerdata)
        return UNITY_AUDIODSP_OK;

    // World-to-local transform matrix for the listener.
    auto L = state->spatializerdata->listenermatrix;

    auto listenerCoordinates = calcListenerCoordinates(L);

    IPLReflectionEffectParams reflectionParams;
    reflectionParams.numChannels = numChannelsForOrder(gSimulationSettings.maxOrder);
    reflectionParams.tanDevice = gSimulationSettings.tanDevice;

    iplReflectionMixerApply(effect->reflectionMixer, &reflectionParams, &effect->reflectionsBuffer);

    IPLAmbisonicsDecodeEffectParams ambisonicsParams;
    ambisonicsParams.order = gSimulationSettings.maxOrder;
    ambisonicsParams.hrtf = gHRTF[0];
    ambisonicsParams.orientation = listenerCoordinates;
    ambisonicsParams.binaural = (effect->binaural) ? IPL_TRUE : IPL_FALSE;

    iplAmbisonicsDecodeEffectApply(effect->ambisonicsEffect, &ambisonicsParams, &effect->reflectionsBuffer, &effect->outBuffer);

    iplAudioBufferDeinterleave(gContext, in, &effect->inBuffer);
    iplAudioBufferMix(gContext, &effect->inBuffer, &effect->outBuffer);

    iplAudioBufferInterleave(gContext, &effect->outBuffer, out);

    return UNITY_AUDIODSP_OK;
}

}

UnityAudioEffectDefinition gMixerReturnEffectDefinition
{
    sizeof(UnityAudioEffectDefinition),
    sizeof(UnityAudioParameterDefinition),
    UNITY_AUDIO_PLUGIN_API_VERSION,
    STEAMAUDIO_UNITY_VERSION,
    0,
    MixerReturnEffect::NUM_PARAMS,
    UnityAudioEffectDefinitionFlags_NeedsSpatializerData,
    "Steam Audio Mixer Return",
    MixerReturnEffect::create,
    MixerReturnEffect::release,
    nullptr,
    MixerReturnEffect::process,
    nullptr,
    MixerReturnEffect::gParamDefinitions,
    MixerReturnEffect::setParam,
    MixerReturnEffect::getParam,
    nullptr
};
