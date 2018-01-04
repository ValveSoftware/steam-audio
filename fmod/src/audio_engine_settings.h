//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

#pragma once

#include <string.h>
#include <exception>
#include <memory>
#include <mutex>
#include <phonon.h>
#include "steamaudio_fmod.h"

/** Data shared by all effect instances created by the audio engine, across all scenes.
 */
class GlobalState
{
public:
    /** Initializes the audio engine global state. Must be called in the process callback of all effects before any
     *  other Steam Audio function is called. This function may be called repeatedly by different effects over
     *  different frames; initialization will be performed only the first time this function is called. This function
     *  must only be called from the audio thread.
     */
    GlobalState(const IPLRenderingSettings& renderingSettings, 
                const IPLAudioFormat& audioFormat);

    /** Destroys the binaural renderer.
     */
    ~GlobalState();

    /** Returns the Rendering Settings object that describes the settings used by the audio engine.
     */
    IPLRenderingSettings renderingSettings() const;

    /** Returns the mixer output format used by the audio engine. This format is used for all audio effects in the
     *  mixer graph, regardless of where they are in the graph.
     */
    IPLAudioFormat outputFormat() const;

    /** Returns the context object used by the audio engine.
     */
    IPLhandle context() const;

    /** Returns the binaural renderer used by the audio engine.
     */
    IPLhandle binauralRenderer() const;

    /** Returns the global Audio Engine Settings object.
     */
    static std::shared_ptr<GlobalState> get();

    /** Initializes a new Audio Engine Settings object.
     */
    static void create(const IPLRenderingSettings& renderingSettings, 
                       const IPLAudioFormat& audioFormat);

    /** Destroys any existing Audio Engine Settings object.
     */
    static void destroy();

private:
    /** Rendering Settings that describe the settings used by the audio engine. */
    IPLRenderingSettings mRenderingSettings;

    /** Mixer output format used by the audio engine. */
    IPLAudioFormat mOutputFormat;

    /** The context object. */
    IPLhandle mContext;

    /** The binaural renderer. */
    IPLhandle mBinauralRenderer;

    /** Mutex for preventing concurrent accesses to the audio engine settings. */
    static std::mutex sMutex;

    /** Pointer to the shared Audio Engine Settings object used by all effects. */
    static std::shared_ptr<GlobalState> sAudioEngineSettings;
};

extern "C"
{
    /** Mini-API wrapper around GlobalState::destroy.
     */
    F_EXPORT void F_CALL iplFmodResetAudioEngine();
}