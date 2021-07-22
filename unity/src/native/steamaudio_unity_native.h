//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

#pragma once

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <atomic>
#include <algorithm>
#include <limits>

#include <unity5/AudioPluginInterface.h>

#include <phonon.h>

#include "steamaudio_unity_version.h"


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

// This function is called by Unity when it loads native audio plugins. It returns metadata that describes all of the
// effects implemented in this DLL.
UNITY_AUDIODSP_EXPORT_API int UnityGetAudioEffectDefinitions(UnityAudioEffectDefinition*** definitions);

UNITY_AUDIODSP_EXPORT_API void UNITY_AUDIODSP_CALLBACK iplUnityGetVersion(unsigned int* major, unsigned int* minor, unsigned int* patch);

UNITY_AUDIODSP_EXPORT_API void UNITY_AUDIODSP_CALLBACK iplUnityInitialize(IPLContext context);

UNITY_AUDIODSP_EXPORT_API void UNITY_AUDIODSP_CALLBACK iplUnityTerminate();

UNITY_AUDIODSP_EXPORT_API void UNITY_AUDIODSP_CALLBACK iplUnitySetHRTF(IPLHRTF hrtf);

UNITY_AUDIODSP_EXPORT_API void UNITY_AUDIODSP_CALLBACK iplUnitySetSimulationSettings(IPLSimulationSettings simulationSettings);

UNITY_AUDIODSP_EXPORT_API void UNITY_AUDIODSP_CALLBACK iplUnitySetReverbSource(IPLSource reverbSource);

}


// --------------------------------------------------------------------------------------------------------------------
// Helper Functions
// --------------------------------------------------------------------------------------------------------------------

// Returns an IPLAudioFormat that corresponds to a given number of channels.
IPLSpeakerLayout speakerLayoutForNumChannels(int numChannels);

// Returns the Ambisonics order corresponding to a given number of channels.
int orderForNumChannels(int numChannels);

// Returns the number of channels corresponding to a given Ambisonics order.
int numChannelsForOrder(int order);

// Returns the number of samples corresponding to a given duration and sampling rate.
int numSamplesForDuration(float duration, 
                          int samplingRate);

// Converts a 3D vector from Unity's coordinate system to Steam Audio's coordinate system.
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

// Ramps a volume from a start value to an end value, applying it to a buffer.
void applyVolumeRamp(float startVolume, 
                     float endVolume, 
                     int numSamples,
                     float* buffer);

// Crossfades between dry and wet audio.
//void crossfadeInputAndOutput(const float* inBuffer, const int numChannels, const int numSamples, float* outBuffer);

// Extracts the source coordinate system from the transform provided by Unity.
IPLCoordinateSpace3 calcSourceCoordinates(const float* sourceMatrix);

// Extracts listener coordinate system from the transform provided by Unity.
IPLCoordinateSpace3 calcListenerCoordinates(const float* listenerMatrix);

void getLatestHRTF();

void setHRTF(IPLHRTF hrtf);
