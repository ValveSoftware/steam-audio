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

#if defined(_WIN32)
#define IPL_OS_WINDOWS
#elif defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_OSX
#define IPL_OS_MACOSX
#elif TARGET_OS_IOS
#define IPL_OS_IOS
#endif
#endif

// Visual Studio 2022 v17.10 or later has an ABI-breaking change in the constructor of std::mutex. This can cause
// a crash when linking against code built using older versions of Visual Studio, or when running under the Wwise
// editor, which may manually load an older version of the Visual C++ runtime library. The below definition
// reverts to the older behavior of the std::mutex constructor.
#if defined(IPL_OS_WINDOWS)
#define _DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR
#endif

#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_map>

#if defined(IPL_OS_WINDOWS)
#define NOMINMAX
#include <Windows.h>
#elif defined(IPL_OS_MACOSX)
#include <dlfcn.h>
#include <mach-o/dyld.h>
#endif

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/SoundEngine/Common/AkSoundEngine.h>

#include <phonon.h>

#include "SteamAudioVersion.h"


namespace SteamAudioWwise {

// --------------------------------------------------------------------------------------------------------------------
// Object<T>
// --------------------------------------------------------------------------------------------------------------------

/** Generic function pointer type for a Steam Audio API object retain function, i.e. iplXyzRetain. */
template <typename T>
using RetainFn = T(IPLCALL *)(T object);

/** Generic function pointer type for a Steam Audio API object release function, i.e. iplXyzRelease. */
template <typename T>
using ReleaseFn = void (IPLCALL *)(T* object);

/**
 * Wrapper around a Steam Audio API object. Automatically retains a reference when written to, and
 * releases the reference when destroyed.
 * 
 * Instantiate using, e.g. Object<IPLXyz, iplXyzRetain, iplXyzRelease>.
 */
template <typename T, RetainFn<T> Retain, ReleaseFn<T> Release>
class Object
{
private:
    /** The reference to the API object. */
    T m_value;

public:
    /**
     * Calls the default constructor on the object reference.
     */
    Object()
        : m_value{T{}}
    {}

    ~Object()
    {
        Reset();
    }

    /**
     * Releases the reference.
     */
    void Reset()
    {
        Release(&m_value);
    }

    /**
     * Returns the current value of the reference.
     */
    T Read()
    {
        return m_value;
    }

    /**
     * Sets the reference to a new value.
     * 
     * The reference is retained, so the caller can release the passed-in reference after calling this
     * function if needed.
     */
    bool Write(const T& value)
    {
        m_value = Retain(value);
        return true;
    }
};


// --------------------------------------------------------------------------------------------------------------------
// DoubleBuffer<T>
// --------------------------------------------------------------------------------------------------------------------

/**
 * Wrapper around a Steam Audio API object. Automatically retains a reference when written to, and
 * releases the reference when destroyed.
 * 
 * The value is double-buffered, and this template should be used when the value is written by one thread and read
 * by a different thread.
 * 
 * Instantiate using, e.g. DoubleBufferedObject<IPLXyz, iplXyzRetain, iplXyzRelease>.
 */
template <typename T, RetainFn<T> Retain, ReleaseFn<T> Release>
class DoubleBufferedObject
{
private:
    /** The double buffer of API object references. Index 0 is the front buffer (for reading), index 1 is the
        back buffer (for writing). */
    T m_buffer[2];

    /** True if a new value has been written to the back buffer and has not been consumed yet. */
    std::atomic<bool> m_newValueWritten;

public:
    /**
     * Calls the default constructor on the references stored in both buffers.
     */
    DoubleBufferedObject()
        : m_buffer{T{}, T{}}
        , m_newValueWritten{false}
    {}

    ~DoubleBufferedObject()
    {
        Reset();
    }

    /**
     * Releases the references stored in both buffers.
     */
    void Reset()
    {
        Release(&m_buffer[1]);
        Release(&m_buffer[0]);
    }

    /**
     * Returns the current value of the reference from the front buffer.
     * 
     * Before doing so, if a new value has recently been written into the back buffer, it is
     * retained and copied into the front buffer.
     */
    T Read()
    {
        if (m_newValueWritten)
        {
            Release(&m_buffer[0]);
            m_buffer[0] = Retain(m_buffer[1]);

            m_newValueWritten = false;
        }

        return m_buffer[0];
    }

