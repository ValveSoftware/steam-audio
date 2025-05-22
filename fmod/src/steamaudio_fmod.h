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

/**
 *  DSP parameters for the "Steam Audio Spatializer" effect.
*/
typedef enum IPLSpatializerParams
{
    /**
     *  **Type**: `FMOD_DSP_PARAMETER_TYPE_DATA`
     *
     *  World-space position of the source. Automatically written by FMOD Studio.
     */
    IPL_SPATIALIZE_SOURCE_POSITION,

    /**
     *  **Type**: `FMOD_DSP_PARAMETER_TYPE_DATA`
     *
     *  Overall linear gain of this effect. Automatically read by FMOD Studio.
     */
    IPL_SPATIALIZE_OVERALL_GAIN,

    /**
     *  **Type**: `FMOD_DSP_PARAMETER_TYPE_INT`
     *
     *  **Range**: 0 to 2.
     *
     *  How to render distance attenuation.
     *
     *  -   `0`: Don't render distance attenuation.
     *  -   `1`: Use a distance attenuation value calculated using the default physics-based model.
     *  -   `2`: Use a distance attenuation value calculated using the curve specified in the FMOD Studio UI.
     */
    IPL_SPATIALIZE_APPLY_DISTANCEATTENUATION,

    /**
     *  **Type**: `FMOD_DSP_PARAMETER_TYPE_INT`
     *
     *  **Range**: 0 to 2.
     *
     *  How to render air absorption.
     *
     *  -   `0`: Don't render air absorption.
     *  -   `1`: Use air absorption values calculated using the default exponential decay model.
     *  -   `2`: Use air absorption values specified in the \c AIRABSORPTION_LOW, \c AIRABSORPTION_MID, and
     *           \c AIRABSORPTION_HIGH parameters.
     */
    IPL_SPATIALIZE_APPLY_AIRABSORPTION,

    /**
     *  **Type**: `FMOD_DSP_PARAMETER_TYPE_INT`
     *
     *  **Range**: 0 to 2.
     *
     *  How to render directivity.
     *
     *  -   `0`: Don't render directivity.
     *  -   `1`: Use a directivity value calculated using the default dipole model, driven by the
     *           \c DIRECTIVITY_DIPOLEWEIGHT and \c DIRECTIVITY_DIPOLEPOWER parameters.
     *  -   `2`: Use the directivity value specified in the \c DIRECTIVITY parameter.
     */
    IPL_SPATIALIZE_APPLY_DIRECTIVITY,

    /**
     *  **Type**: `FMOD_DSP_PARAMETER_TYPE_INT`
     *
     *  **Range**: 0 to 2.
     *
     *  How to render occlusion.
     *
     *  -   `0`: Don't render occlusion.
     *  -   `1`: Use the occlusion value calculated by the game engine using simulation, and provided via the
     *           \c SIMULATION_OUTPUTS parameter.
     *  -   `2`: Use the occlusion value specified in the \c OCCLUSION parameter.
     */
    IPL_SPATIALIZE_APPLY_OCCLUSION,

    /**
     *  **Type**: `FMOD_DSP_PARAMETER_TYPE_INT`
     *
     *  **Range**: 0 to 2.
     *
     *  How to render transmission.
     *
     *  -   `0`: Don't render transmission.
     *  -   `1`: Use the transmission values calculated by the game engine using simulation, and provided via the
     *           \c SIMULATION_OUTPUTS parameter.
     *  -   `2`: Use the transmission values specified in the \c TRANSMISSION_LOW, \c TRANSMISSION_MID, and
     *           \c TRANSMISSION_HIGH parameters.
     */
    IPL_SPATIALIZE_APPLY_TRANSMISSION,

    /**
     *  **Type**: `FMOD_DSP_PARAMETER_TYPE_BOOL`
     *
     *  If true, reflections are rendered, using the data calculated by the game engine using simulation, and provided
     *  via the \c SIMULATION_OUTPUTS parameter.
     */
    IPL_SPATIALIZE_APPLY_REFLECTIONS,

    /**
     *  **Type**: `FMOD_DSP_PARAMETER_TYPE_BOOL`
     *
     *  If true, pathing is rendered, using the data calculated by the game engine using simulation, and provided
     *  via the \c SIMULATION_OUTPUTS parameter.
     */
    IPL_SPATIALIZE_APPLY_PATHING,

    /**
     *  **Type**: `FMOD_DSP_PARAMETER_TYPE_INT`
     *
     *  **Range**: 0 to 1.
     *
     *  Controls how HRTFs are interpolated when the source moves relative to the listener.
     *
     *  - `0`: Nearest-neighbor interpolation.
     *  - `1`: Bilinear interpolation.
     */
    IPL_SPATIALIZE_HRTF_INTERPOLATION,

    /**
     *  **Type**: `FMOD_DSP_PARAMETER_TYPE_FLOAT`
     *
     *  Not currently used.
     */
    IPL_SPATIALIZE_DISTANCEATTENUATION,

    /**
     *  **Type**: `FMOD_DSP_PARAMETER_TYPE_INT`
     *
     *  **Range**: 0 to 4.
     *
     *  Type of distance attenuation curve preset to use when \c APPLY_DISTANCEATTENUATION is \c 1.
     *
     *  - `0`: Linear squared rolloff.
     *  - `1`: Linear rolloff.
     *  - `2`: Inverse rolloff.
     *  - `3`: Inverse squared rolloff.
     *  - `4`: Custom rolloff.
     */
    IPL_SPATIALIZE_DISTANCEATTENUATION_ROLLOFFTYPE,

    /**
     *  **Type**: `FMOD_DSP_PARAMETER_TYPE_FLOAT`
     *
     *  **Range**: 0 to 10000.
     *
     *  Minimum distance value for the distance attenuation curve.
     */
    IPL_SPATIALIZE_DISTANCEATTENUATION_MINDISTANCE,

    /**
     *  **Type**: `FMOD_DSP_PARAMETER_TYPE_FLOAT`
     *
     *  **Range**: 0 to 10000.
     *
     *  Maximum distance value for the distance attenuation curve.
     */
    IPL_SPATIALIZE_DISTANCEATTENUATION_MAXDISTANCE,

    /**
     *  **Type**: `FMOD_DSP_PARAMETER_TYPE_FLOAT`
     *
     *  **Range**: 0 to 1.
     *
     *  The low frequency (up to 800 Hz) EQ value for air absorption. Only used if \c APPLY_AIRABSORPTION is set to
     *  \c 2. 0 = low frequencies are completely attenuated, 1 = low frequencies are not attenuated at all.
     */
    IPL_SPATIALIZE_AIRABSORPTION_LOW,

    /**
     *  **Type**: `FMOD_DSP_PARAMETER_TYPE_FLOAT`
     *
     *  **Range**: 0 to 1.
     *
     *  The middle frequency (800 Hz - 8 kHz) EQ value for air absorption. Only used if \c APPLY_AIRABSORPTION is set
     *  to \c 2. 0 = middle frequencies are completely attenuated, 1 = middle frequencies are not attenuated at all.
     */
    IPL_SPATIALIZE_AIRABSORPTION_MID,

    /**
     *  **Type**: `FMOD_DSP_PARAMETER_TYPE_FLOAT`
     *
     *  **Range**: 0 to 1.
     *
     *  The high frequency (8 kHz and above) EQ value for air absorption. Only used if \c APPLY_AIRABSORPTION is set to
     *  \c 2. 0 = high frequencies are completely attenuated, 1 = high frequencies are not attenuated at all.
     */
    IPL_SPATIALIZE_AIRABSORPTION_HIGH,

    /**
     *  **Type**: `FMOD_DSP_PARAMETER_TYPE_FLOAT`
     *
     *  **Range**: 0 to 1.
     *
     *  The directivity attenuation value. Only used if \c APPLY_DIRECTIVITY is set to \c 2. 0 = sound is completely
     *  attenuated, 1 = sound is not attenuated at all.
     */
    IPL_SPATIALIZE_DIRECTIVITY,

    /**
     *  **Type**: `FMOD_DSP_PARAMETER_TYPE_FLOAT`
     *
     *  **Range**: 0 to 1.
     *
     *  Blends between monopole (omnidirectional) and dipole directivity patterns. 0 = pure monopole (sound is emitted
     *  in all directions with equal intensity), 1 = pure dipole (sound is focused to the front and back of the source).
     *  At 0.5, the source has a cardioid directivity, with most of the sound emitted to the front of the source. Only
     *  used if \c APPLY_DIRECTIVITY is set to \c 1.
     */
    IPL_SPATIALIZE_DIRECTIVITY_DIPOLEWEIGHT,

    /**
     *  **Type**: `FMOD_DSP_PARAMETER_TYPE_FLOAT`
     *
     *  **Range**: 0 to 4.
     *
     *  Controls how focused the dipole directivity is. Higher values result in sharper directivity patterns. Only used
     *  if \c APPLY_DIRECTIVITY is set to \c 1.
     */
    IPL_SPATIALIZE_DIRECTIVITY_DIPOLEPOWER,

    /**
     *  **Type**: `FMOD_DSP_PARAMETER_TYPE_FLOAT`
     *
     *  **Range**: 0 to 1.
     *
     *  The occlusion attenuation value. Only used if \c APPLY_OCCLUSION is set to \c 2. 0 = sound is completely
     *  attenuated, 1 = sound is not attenuated at all.
     */
    IPL_SPATIALIZE_OCCLUSION,

    /**
     *  **Type**: `FMOD_DSP_PARAMETER_TYPE_INT`
     *
     *  **Range**: 0 to 1.
     *
     *  Specifies how the transmission filter is applied.
     *
     * - `0`: Transmission is modeled as a single attenuation factor.
     * - `1`: Transmission is modeled as a 3-band EQ.
     */
    IPL_SPATIALIZE_TRANSMISSION_TYPE,

    /**
     *  **Type**: `FMOD_DSP_PARAMETER_TYPE_FLOAT`
     *
     *  **Range**: 0 to 1.
     *
     *  The low frequency (up to 800 Hz) EQ value for transmission. Only used if \c APPLY_TRANSMISSION is set to \c 2.
     *  0 = low frequencies are completely attenuated, 1 = low frequencies are not attenuated at all.
     */
    IPL_SPATIALIZE_TRANSMISSION_LOW,

    /**
     *  **Type**: `FMOD_DSP_PARAMETER_TYPE_FLOAT`
     *
     *  **Range**: 0 to 1.
     *
     *  The middle frequency (800 Hz to 8 kHz) EQ value for transmission. Only used if \c APPLY_TRANSMISSION is set to
     *  \c 2. 0 = middle frequencies are completely attenuated, 1 = middle frequencies are not attenuated at all.
     */
    IPL_SPATIALIZE_TRANSMISSION_MID,

    /**
     *  **Type**: `FMOD_DSP_PARAMETER_TYPE_FLOAT`
     *
     *  **Range**: 0 to 1.
     *
     *  The high frequency (8 kHz and above) EQ value for transmission. Only used if \c APPLY_TRANSMISSION is set to
     *  \c 2. 0 = high frequencies are completely attenuated, 1 = high frequencies are not attenuated at all.
     */
    IPL_SPATIALIZE_TRANSMISSION_HIGH,

    /**
     *  **Type**: `FMOD_DSP_PARAMETER_TYPE_FLOAT`
     *
     *  **Range**: 0 to 1.
     *
     *  The contribution of the direct sound path to the overall mix for this event. Lower values reduce the
     *  contribution more.
     */
    IPL_SPATIALIZE_DIRECT_MIXLEVEL,

    /**
     *  **Type**: `FMOD_DSP_PARAMETER_TYPE_BOOL`
     *
     *  If true, applies HRTF-based 3D audio rendering to reflections. Results in an improvement in spatialization
     *  quality when using convolution or hybrid reverb, at the cost of slightly increased CPU usage.
     */
    IPL_SPATIALIZE_REFLECTIONS_BINAURAL,

    /**
     *  **Type**: `FMOD_DSP_PARAMETER_TYPE_FLOAT`
     *
     *  **Range**: 0 to 10.
     *
     *  The contribution of reflections to the overall mix for this event. Lower values reduce the contribution more.
     */
    IPL_SPATIALIZE_REFLECTIONS_MIXLEVEL,

    /**
     *  **Type**: `FMOD_DSP_PARAMETER_TYPE_BOOL`
     *
     *  If true, applies HRTF-based 3D audio rendering to pathing. Results in an improvement in spatialization
     *  quality, at the cost of slightly increased CPU usage.
     */
    IPL_SPATIALIZE_PATHING_BINAURAL,

    /**
     *  **Type**: `FMOD_DSP_PARAMETER_TYPE_FLOAT`
     *
     *  **Range**: 0 to 10.
     *
     *  The contribution of pathing to the overall mix for this event. Lower values reduce the contribution more.
     */
    IPL_SPATIALIZE_PATHING_MIXLEVEL,

    /**
     *  **Type**: `FMOD_DSP_PARAMETER_TYPE_DATA`
     *
     *  **DEPRECATED**
     *
     *  Pointer to the `IPLSimulationOutputs` structure containing simulation results.
     */
    IPL_SPATIALIZE_SIMULATION_OUTPUTS,

    /**
     *  **Type**: `FMOD_DSP_PARAMETER_TYPE_BOOL`
     *
     *  If true, applies HRTF-based 3D audio rendering to the direct sound path. Otherwise, sound is panned based on
     *  the speaker configuration.
     */
    IPL_SPATIALIZE_DIRECT_BINAURAL,

    /**
     *  **Type**: `FMOD_DSP_PARAMETER_TYPE_DATA`
     *
     *  (FMOD Studio 2.02+) The event's min/max distance range. Automatically set by FMOD Studio.
     */
    IPL_SPATIALIZE_DISTANCE_ATTENUATION_RANGE,

    /**
     *  **Type**: `FMOD_DSP_PARAMETER_TYPE_INT`
     *
     *  Handle of the `IPLSource` object to use for obtaining simulation results. The handle can
     *  be obtained by calling `iplFMODAddSource`.
     */
    IPL_SPATIALIZE_SIMULATION_OUTPUTS_HANDLE,
    
    /**
     *  **Type**: `FMOD_DSP_PARAMETER_TYPE_INT`
     *
     *  **Range**: 0 to 2.
     *
     *  Controls the output format.
     *
     *  - `0`: Output will be the format in FMOD's mixer.
     *  - `1`: Output will be the format from FMOD's final output.
     *  - `2`: Output will be the format from the event's input.
     */
    IPL_SPATIALIZE_OUTPUT_FORMAT,

    /** The number of parameters in this effect. */
    IPL_SPATIALIZE_NUM_PARAMS
} IPLSpatializerParams;

