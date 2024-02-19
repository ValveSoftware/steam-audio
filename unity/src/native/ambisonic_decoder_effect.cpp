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

namespace AmbisonicDecoderEffect {

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
    bool binaural = true;

    IPLAudioBuffer inBuffer{};
    IPLAudioBuffer n3dInBuffer{};
    IPLAudioBuffer outBuffer{};

    IPLAmbisonicsDecodeEffect ambisonicsDecodeEffect = nullptr;
};

enum InitFlags
{
    INIT_NONE = 0,
    INIT_AUDIOBUFFERS = 1 << 0,
    INIT_DECODEEFFECT = 1 << 1
};

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

    auto effect = state->GetEffectData<State>();
    if (!effect)
        return initFlags;

    IPLAudioSettings audioSettings;
    audioSettings.samplingRate = state->samplerate;
    audioSettings.frameSize = state->dspbuffersize;

    auto status = IPL_STATUS_SUCCESS;

    if (numChannelsIn > 0)
    {
        status = IPL_STATUS_SUCCESS;
        if (!effect->ambisonicsDecodeEffect)
        {
            IPLAmbisonicsDecodeEffectSettings effectSettings;
            effectSettings.speakerLayout = speakerLayoutForNumChannels(numChannelsOut);
            effectSettings.hrtf = gHRTF[1];
            effectSettings.maxOrder = orderForNumChannels(numChannelsIn);

            status = iplAmbisonicsDecodeEffectCreate(gContext, &audioSettings, &effectSettings, &effect->ambisonicsDecodeEffect);
        }

        if (status == IPL_STATUS_SUCCESS)
            initFlags = static_cast<InitFlags>(initFlags | INIT_DECODEEFFECT);
    }

    if (numChannelsIn > 0 && numChannelsOut > 0)
    {
        if (!effect->inBuffer.data)
            iplAudioBufferAllocate(gContext, numChannelsIn, audioSettings.frameSize, &effect->inBuffer);

        if (!effect->n3dInBuffer.data)
            iplAudioBufferAllocate(gContext, numChannelsIn, audioSettings.frameSize, &effect->n3dInBuffer);

        if (!effect->outBuffer.data)
            iplAudioBufferAllocate(gContext, numChannelsOut, audioSettings.frameSize, &effect->outBuffer);

        initFlags = static_cast<InitFlags>(initFlags | INIT_AUDIOBUFFERS);
    }

    return initFlags;
}

