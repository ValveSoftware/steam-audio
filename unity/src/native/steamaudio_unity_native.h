//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

#pragma once

#include <stdlib.h>
#include <unity5/AudioPluginInterface.h>
#include <phonon.h>

#include "steamaudio_unity_version.h"

/** Returns an IPLAudioFormat that corresponds to a given number of channels.
 */
IPLAudioFormat audioFormatForNumChannels(int numChannels);

/** Converts a 3D vector from Unity's coordinate system to Steam Audio's coordinate system.
 */
IPLVector3 convertVector(float x, float y, float z);

/** Normalizes a 3D vector.
 */
IPLVector3 unitVector(IPLVector3 v);

// Calculates the dot product of two 3D vectors.
float dot(const IPLVector3& a, const IPLVector3& b);

// Calculates the cross product of two 3D vectors.
IPLVector3 cross(const IPLVector3& a, const IPLVector3& b);

/** Crossfades between dry and wet audio.
 */
void crossfadeInputAndOutput(const float* inBuffer, const int numChannels, const int numSamples, float* outBuffer);