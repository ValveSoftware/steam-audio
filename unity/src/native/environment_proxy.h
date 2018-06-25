//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

#pragma once

#include <future>
#include <memory>
#include "audio_engine_settings.h"

/** A global proxy object that receives data from the game engine. This object is controlled by the game engine
 *  using a mini-API created for just this purpose.
 */
class EnvironmentProxy
{
public:
    /** Default constructor. */
    EnvironmentProxy(const IPLSimulationSettings& simulationSettings, const IPLhandle environment,
        const IPLConvolutionType convolutionType);

    /** Destroys the Environmental Renderer if created. */
    ~EnvironmentProxy();

    /** Returns the Simulation Settings object used for the current scene.
     */
    IPLSimulationSettings simulationSettings() const;
    
    /** Returns the Environment specified by the game engine.
     */
    IPLhandle environment() const;

    /** Returns an Environmental Renderer object that uses the Environment specified by the game engine.
     */
    IPLhandle environmentalRenderer();

    /** Returns the convolution engine used for the current scene.
     */
    IPLConvolutionType convolutionType() const;

    /** Returns whether accelerated mixing is being used.
     */
    bool isUsingAcceleratedMixing() const;

    /** Enables or disables accelerated mixing.
     */
    void setUsingAcceleratedMixing(const bool usingAcceleratedMixing);

    /** Returns the current listener position.
     */
    IPLVector3 listenerPosition() const;

    /** Returns the current listener forward vector.
     */
    IPLVector3 listenerAhead() const;

    /** Returns the current listener up vector.
     */
    IPLVector3 listenerUp() const;

    /** Sets the listener coordinate system for the current scene.
     */
    void setListener(const IPLVector3& position, const IPLVector3& ahead, const IPLVector3& up);

    /** Sets a new Environment object. This Environment object is owned by the game engine, and is tied to the
     *  lifetime of a scene. After this function is called, all subsequently created Effect objects will use the
     *  scene data contained in this Environment object for any simulation. If this function is called when an
     *  Environment object has already been specified, the old Environment will no longer be used when creating new
     *  Effect objects, but existing Effect objects will continue to use the old Environment.
     */
    static void setEnvironment(const IPLSimulationSettings& simulationSettings, const IPLhandle environment,
        const IPLConvolutionType convolutionType);

    /** Resets the Environment object to NULL. This essentially says that any subsequently created Effect objects will
     *  work with an empty environment, in which occlusion/transmission, reflection, etc. simulations cannot be
     *  performed. This function should be called when your app is shutting down to ensure that any memory leak
     *  detection software does not incorrectly report a memory leak. If your app is asynchronously loading a scene
     *  while a scene is already running, you must call this function before starting the asynchronous load, to ensure
     *  that any Effect objects that are created when the new scene loads do not end up using the Environment object
     *  from the old scene.
     */
    static void resetEnvironment();
    
    static void setListenerGlobal(const IPLVector3& position, const IPLVector3& ahead, const IPLVector3& up);

    /** Queries whether the environment has recently been reset.
     */
    static bool hasEnvironmentReset();

    /** Notifies that the recent environment reset has been processed.
     */
    static void acknowledgeEnvironmentReset();

    /** Returns the Environment Proxy object for the current scene.
     */
    static std::shared_ptr<EnvironmentProxy> get();

private:
    /** The Simulation Settings used for the current scene. */
    IPLSimulationSettings mSimulationSettings;

    /** The Environment object used for the current scene. */
    IPLhandle mEnvironment;
    mutable IPLhandle mEnvironmentCopy;

    /** The convolution engine to use for the current scene. */
    IPLConvolutionType mConvolutionType;

    /** The Environmental Renderer object created using the Environment object for the current scene. */
    IPLhandle mEnvironmentalRenderer;

    std::future<IPLhandle> mEnvironmentalRendererFuture;

    /** Whether we're using accelerated mixing for the current scene. */
    bool mUsingAcceleratedMixing;

    /** The listener coordinate system for the current scene. */
    IPLVector3 mListenerPosition;
    IPLVector3 mListenerAhead;
    IPLVector3 mListenerUp;
    
    /** Mutex used to prevent concurrent access to this object from the game engine and audio engine. */
    static std::mutex sMutex;

    /** Pointer to the Environment Proxy for the current scene. */
    static std::shared_ptr<EnvironmentProxy> sEnvironmentProxy;

    /** Indicates whether the environment has recently been reset. */
    static bool sEnvironmentHasReset;
};

extern "C"
{
    /** Mini-API wrapper around EnvironmentProxy::setEnvironment().
     */
    UNITY_AUDIODSP_EXPORT_API void UNITY_AUDIODSP_CALLBACK iplUnitySetEnvironment(
        IPLSimulationSettings simulationSettings, IPLhandle environment, IPLConvolutionType convolutionType);

    /** Mini-API wrapper around EnvironmentProxy::resetEnvironment().
     */
    UNITY_AUDIODSP_EXPORT_API void UNITY_AUDIODSP_CALLBACK iplUnityResetEnvironment();

    /** Mini-API wrapper around EnvironmentProxy::setListener().
     */
    UNITY_AUDIODSP_EXPORT_API void UNITY_AUDIODSP_CALLBACK iplUnitySetListener(IPLVector3 position, IPLVector3 ahead,
        IPLVector3 up);
}
