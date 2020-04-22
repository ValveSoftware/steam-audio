//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

#include "steamaudio_unity_native.h"
#include <math.h>

extern UnityAudioEffectDefinition gMixEffect;
extern UnityAudioEffectDefinition gReverbEffect;
extern UnityAudioEffectDefinition gSpatializeEffect;
extern UnityAudioEffectDefinition gAmbisonicDecoder;

IPLAudioFormat audioFormatForNumChannels(int numChannels)
{
    IPLAudioFormat format{};
    format.channelLayoutType = IPL_CHANNELLAYOUTTYPE_SPEAKERS;
    format.numSpeakers = numChannels;
    format.channelOrder = IPL_CHANNELORDER_INTERLEAVED;

    if (numChannels == 1)
        format.channelLayout = IPL_CHANNELLAYOUT_MONO;
    else if (numChannels == 2)
        format.channelLayout = IPL_CHANNELLAYOUT_STEREO;
    else if (numChannels == 4)
        format.channelLayout = IPL_CHANNELLAYOUT_QUADRAPHONIC;
    else if (numChannels == 6)
        format.channelLayout = IPL_CHANNELLAYOUT_FIVEPOINTONE;
    else if (numChannels == 8)
        format.channelLayout = IPL_CHANNELLAYOUT_SEVENPOINTONE;
    else
        format.channelLayout = IPL_CHANNELLAYOUT_CUSTOM;

    return format;
}

IPLVector3 convertVector(float x, float y, float z)
{
    return IPLVector3{ x, y, -z };
}

IPLVector3 unitVector(IPLVector3 v)
{
    auto length = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
    if (length < 1e-2f)
        length = 1e-2f;

    return IPLVector3{ v.x / length, v.y / length, v.z / length };
}

float dot(const IPLVector3& a, const IPLVector3& b)
{
    return (a.x * b.x + a.y * b.y + a.z * b.z);
}

IPLVector3 cross(const IPLVector3& a, const IPLVector3& b)
{
    IPLVector3 c;
    c.x = a.y * b.z - a.z * b.y;
    c.y = a.z * b.x - a.x * b.z;
    c.z = a.x * b.y - a.y * b.x;
    return c;
}

void crossfadeInputAndOutput(const float* inBuffer, const int numChannels, const int numSamples, float* outBuffer)
{
    auto step = 1.0f / (numSamples - 1);
    auto weight = 0.0f;

    for (auto i = 0, index = 0; i < numSamples; ++i, weight += step)
        for (auto j = 0; j < numChannels; ++j, ++index)
            outBuffer[index] = weight * outBuffer[index] + (1.0f - weight) * inBuffer[index];
}

// This function is called by Unity when it loads native audio plugins. It returns metadata that describes all of the
// effects implemented in this DLL.
extern "C" UNITY_AUDIODSP_EXPORT_API int UnityGetAudioEffectDefinitions(UnityAudioEffectDefinition*** definitions)
{
    static UnityAudioEffectDefinition* effects[] = { &gMixEffect, &gReverbEffect, &gSpatializeEffect, &gAmbisonicDecoder };
    *definitions = effects;
    return (sizeof(effects) / sizeof(effects[0]));
}

extern "C" UNITY_AUDIODSP_EXPORT_API 
void iplUnityGetVersion(unsigned int* major, unsigned int* minor, unsigned int* patch)
{
    if (major) {
        *major = STEAMAUDIO_UNITY_VERSION_MAJOR;
    }
    if (minor) {
        *minor = STEAMAUDIO_UNITY_VERSION_MINOR;
    }
    if (patch) {
        *patch = STEAMAUDIO_UNITY_VERSION_PATCH;
    }
}