void reset(UnityAudioEffectState* state)
{
    assert(state);

    auto effect = state->GetEffectData<State>();
    if (!effect)
        return;

    effect->binaural = true;
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

    auto effect = state->GetEffectData<State>();
    if (!effect)
        return UNITY_AUDIODSP_OK;

    iplAudioBufferFree(gContext, &effect->inBuffer);
    iplAudioBufferFree(gContext, &effect->n3dInBuffer);
    iplAudioBufferFree(gContext, &effect->outBuffer);

    iplAmbisonicsDecodeEffectRelease(&effect->ambisonicsDecodeEffect);

    delete state->effectdata;

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

static void remapAmbisonicsToOutChannels(unsigned int numSamples,
                                         int numAmbisonicsChannelsOut,
                                         int numChannelsOut,
                                         float* const* ambisonicsOut,
                                         float* out)
{
    // Remapping is needed to output audio in a way that makes sense to Unity. Following is a note from Unity's
    // documentation (https://docs.unity3d.com/Manual/AmbisonicAudio.html) on this issue:
    // UnityAudioAmbisonicData struct that is passed into ambisonic decoders,
    // which is very similar to the UnityAudioSpatializerData struct that is passed into spatializers, but
    // there is a new ambisonicOutChannels integer that has been added. This field will be set to the
    // DefaultSpeakerMode�s channel count. Ambisonic decoders are placed very early in the audio pipeline, where
    // we are running at the clip�s channel count, so ambisonicOutChannels tells the plugin how many of the
    // output channels will actually be used. If we are playing back a first order ambisonic audio clip(4 channels)
    // and our speaker mode is stereo (2 channels), an ambisonic decoder�s process callback will be passed in 4 for
    // the in and out channel count. The ambisonicOutChannels field will be set to 2. In this common scenario,
    // the plugin should output its spatialized data to the first 2 channels and zero out the other 2 channels.

    auto numChannels = std::min(numAmbisonicsChannelsOut, numChannelsOut);
    for (auto i = 0u; i < numSamples; ++i)
    {
        for (auto j = 0; j < numChannels; ++j)
        {
            out[i * numChannelsOut + j] = ambisonicsOut[j][i];
        }
    }
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
        reset(state);
        return UNITY_AUDIODSP_OK;
    }

    // Make sure that audio processing state has been initialized. If initialization fails, stop and emit silence.
    auto initFlags = lazyInit(state, numChannelsIn, state->ambisonicdata->ambisonicOutChannels);
    if (!(initFlags & INIT_AUDIOBUFFERS) || !(initFlags & INIT_DECODEEFFECT))
        return UNITY_AUDIODSP_OK;

    getLatestHRTF();

    auto effect = state->GetEffectData<State>();
    if (!effect)
        return UNITY_AUDIODSP_OK;

    // Local-to-world transform matrix for the source.
    auto S = state->ambisonicdata->sourcematrix;

    // The source sound field can be rotated by rotating the AudioSource.
    auto sourceAhead = unitVector(IPLVector3{ S[8], S[9], S[10] });
    auto sourceUp = unitVector(IPLVector3{ S[4], S[5], S[6] });

    // World-to-local transform matrix for the listener.
    auto L = state->ambisonicdata->listenermatrix;

    // Rotate the sound field to the listener's coordinates.
    auto ambisonicAheadX = L[0] * sourceAhead.x + L[4] * sourceAhead.y + L[8] * sourceAhead.z;
    auto ambisonicAheadY = L[1] * sourceAhead.x + L[5] * sourceAhead.y + L[9] * sourceAhead.z;
    auto ambisonicAheadZ = L[2] * sourceAhead.x + L[6] * sourceAhead.y + L[10] * sourceAhead.z;
    auto ambisonicUpX = L[0] * sourceUp.x + L[4] * sourceUp.y + L[8] * sourceUp.z;
    auto ambisonicUpY = L[1] * sourceUp.x + L[5] * sourceUp.y + L[9] * sourceUp.z;
    auto ambisonicUpZ = L[2] * sourceUp.x + L[6] * sourceUp.y + L[10] * sourceUp.z;

    auto ambisonicAhead = unitVector(convertVector(ambisonicAheadX, ambisonicAheadY, ambisonicAheadZ));
    auto ambisonicUp = unitVector(convertVector(ambisonicUpX, ambisonicUpY, ambisonicUpZ));
    auto ambisonicRight = unitVector(cross(ambisonicAhead, ambisonicUp));

    IPLVector3 listenerAhead;
    listenerAhead.x = -ambisonicRight.z;
    listenerAhead.y = -ambisonicUp.z;
    listenerAhead.z = ambisonicAhead.z;
    listenerAhead = unitVector(listenerAhead);

    IPLVector3 listenerUp;
    listenerUp.x = ambisonicRight.y;
    listenerUp.y = ambisonicUp.y;
    listenerUp.z = -ambisonicAhead.y;
    listenerUp = unitVector(listenerUp);

    IPLVector3 listenerRight = unitVector(cross(listenerAhead, listenerUp));

    iplAudioBufferDeinterleave(gContext, in, &effect->inBuffer);

    iplAudioBufferConvertAmbisonics(gContext, IPL_AMBISONICSTYPE_SN3D, IPL_AMBISONICSTYPE_N3D, &effect->inBuffer, &effect->n3dInBuffer);

    IPLAmbisonicsDecodeEffectParams decodeParams;
    decodeParams.order = orderForNumChannels(numChannelsIn);
    decodeParams.hrtf = gHRTF[0];
    decodeParams.orientation.ahead = listenerAhead;
    decodeParams.orientation.up = listenerUp;
    decodeParams.orientation.right = listenerRight;
    decodeParams.orientation.origin = IPLVector3{ 0.0f, 0.0f, 0.0f };
    decodeParams.binaural = (effect->binaural) ? IPL_TRUE : IPL_FALSE;

    iplAmbisonicsDecodeEffectApply(effect->ambisonicsDecodeEffect, &decodeParams, &effect->n3dInBuffer, &effect->outBuffer);

    remapAmbisonicsToOutChannels(numSamples, state->ambisonicdata->ambisonicOutChannels, numChannelsOut, effect->outBuffer.data, out);

    // Normalize the output so that an Ambisonics order 0 clip with peak magnitude 1 is comparable to an
    // unspatialized mono clip of peak magnitude 1.
    const auto kPi = 3.141592f;
    auto scalar = 1.0f / sqrtf(4.0f * kPi);

    for (auto i = 0u; i < numSamples * numChannelsOut; ++i)
    {
        out[i] *= scalar;
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

UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK getParam(UnityAudioEffectState* state,
                                                       int index,
                                                       float* value,
                                                       char* valueStr)
{
    *value = 0.0f;
    return UNITY_AUDIODSP_OK;
}

UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK setParam(UnityAudioEffectState* state,
                                                       int index,
                                                       float value)
{
    return UNITY_AUDIODSP_OK;
}

UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK process(UnityAudioEffectState* state,
                                                      float* in,
                                                      float* out,
                                                      unsigned int numSamples,
                                                      int numChannelsIn,
                                                      int numChannelsOut)
{
    memset(out, 0, numChannelsOut * numSamples * sizeof(float));

    if (state->flags & UnityAudioEffectStateFlags_IsPlaying)
    {
        const auto kPi = 3.141592f;
        const auto scalar = 1.0f / sqrtf(4.0f * kPi);

        auto numChannelsToCopy = std::min(state->ambisonicdata->ambisonicOutChannels, numChannelsOut);

        for (int i = 0; i < numSamples; ++i)
        {
            for (int j = 0; j < numChannelsToCopy; ++j)
            {
                out[i * numChannelsOut + j] = scalar * in[i * numChannelsIn];
            }
        }
    }

    return UNITY_AUDIODSP_OK;
}

#endif
}

UnityAudioEffectDefinition gAmbisonicDecoderEffectDefinition
{
    sizeof(UnityAudioEffectDefinition),
    sizeof(UnityAudioParameterDefinition),
    UNITY_AUDIO_PLUGIN_API_VERSION,
    STEAMAUDIO_UNITY_VERSION,
    0,
    AmbisonicDecoderEffect::NUM_PARAMS,
    UnityAudioEffectDefinitionFlags_IsAmbisonicDecoder,
    "Steam Audio Ambisonic Decoder",
    AmbisonicDecoderEffect::create,
    AmbisonicDecoderEffect::release,
    nullptr,
    AmbisonicDecoderEffect::process,
    nullptr,
    AmbisonicDecoderEffect::gParamDefinitions,
    AmbisonicDecoderEffect::setParam,
    AmbisonicDecoderEffect::getParam,
    nullptr
};

}