    /**
     * Writes a new value for the reference into the back buffer.
     * 
     * The reference is retained, so the caller can release the passed-in reference after calling this
     * function if needed.
     * 
     * If the back buffer already has a new value that has not been consumed, the back buffer is
     * not updated.
     */
    bool Write(const T& value)
    {
        if (m_buffer[1] == value)
            return true;

        if (!m_newValueWritten)
        {
            Release(&m_buffer[1]);
            m_buffer[1] = Retain(value);

            m_newValueWritten = true;
            return true;
        }
        else
        {
            return false;
        }
    }
};


using Context = Object<IPLContext, iplContextRetain, iplContextRelease>;
using DoubleBufferedHRTF = DoubleBufferedObject<IPLHRTF, iplHRTFRetain, iplHRTFRelease>;
using DoubleBufferedSource = DoubleBufferedObject<IPLSource, iplSourceRetain, iplSourceRelease>;
using DoubleBufferedReflectionMixer = DoubleBufferedObject<IPLReflectionMixer, iplReflectionMixerRetain, iplReflectionMixerRelease>;


// --------------------------------------------------------------------------------------------------------------------
// SourceMap
// --------------------------------------------------------------------------------------------------------------------

/**
 * A thread-safe mapping from AkGameObjectID values to (double-buffered) IPLSource objects.
 */
struct SourceMap
{
    /** The map. */
    std::unordered_map<AkGameObjectID, std::shared_ptr<DoubleBufferedSource>> map;

    /** A mutex for allowing thread-safe access to the map. */
    std::mutex mutex;

    /**
     * Maps the given AkGameObjectID to the given IPLSource.
     */
    void Add(AkGameObjectID gameObjectID, IPLSource source);

    /**
     * Removes the mapping between the given AkGameObjectID and any IPLSource it is mapped to.
     */
    void Remove(AkGameObjectID gameObjectID);

    /**
     * Returns the (double-buffered) IPLSource that the given AkGameObjectID is mapped to.
     * 
     * This is returned as a shared_ptr, so typically an effect plugin will call this during Init(),
     * after which it doesn't have worry about Remove() being called while the plugin is still
     * processing audio, or about any performance penality due to Get() locking a mutex.
     */
    std::shared_ptr<DoubleBufferedSource> Get(AkGameObjectID gameObjectID);
};


// --------------------------------------------------------------------------------------------------------------------
// GlobalState
// --------------------------------------------------------------------------------------------------------------------

/**
 * Global state of the Steam Audio Wwise integration.
 * 
 * Initialized either automatically when running in the Wwise editor, or explicitly (by calling iplWwiseInitialize)
 * otherwise.
 */
struct GlobalState
{
    /** Handle to the Steam Audio core library (e.g. phonon.dll). */
#if defined(IPL_OS_WINDOWS)
    HMODULE library;
#else
    void* library;
#endif

    /** Pointer to the iplContextCreate function loaded from the core library. */
    decltype(iplContextCreate)* iplContextCreateFn;

    /** The Steam Audio context. */
    Context context;

    /** The Wwise global plugin context. */
    AK::IAkGlobalPluginContext* globalPluginContext;

    /** The current HRTF. */
    DoubleBufferedHRTF hrtf;

    /** The mapping between AkGameObjectID and IPLSource. */
    SourceMap sourceMap;

    /** Indicates how many Wwise integration objects reference this global state. One reference is typically
        retained by each effect object, and one global reference is retained by iplWwiseInitialize. */
    std::atomic<int> refCount;

    /** Conversion between game engine distance units to Steam Audio distance units (which are in meters). */
    float metersPerUnit;

    /** The simulation settings provided by the game engine. */
    IPLSimulationSettings simulationSettings;

    /** Set to true once we have received simulation settings from the game engine. */
    std::atomic<bool> simulationSettingsValid;

    /** The IPLReflectionMixer used by the mix return effect. */
    DoubleBufferedReflectionMixer reflectionMixer;

    /** The IPLSource used by the game engine for simulating reverb. */
    DoubleBufferedSource reverbSource;

    /**
     * Default constructor.
     */
    GlobalState();

    /**
     * Increments the reference count.
     */
    void Retain();

    /**
     * Decrements the reference count.
     * 
     * If the reference count becomes 0, the global state is reset.
     */
    void Release();

    /**
     * Returns the global state.
     * 
     * Retain() must be called explicitly if needed.
     */
    static GlobalState& Get();
};


// --------------------------------------------------------------------------------------------------------------------
// Helper Functions
// --------------------------------------------------------------------------------------------------------------------

/**
 * Returns an IPLSpeakerLayout that corresponds to a given number of channels.
 */
IPLSpeakerLayout SpeakerLayoutForNumChannels(int numChannels);

/**
 * Returns the Ambisonics order corresponding to a given number of channels.
 */
int OrderForNumChannels(int numChannels);

/**
 * Returns the number of channels corresponding to a given Ambisonics order.
 */
int NumChannelsForOrder(int order);

/**
 * Returns the number of samples corresponding to a given duration and sampling rate.
 */
int NumSamplesForDuration(float duration, int samplingRate);

/**
 * Converts a 3D vector from Wwise's coordinate system to Steam Audio's coordinate system.
 */
IPLVector3 ConvertVector(const AkVector64& vec);

/**
 * Performs dot product between the vectors.
 */
float Dot(const IPLVector3& a, const IPLVector3& b);

/**
 * Normalizes a 3D vector.
 */
IPLVector3 UnitVector(IPLVector3 v);

/**
 * Calculates the cross product of two 3D vectors.
 */
IPLVector3 CrossVector(const IPLVector3& a, const IPLVector3& b);

/**
 * Extracts coordinate system from the transform provided by Wwise.
 */
IPLCoordinateSpace3 CalculateCoordinates(AkWorldTransform& transform);

/**
 * Returns true if the calling code is running under the Wwise editor.
 */
bool IsRunningInEditor();

/**
 * If running under the Wwise editor, ensures that the Steam Audio SDK library has been loaded,
 * and a context has been created.
 * 
 * Otherwise, checks to see if a valid Steam Audio context has been passed in via iplWwiseInitialize.
 */
bool EnsureSteamAudioContextExists(IPLAudioSettings& in_audioSettings, AK::IAkGlobalPluginContext* in_pGlobalPluginContext);

/**
 * Applies a linear volume ramp to an audio buffer, starting at prevVolume and ending at volume.
 * 
 * When the function returns, prevVolume is updated to be equal to volume.
 */
void ApplyVolumeRamp(float volume, float& prevVolume, const IPLAudioBuffer& audioBuffer);

}

