//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

#pragma once

#include <stdlib.h>
#include <unity5/AudioPluginInterface.h>
#include <phonon.h>

#define STEAM_AUDIO_PLUGIN_VERSION 0x020006

/** Returns an IPLAudioFormat that corresponds to a given number of channels.
 */
IPLAudioFormat audioFormatForNumChannels(int numChannels);

/** Converts a 3D vector from Unity's coordinate system to Steam Audio's coordinate system.
 */
IPLVector3 convertVector(float x, float y, float z);

/** Normalizes a 3D vector.
 */
IPLVector3 unitVector(IPLVector3 v);

/** Crossfades between dry and wet audio.
 */
void crossfadeInputAndOutput(const float* inBuffer, const int numChannels, const int numSamples, float* outBuffer);