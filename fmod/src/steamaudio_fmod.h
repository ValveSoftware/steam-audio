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

#include <assert.h>
#include <math.h>
#include <string.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <queue>
#include <unordered_map>

#if defined(IPL_OS_WINDOWS)
#include <Windows.h>
#elif defined(IPL_OS_MACOSX)
#endif

#include <fmod/fmod.hpp>

#include <phonon.h>

#include "steamaudio_fmod_version.h"
#include "library.h"


namespace SteamAudioFMOD {

// --------------------------------------------------------------------------------------------------------------------
// Parameter Types
// --------------------------------------------------------------------------------------------------------------------

enum ParameterApplyType
{
    PARAMETER_DISABLE,
    PARAMETER_SIMULATIONDEFINED,
    PARAMETER_USERDEFINED,
};

enum ParameterSpeakerFormatType
{
    PARAMETER_FROM_MIXER,
    PARAMETER_FROM_FINAL_OUTPUT,
    PARAMETER_FROM_INPUT,
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
extern std::atomic<bool> gHRTFDisabled;


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

// Initialize FMOD's outBuffer (output format, channel count, mask). Returns true on success.
bool initFmodOutBufferFormat(const FMOD_DSP_BUFFER_ARRAY* inBuffers, 
                             FMOD_DSP_BUFFER_ARRAY* outBuffers,
                             FMOD_DSP_STATE* state,
                             ParameterSpeakerFormatType outputFormat);


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

}


// --------------------------------------------------------------------------------------------------------------------
// API Functions
// --------------------------------------------------------------------------------------------------------------------

extern "C" {

// This function is called by FMOD Studio when it loads plugins. It returns metadata that describes all of the
// effects implemented in this DLL.
F_EXPORT FMOD_PLUGINLIST* F_CALL FMODGetPluginDescriptionList();

F_EXPORT FMOD_DSP_DESCRIPTION* F_CALL FMOD_SteamAudio_Spatialize_GetDSPDescription();
F_EXPORT FMOD_DSP_DESCRIPTION* F_CALL FMOD_SteamAudio_MixerReturn_GetDSPDescription();
F_EXPORT FMOD_DSP_DESCRIPTION* F_CALL FMOD_SteamAudio_Reverb_GetDSPDescription();

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

F_EXPORT IPLint32 F_CALL iplFMODAddSource(IPLSource source);

F_EXPORT void F_CALL iplFMODRemoveSource(IPLint32 handle);

F_EXPORT void F_CALL iplFMODSetHRTFDisabled(bool disabled);

}
