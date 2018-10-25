//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

#pragma once

#include <fmod/fmod.hpp>
#include <phonon/phonon.h>

#include "steamaudio_fmod_version.h"

/** Returns an IPLAudioFormat that corresponds to a given number of channels.
*/
IPLAudioFormat audioFormatForNumChannels(int numChannels);

/** Converts a 3D vector from Unity's coordinate system to Steam Audio's coordinate system.
*/
IPLVector3 convertVector(float x, float y, float z);