// --------------------------------------------------------------------------------------------------------------------
// API Functions
// --------------------------------------------------------------------------------------------------------------------

extern "C" {

/**
 *  Settings used for initializing the Steam Audio Wwise integration.
 */
typedef struct {
    /** Scaling factor to apply when converting from game engine units to Steam Audio units (which are in meters). */
    IPLfloat32  metersPerUnit;
} IPLWwiseSettings;

/**
 *  Returns the version of the Wwise integration being used.
 *
 *  \param  major   Major version number. For example, "1" in "1.2.3".
 *  \param  minor   Minor version number. For example, "2" in "1.2.3".
 *  \param  patch   Patch version number. For example, "3" in "1.2.3".
 */
AK_DLLEXPORT void AKSOUNDENGINE_CALL iplWwiseGetVersion(unsigned int* major, unsigned int* minor, unsigned int* patch);

/**
 *  Initializes the Wwise integration. This function must be called before creating any Steam Audio DSP effects.
 *
 *  \param  context     The Steam Audio context created by the game engine when initializing Steam Audio.
 *  \param  settings    Settings for initializing the Wwise integration. May be \c NULL, in which case default
 *                      values will be used.
 */
AK_DLLEXPORT void AKSOUNDENGINE_CALL iplWwiseInitialize(IPLContext context, IPLWwiseSettings* settings);

/**
 *  Shuts down the Wwise integration. This function must be called after all Steam Audio DSP effects have been
 *  destroyed.
 */
AK_DLLEXPORT void AKSOUNDENGINE_CALL iplWwiseTerminate();

/**
 *  Specifies the HRTF to use for spatialization in subsequent audio frames. This function must be called once during
 *  initialization, after \c iplWwiseInitialize. It should also be called whenever the game engine needs to change the
 *  HRTF.
 *
 *  \param  hrtf    The HRTF to use for spatialization.
 */
AK_DLLEXPORT void AKSOUNDENGINE_CALL iplWwiseSetHRTF(IPLHRTF hrtf);

/**
 *  Specifies the simulation settings used by the game engine for simulating direct and/or indirect sound propagation.
 *  This function must be called once during initialization, after \c iplWwiseInitialize.
 *
 *  \param  simulationSettings  The simulation settings used by the game engine.
 */
AK_DLLEXPORT void AKSOUNDENGINE_CALL iplWwiseSetSimulationSettings(IPLSimulationSettings simulationSettings);

/**
 *  Specifies the \c IPLSource object used by the game engine for simulating reverb. Typically, listener-centric reverb
 *  is simulated by creating an \c IPLSource object with the same position as the listener, and simulating reflections.
 *  To render this simulated reverb, call this function and pass it the \c IPLSource object used.
 *
 *  \param  reverbSource    The source object used by the game engine for simulating reverb.
 */
AK_DLLEXPORT void AKSOUNDENGINE_CALL iplWwiseSetReverbSource(IPLSource reverbSource);

/**
 * Specifies the \c IPLSource object used by the game engine for simulating occlusion, reflections, etc. for
 * the given Wwise game object (identified by its AkGameObjectID).
 * 
 * \param gameObjectID  The Wwise game object ID.
 * \param source        The Steam Audio source.
 */
AK_DLLEXPORT void AKSOUNDENGINE_CALL iplWwiseAddSource(AkGameObjectID gameObjectID, IPLSource source);

/**
 * Remove any \c IPLSource object associated the given Wwise game object ID. This should be called
 * when the game engine no longer needs to render occlusion, reflections, etc. for the given
 * game object.
 * 
 * \param gameObjectID  The Wwise game object ID.
 */
AK_DLLEXPORT void AKSOUNDENGINE_CALL iplWwiseRemoveSource(AkGameObjectID gameObjectID);

}
