//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

#pragma once

#include <assert.h>
#include <math.h>
#include <string.h>

#include <atomic>

#if defined(IPL_OS_WINDOWS)
#include <Windows.h>
#elif defined(IPL_OS_MACOSX)
#endif

#include <fmod/fmod.hpp>

#include <phonon.h>

#include "steamaudio_fmod_version.h"
#include "library.h"


// --------------------------------------------------------------------------------------------------------------------
// Parameter Types
// --------------------------------------------------------------------------------------------------------------------

enum ParameterApplyType
{
    PARAMETER_DISABLE,
    PARAMETER_SIMULATIONDEFINED,
    PARAMETER_USERDEFINED,
};


// --------------------------------------------------------------------------------------------------------------------
// Global State
// --------------------------------------------------------------------------------------------------------------------

extern IPLContext gContext;
extern IPLHRTF gHRTF[2];
extern IPLSimulationSettings gSimulationSettings;
extern IPLSource gReverbSource[2];
extern IPLReflectionMixer gReflectionMixer[2];

extern std::atomic<bool> gNewHRTFWritten;
extern std::atomic<bool> gIsSimulationSettingsValid;
extern std::atomic<bool> gNewReverbSourceWritten;
extern std::atomic<bool> gNewReflectionMixerWritten;


// --------------------------------------------------------------------------------------------------------------------
// API Functions
// --------------------------------------------------------------------------------------------------------------------

extern "C" {

// This function is called by FMOD Studio when it loads plugins. It returns metadata that describes all of the
// effects implemented in this DLL.
F_EXPORT FMOD_PLUGINLIST* F_CALL FMODGetPluginDescriptionList();

/**
 *  Returns the version of the FMOD Studio integration being used.
 * 
 *  \param  major   Major version number. For example, "1" in "1.2.3".
 *  \param  minor   Minor version number. For example, "2" in "1.2.3".
 *  \param  patch   Patch version number. For example, "3" in "1.2.3".
 */
F_EXPORT void F_CALL iplFMODGetVersion(unsigned int* major, unsigned int* minor, unsigned int* patch);

/**
 *  Initializes the FMOD Studio integration. This function must be called before creating any Steam Audio DSP effects.
 * 
 *  \param  context The Steam Audio context created by the game engine when initializing Steam Audio.
 */
F_EXPORT void F_CALL iplFMODInitialize(IPLContext context);

/**
 *  Shuts down the FMOD Studio integration. This function must be called after all Steam Audio DSP effects have been
 *  destroyed.
 */
F_EXPORT void F_CALL iplFMODTerminate();

/**
 *  Specifies the HRTF to use for spatialization in subsequent audio frames. This function must be called once during
 *  initialization, after \c iplFMODInitialize. It should also be called whenever the game engine needs to change the
 *  HRTF.
 * 
 *  \param  hrtf    The HRTF to use for spatialization.
 */
F_EXPORT void F_CALL iplFMODSetHRTF(IPLHRTF hrtf);

/**
 *  Specifies the simulation settings used by the game engine for simulating direct and/or indirect sound propagation.
 *  This function must be called once during initialization, after \c iplFMODInitialize.
 * 
 *  \param  simulationSettings  The simulation settings used by the game engine.
 */
F_EXPORT void F_CALL iplFMODSetSimulationSettings(IPLSimulationSettings simulationSettings);

/**
 *  Specifies the \c IPLSource object used by the game engine for simulating reverb. Typically, listener-centric reverb
 *  is simulated by creating an \c IPLSource object with the same position as the listener, and simulating reflections.
 *  To render this simulated reverb, call this function and pass it the \c IPLSource object used.
 * 
 *  \param  reverbSource    The source object used by the game engine for simulating reverb.
 */
F_EXPORT void F_CALL iplFMODSetReverbSource(IPLSource reverbSource);

}


// --------------------------------------------------------------------------------------------------------------------
// Helper Functions
// --------------------------------------------------------------------------------------------------------------------

// Returns an IPLSpeakerLayout that corresponds to a given number of channels.
IPLSpeakerLayout speakerLayoutForNumChannels(int numChannels);

// Returns the Ambisonics order corresponding to a given number of channels.
int orderForNumChannels(int numChannels);

// Returns the number of channels corresponding to a given Ambisonics order.
int numChannelsForOrder(int order);

// Returns the number of samples corresponding to a given duration and sampling rate.
int numSamplesForDuration(float duration,
                          int samplingRate);

// Converts a 3D vector from FMOD Studio's coordinate system to Steam Audio's coordinate system.
IPLVector3 convertVector(float x,
                         float y,
                         float z);

// Normalizes a 3D vector.
IPLVector3 unitVector(IPLVector3 v);

// Calculates the dot product of two 3D vectors.
float dot(const IPLVector3& a,
          const IPLVector3& b);

// Calculates the cross product of two 3D vectors.
IPLVector3 cross(const IPLVector3& a,
                 const IPLVector3& b);

// Calculates the distance between two points.
float distance(const IPLVector3& a,
               const IPLVector3& b);

// Ramps a volume from a start value to an end value, applying it to a buffer.
void applyVolumeRamp(float startVolume,
                     float endVolume,
                     int numSamples,
                     float* buffer);

// Converts from FMOD's coordinate system structure to Steam Audio's.
IPLCoordinateSpace3 calcCoordinates(const FMOD_3D_ATTRIBUTES& attributes);

// Extracts listener coordinate system from the transform provided by FMOD.
IPLCoordinateSpace3 calcListenerCoordinates(FMOD_DSP_STATE* state);

// Returns true if we're currently running in the FMOD Studio editor.
bool isRunningInEditor();

// Creates a context and default HRTF. Should only be called if isRunningInEditor returns true.
void initContextAndDefaultHRTF(IPLAudioSettings audioSettings);
