//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

#include "steamaudio_fmod.h"

extern FMOD_DSP_DESCRIPTION gSpatializerEffect;
extern FMOD_DSP_DESCRIPTION gMixEffect;
extern FMOD_DSP_DESCRIPTION gReverbEffect;

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

static FMOD_PLUGINLIST gPluginList[] =
{
    { FMOD_PLUGINTYPE_DSP, &gSpatializerEffect },
    { FMOD_PLUGINTYPE_DSP, &gMixEffect },
    { FMOD_PLUGINTYPE_DSP, &gReverbEffect },
    { FMOD_PLUGINTYPE_MAX, nullptr }
};

extern void initSpatializerParamDescs();
extern void initMixParamDescs();
extern void initReverbParamDescs();

extern "C" F_EXPORT FMOD_PLUGINLIST* F_CALL FMODGetPluginDescriptionList()
{
    initSpatializerParamDescs();
    initMixParamDescs();
    initReverbParamDescs();
	return gPluginList;
}

extern "C" F_EXPORT
void F_CALL iplFMODGetVersion(unsigned int* major, unsigned int* minor, unsigned int* patch)
{
    if (major) {
        *major = STEAMAUDIO_FMOD_VERSION_MAJOR;
    }
    if (minor) {
        *minor = STEAMAUDIO_FMOD_VERSION_MINOR;
    }
    if (patch) {
        *patch = STEAMAUDIO_FMOD_VERSION_PATCH;
    }
}