/**
 *  DSP parameters for the "Steam Audio Reverb" effect.
 */
typedef enum IPLReverbParams
{
    /**
     *  **Type**: `FMOD_DSP_PARAMETER_TYPE_BOOL`
     *
     *  If true, applies HRTF-based 3D audio rendering to reverb. Results in an improvement in spatialization quality
     *  when using convolution or hybrid reverb, at the cost of slightly increased CPU usage.
     */
    IPL_REVERB_BINAURAL,

    /**
     *  **Type**: `FMOD_DSP_PARAMETER_TYPE_INT`
     *
     *  **Range**: 0 to 2.
     *
     *  Controls the output format.
     *
     *  - `0`: Output will be the format in FMOD's mixer.
     *  - `1`: Output will be the format from FMOD's final output.
     *  - `2`: Output will be the format from the event's input.
     */
    IPL_REVERB_OUTPUT_FORMAT,

    /** The number of parameters in this effect. */
    IPL_REVERB_NUM_PARAMS
} IPLReverbParams;

/**
 *  DSP parameters for the "Steam Audio Mixer Return" effect.
 */
typedef enum IPLMixerReturnParams
{
    /**
     *  **Type**: `FMOD_DSP_PARAMETER_TYPE_BOOL`
     *
     *  If true, applies HRTF-based 3D audio rendering to mixed reflected sound. Results in an improvement in
     *  spatialization quality, at the cost of slightly increased CPU usage.
     */
    IPL_MIXRETURN_BINAURAL,

    /**
     *  **Type**: `FMOD_DSP_PARAMETER_TYPE_INT`
     *
     *  **Range**: 0 to 2.
     *
     *  Controls the output format.
     *
     *  - `0`: Output will be the format in FMOD's mixer.
     *  - `1`: Output will be the format from FMOD's final output.
     *  - `2`: Output will be the format from the event's input.
     */
    IPL_MIXRETURN_OUTPUT_FORMAT,

    /** The number of parameters in this effect. */
    IPL_MIXRETURN_NUM_PARAMS
} IPLMixerReturnParams;

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
