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

#pragma once

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <atomic>
#include <algorithm>
#include <limits>
#include <memory>
#include <mutex>
#include <queue>
#include <unordered_map>

#include <unity5/AudioPluginInterface.h>

#include <phonon.h>

#include "steamaudio_unity_version.h"


// --------------------------------------------------------------------------------------------------------------------
// API Functions
// --------------------------------------------------------------------------------------------------------------------

extern "C" {

#if !defined(IPL_OS_UNSUPPORTED)

/** Settings for perspective correction. */
typedef struct {
    IPLbool enabled;
    IPLfloat32 xfactor;
    IPLfloat32 yfactor;
    IPLMatrix4x4 transform;
} IPLUnityPerspectiveCorrection;

#endif

// This function is called by Unity when it loads native audio plugins. It returns metadata that describes all of the
// effects implemented in this DLL.
UNITY_AUDIODSP_EXPORT_API int UnityGetAudioEffectDefinitions(UnityAudioEffectDefinition*** definitions);

#if !defined(IPL_OS_UNSUPPORTED)

UNITY_AUDIODSP_EXPORT_API void UNITY_AUDIODSP_CALLBACK iplUnityGetVersion(unsigned int* major, unsigned int* minor, unsigned int* patch);

UNITY_AUDIODSP_EXPORT_API void UNITY_AUDIODSP_CALLBACK iplUnityInitialize(IPLContext context);

UNITY_AUDIODSP_EXPORT_API void UNITY_AUDIODSP_CALLBACK iplUnityTerminate();

UNITY_AUDIODSP_EXPORT_API void UNITY_AUDIODSP_CALLBACK iplUnitySetPerspectiveCorrection(IPLUnityPerspectiveCorrection correction);

UNITY_AUDIODSP_EXPORT_API void UNITY_AUDIODSP_CALLBACK iplUnitySetHRTF(IPLHRTF hrtf);

UNITY_AUDIODSP_EXPORT_API void UNITY_AUDIODSP_CALLBACK iplUnitySetSimulationSettings(IPLSimulationSettings simulationSettings);

UNITY_AUDIODSP_EXPORT_API void UNITY_AUDIODSP_CALLBACK iplUnitySetReverbSource(IPLSource reverbSource);

UNITY_AUDIODSP_EXPORT_API IPLint32 UNITY_AUDIODSP_CALLBACK iplUnityAddSource(IPLSource source);

UNITY_AUDIODSP_EXPORT_API void UNITY_AUDIODSP_CALLBACK iplUnityRemoveSource(IPLint32 handle);

UNITY_AUDIODSP_EXPORT_API void UNITY_AUDIODSP_CALLBACK iplUnitySetHRTFDisabled(bool disabled);

#endif

}

namespace SteamAudioUnity {

#if !defined(IPL_OS_UNSUPPORTED)

// --------------------------------------------------------------------------------------------------------------------
// Global State
// --------------------------------------------------------------------------------------------------------------------

extern IPLContext gContext;
extern IPLHRTF gHRTF[2];
extern IPLUnityPerspectiveCorrection gPerspectiveCorrection[2];
extern IPLSimulationSettings gSimulationSettings;
extern IPLSource gReverbSource[2];
extern IPLReflectionMixer gReflectionMixer[2];

extern std::atomic<bool> gNewHRTFWritten;
extern std::atomic<bool> gNewPerspectiveCorrectionWritten;
extern std::atomic<bool> gIsSimulationSettingsValid;
extern std::atomic<bool> gNewReverbSourceWritten;
extern std::atomic<bool> gNewReflectionMixerWritten;
extern std::atomic<bool> gHRTFDisabled;


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

void getLatestPerspectiveCorrection();
void setPerspectiveCorrection(IPLUnityPerspectiveCorrection& correction);

// --------------------------------------------------------------------------------------------------------------------
// SourceManager
// --------------------------------------------------------------------------------------------------------------------

// Manages assigning a 32-bit integer handle to IPLSource objects, so the C# scripts can reference a specific IPLSource
// in a single call to AudioSource.SetSpatializerFloat or similar.
class SourceManager
{
public:
    SourceManager();
    ~SourceManager();

    // Registers a source that has already been created, and returns the corresponding handle. A reference to the
    // IPLSource will be retained by this object.
    int32_t addSource(IPLSource source);

    // Unregisters a source (by handle), and releases the reference.
    void removeSource(int32_t handle);

    // Returns the IPLSource corresponding to a given handle. If the handle is invalid or the IPLSource has been
    // released, returns nullptr. Does not retain an additional reference.
    IPLSource getSource(int32_t handle);

private:
    // The next available integer that hasn't yet been assigned as the handle for any source.
    int32_t mNextHandle;

    // Handles for sources that have been unregistered, and which can now be reused. We will prefer reusing free
    // handle values over using a new handle value.
    std::priority_queue<int32_t> mFreeHandles;

    // The mapping from handle values to IPLSource objects.
    std::unordered_map<int32_t, IPLSource> mSources;

    // Synchronizes access to the handle priority queue and related values.
    std::mutex mHandleMutex;

    // Synchronizes access to the handle-to-source map.
    std::mutex mSourceMutex;
};

#endif

}
