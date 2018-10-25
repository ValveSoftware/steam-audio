//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

#include <vector>
#include "environment_proxy.h"
#include "auto_load_library.h"

/** Parameters that can be set by the user on a Mixer Return effect.
 */
enum MixEffectParams
{
    SA_MIX_PARAM_BINAURAL,
    SA_MIX_NUM_PARAMS
};

FMOD_DSP_PARAMETER_DESC gMixParamIndirectBinaural = { FMOD_DSP_PARAMETER_TYPE_BOOL, "IndirBinaural", "", "Spatialize indirect sound using HRTF." };

FMOD_DSP_PARAMETER_DESC* gMixEffectParams[] =
{
    &gMixParamIndirectBinaural
};

void initMixParamDescs()
{
    gMixParamIndirectBinaural.booldesc = { false, nullptr };
}

/** A native audio mixer effect that inserts the output of accelerated mixing into the audio pipeline. This effect can
 *  be added on any Mixer Group, and it will add mixed indirect sound to the input flowing through that Mixer Group.
 */
class MixEffectState
{
public:

    /** User-facing parameters.
     */

    /** Whether or not the indirect sound (which is in Ambisonics format) is decoded using binaural rendering. */
    bool binaural;

    /** The default constructor initializes parameters to default values.
     */
    MixEffectState() :
        binaural{ false },
        mInputFormat{},
        mOutputFormat{},
        mBinauralRenderer{ nullptr },
        mGlobalState{ nullptr },
        mSceneState{ nullptr },
        mAmbisonicsPanningEffect{ nullptr },
        mAmbisonicsBinauralEffect{ nullptr },
        mIndirectEffectOutputBuffer{},
        mIndirectSpatializedOutputBuffer{},
        mUsedAmbisonicsPanningEffect{ false },
        mUsedAmbisonicsBinauralEffect{ false }
    {}

    /** The destructor ensures that audio processing state is destroyed.
     */
    ~MixEffectState()
    {
        terminate();
    }

    /** Retrieves a parameter value and passes it to Unity. Returns true if the parameter index is valid.
     */
    bool getParameter(MixEffectParams index, 
                      bool& value)
    {
        switch (index)
        {
        case SA_MIX_PARAM_BINAURAL:
            value = binaural;
            return true;
        default:
            return false;
        }
    }

    /** Sets a parameter to a value provided by Unity. Returns true if the parameter index is valid.
     */
    bool setParameter(MixEffectParams index, 
                      bool value)
    {
        switch (index)
        {
        case SA_MIX_PARAM_BINAURAL:
            binaural = value;
            return true;
        default:
            return false;
        }
    }

    /** Attempts to initialize audio processing state. Returns true when it succeeds. Doesn't do anything if
     *  initialization has already happened once. This function should be called at the start of every frame to
     *  ensure that all necessary audio processing state has been initialized.
     */
    bool initialize(const int samplingRate, 
                    const int frameSize, 
                    const IPLAudioFormat inputFormat,
                    const IPLAudioFormat outputFormat)
    {
        mInputFormat = inputFormat;
        mOutputFormat = outputFormat;

        // Make sure the audio engine global state has been initialized.
        if (!mGlobalState)
        {
            mGlobalState = AudioEngineSettings::get();
            if (!mGlobalState)
                return false;
        }

        mBinauralRenderer = mGlobalState->binauralRenderer();
        if (!mBinauralRenderer)
            return false;

        // If the environment has recently been reset, release all everything that relates
        // to the environmental renderer.
        if (SceneState::hasEnvironmentReset())
        {
            gApi.iplDestroyAmbisonicsBinauralEffect(&mAmbisonicsBinauralEffect);
            gApi.iplDestroyAmbisonicsPanningEffect(&mAmbisonicsPanningEffect);

            mIndirectSpatializedOutputBufferData.clear();
            mIndirectEffectOutputBufferChannels.clear();
            mIndirectEffectOutputBufferData.clear();

            mSceneState = nullptr;

            SceneState::acknowledgeEnvironmentReset();
        }

        // Check to see if an environmental renderer has just been created.
        if (!mSceneState)
            mSceneState = SceneState::get();

        // Adding a Mixer Return effect to any Mixer Group indicates that all Steam Audio Sources will be using
        // accelerated mixing.
        if (mSceneState && !mSceneState->isUsingAcceleratedMixing())
            mSceneState->setUsingAcceleratedMixing(true);

        // Make sure the temporary buffer for storing the mixed indirect sound has been created.
        if (mSceneState && mIndirectEffectOutputBufferData.empty())
        {
            auto ambisonicsOrder = mSceneState->simulationSettings().ambisonicsOrder;
            auto numChannels = (ambisonicsOrder + 1) * (ambisonicsOrder + 1);

            mIndirectEffectOutputBufferData.resize(numChannels * frameSize);

            mIndirectEffectOutputBufferChannels.resize(numChannels);
            for (auto i = 0; i < numChannels; ++i)
                mIndirectEffectOutputBufferChannels[i] = &mIndirectEffectOutputBufferData[i * frameSize];

            mIndirectEffectOutputBuffer.format.channelLayoutType = IPL_CHANNELLAYOUTTYPE_AMBISONICS;
            mIndirectEffectOutputBuffer.format.ambisonicsOrder = ambisonicsOrder;
            mIndirectEffectOutputBuffer.format.ambisonicsOrdering = IPL_AMBISONICSORDERING_ACN;
            mIndirectEffectOutputBuffer.format.ambisonicsNormalization = IPL_AMBISONICSNORMALIZATION_N3D;
            mIndirectEffectOutputBuffer.format.channelOrder = IPL_CHANNELORDER_DEINTERLEAVED;
            mIndirectEffectOutputBuffer.numSamples = frameSize;
            mIndirectEffectOutputBuffer.deinterleavedBuffer = mIndirectEffectOutputBufferChannels.data();
        }

        // Make sure the temporary buffer for storing the spatialized indirect sound has been created.
        if (mIndirectSpatializedOutputBufferData.empty())
        {
            auto numChannels = mOutputFormat.numSpeakers;

            mIndirectSpatializedOutputBufferData.resize(numChannels * frameSize);

            mIndirectSpatializedOutputBuffer.format = mOutputFormat;
            mIndirectSpatializedOutputBuffer.numSamples = frameSize;
            mIndirectSpatializedOutputBuffer.interleavedBuffer = mIndirectSpatializedOutputBufferData.data();
        }

        // Make sure the Ambisonics panning effect has been created.
        if (mBinauralRenderer && !mAmbisonicsPanningEffect)
        {
            if (gApi.iplCreateAmbisonicsPanningEffect(mBinauralRenderer, mIndirectEffectOutputBuffer.format, mOutputFormat,
                &mAmbisonicsPanningEffect) != IPL_STATUS_SUCCESS)
            {
                return false;
            }
        }

        // Make sure the Ambisonics binaural effect has been created.
        if (mBinauralRenderer && !mAmbisonicsBinauralEffect)
        {
            if (gApi.iplCreateAmbisonicsBinauralEffect(mBinauralRenderer, mIndirectEffectOutputBuffer.format, mOutputFormat,
                &mAmbisonicsBinauralEffect) != IPL_STATUS_SUCCESS)
            {
                return false;
            }
        }

        return true;
    }

    /** Destroys all audio processing state. Doesn't do anything if all audio processing state has already been
     *  destroyed. Should be called as soon as the audio processing state is no longer needed.
     */
    void terminate()
    {
        if (mSceneState)
            mSceneState->setUsingAcceleratedMixing(false);

        gApi.iplDestroyAmbisonicsBinauralEffect(&mAmbisonicsBinauralEffect);
        gApi.iplDestroyAmbisonicsPanningEffect(&mAmbisonicsPanningEffect);

        mIndirectSpatializedOutputBufferData.clear();
        mIndirectEffectOutputBufferChannels.clear();
        mIndirectEffectOutputBufferData.clear();

        mBinauralRenderer = nullptr;
        mGlobalState = nullptr;
        mSceneState = nullptr;
    }

    /** Applies the Mixer Return effect to audio flowing through a Mixer Group.
     */
    void process(float* inBuffer, 
                 float* outBuffer, 
                 unsigned int numSamples, 
                 int inChannels, 
                 int outChannels,
                 int samplingRate, 
                 int frameSize,
                 IPLVector3 listenerPosition,
                 IPLVector3 listenerAhead,
                 IPLVector3 listenerUp)
    {
        // Start by clearing the output buffer.
        memset(outBuffer, 0, outChannels * numSamples * sizeof(float));

        // Prepare the input and output buffers.
        auto inputFormat = audioFormatForNumChannels(inChannels);
        auto outputFormat = audioFormatForNumChannels(outChannels);
        auto inputAudio = IPLAudioBuffer{ inputFormat, static_cast<IPLint32>(numSamples), inBuffer, nullptr };
        auto outputAudio = IPLAudioBuffer{ outputFormat, static_cast<IPLint32>(numSamples), outBuffer, nullptr };

        // Make sure that audio processing state has been initialized. If initialization fails, stop and emit silence.
        if (!initialize(samplingRate, frameSize, inputFormat, outputFormat) ||
            !mSceneState || !mSceneState->environmentalRenderer() || 
            !mAmbisonicsPanningEffect || !mAmbisonicsBinauralEffect)
        {
            return;
        }

        // Retrieve an Ambisonics buffer containing mixed indirect audio.
        gApi.iplGetMixedEnvironmentalAudio(mSceneState->environmentalRenderer(), listenerPosition, listenerAhead,
                                           listenerUp, mIndirectEffectOutputBuffer);

        // Spatialize the mixed indirect audio.
        if (binaural)
        {
            if (mAmbisonicsPanningEffect && mUsedAmbisonicsPanningEffect)
            {
                gApi.iplFlushAmbisonicsPanningEffect(mAmbisonicsPanningEffect);
                mUsedAmbisonicsPanningEffect = false;
            }

            gApi.iplApplyAmbisonicsBinauralEffect(mAmbisonicsBinauralEffect, mBinauralRenderer, mIndirectEffectOutputBuffer,
                mIndirectSpatializedOutputBuffer);

            mUsedAmbisonicsBinauralEffect = true;
        }
        else
        {
            if (mAmbisonicsBinauralEffect && mUsedAmbisonicsBinauralEffect)
            {
                gApi.iplFlushAmbisonicsBinauralEffect(mAmbisonicsBinauralEffect);
                mUsedAmbisonicsBinauralEffect = false;
            }

            gApi.iplApplyAmbisonicsPanningEffect(mAmbisonicsPanningEffect, mAmbisonicsBinauralEffect, mIndirectEffectOutputBuffer,
                mIndirectSpatializedOutputBuffer);

            mUsedAmbisonicsPanningEffect = true;
        }

        // Add the spatialized indirect audio to the input audio, and place the result in the output buffer.
        for (auto i = 0u; i < outChannels * numSamples; ++i)
        {
            outputAudio.interleavedBuffer[i] = inputAudio.interleavedBuffer[i] +
                mIndirectSpatializedOutputBuffer.interleavedBuffer[i];
        }
    }

private:

    /** Audio processing state.
     */

    /** Format of the audio buffer provided as input to this effect. */
    IPLAudioFormat mInputFormat;

    /** Format of the audio buffer generated as output by this effect. */
    IPLAudioFormat mOutputFormat;

    /** Handle to the binaural renderer used by the audio engine. */
    IPLhandle mBinauralRenderer;

    /** An object that contains the rendering settings and binaural renderer used globally. */
    std::shared_ptr<AudioEngineSettings> mGlobalState;

    /** An object that contains the environmental renderer for the current scene. */
    std::shared_ptr<SceneState> mSceneState;

    /** Handle to the Ambisonics panning effect used by this effect. */
    IPLhandle mAmbisonicsPanningEffect;

    /** Handle to the Ambisonics binaural effect used by this effect. */
    IPLhandle mAmbisonicsBinauralEffect;

    /** Contiguous, deinterleaved buffer for storing the mixed indirect audio, before spatialization. */
    std::vector<float> mIndirectEffectOutputBufferData;

    /** Array of pointers to per-channel data in the above buffer. */
    std::vector<float*> mIndirectEffectOutputBufferChannels;

    /** Steam Audio buffer descriptor for the above buffer. */
    IPLAudioBuffer mIndirectEffectOutputBuffer;

    /** Interleaved buffer for storing the mixed indirect audio, after spatialization. */
    std::vector<float> mIndirectSpatializedOutputBufferData;

    /** Steam Audio buffer descriptor for the above buffer. */
    IPLAudioBuffer mIndirectSpatializedOutputBuffer;

    /** Have we used the Ambisonics panning effect in the previous frame? */
    bool mUsedAmbisonicsPanningEffect;

    /** Have we used the Ambisonics binaural effect in the previous frame? */
    bool mUsedAmbisonicsBinauralEffect;
};

/** Callback that is called by Unity when the Mixer Return effect is created. This may be called in the editor when
 *  a Mixer Return effect is added to a Mixer Group, or when entering play mode. Therefore, it should only initialize
 *  user-controlled parameters.
 */
FMOD_RESULT F_CALL createMixEffect(FMOD_DSP_STATE* state)
{
    state->plugindata = new MixEffectState{};
    return FMOD_OK;
}

/** Callback that is called by Unity when the Mixer Return effect is destroyed. This may be called in the editor when
 *  the effect is removed from a Mixer Group, or just before entering play mode. It may not be called on exiting play
 *  mode, so audio processing state must be destroyed elsewhere.
 */
FMOD_RESULT F_CALL releaseMixEffect(FMOD_DSP_STATE* state)
{
    delete reinterpret_cast<MixEffectState*>(state->plugindata);
    return FMOD_OK;
}

/** Callback that is called by Unity to process frames of audio data. Apart from in play mode, this may also be called
 *  in the editor when no sounds are playing, so this condition should be detected, and actual processing or
 *  initialization should be avoided in this case.
 */
FMOD_RESULT F_CALL processMixEffect(FMOD_DSP_STATE* state, 
                                    unsigned int length,
                                    const FMOD_DSP_BUFFER_ARRAY* inputBuffers, 
                                    FMOD_DSP_BUFFER_ARRAY* outputBuffers, 
                                    FMOD_BOOL inputsIdle,
                                    FMOD_DSP_PROCESS_OPERATION operation)
{
    if (operation == FMOD_DSP_PROCESS_QUERY)
    {
        if (inputsIdle)
            return FMOD_ERR_DSP_DONTPROCESS;
    }
    else if (operation == FMOD_DSP_PROCESS_PERFORM)
    {
        auto params = reinterpret_cast<MixEffectState*>(state->plugindata);

        auto samplingRate = 0;
        auto frameSize = 0u;
        state->functions->getsamplerate(state, &samplingRate);
        state->functions->getblocksize(state, &frameSize);

        auto numListeners = 1;
        FMOD_3D_ATTRIBUTES listener;
        state->functions->getlistenerattributes(state, &numListeners, &listener);

        auto listenerPosition = convertVector(listener.position.x, listener.position.y, listener.position.z);
        auto listenerAhead = convertVector(listener.forward.x, listener.forward.y, listener.forward.z);
        auto listenerUp = convertVector(listener.up.x, listener.up.y, listener.up.z);

        params->process(inputBuffers->buffers[0], outputBuffers->buffers[0], length, 
                        inputBuffers->buffernumchannels[0], outputBuffers->buffernumchannels[0], samplingRate, 
                        frameSize, listenerPosition, listenerAhead, listenerUp);
    }

    return FMOD_OK;
}

/** Callback that is called by Unity when the Mixer Group inspector needs to update the value of some user-facing
 *  parameter.
 */
FMOD_RESULT F_CALL setMixBool(FMOD_DSP_STATE* state, 
                              int index, 
                              FMOD_BOOL value)
{
    auto* params = reinterpret_cast<MixEffectState*>(state->plugindata);
    return params->setParameter(static_cast<MixEffectParams>(index), (value != 0)) ? FMOD_OK : FMOD_ERR_INVALID_PARAM;
}

/** Callback that is called by Unity when the Mixer Group inspector needs to query the value of some user-facing
 *  parameter.
 */
FMOD_RESULT F_CALL getMixBool(FMOD_DSP_STATE* state, 
                              int index, 
                              FMOD_BOOL* value, 
                              char*)
{
    auto* params = reinterpret_cast<MixEffectState*>(state->plugindata);
    auto boolValue = false;
    auto status = params->getParameter(static_cast<MixEffectParams>(index), boolValue);
    *value = boolValue;
    return status ? FMOD_OK : FMOD_ERR_INVALID_PARAM;
}

/** Descriptor for the Mixer Return effect. */
FMOD_DSP_DESCRIPTION gMixEffect 
{
    FMOD_PLUGIN_SDK_VERSION,
    "Steam Audio Mixer Return",
    STEAMAUDIO_FMOD_VERSION,
    1, 1,
    createMixEffect,
    releaseMixEffect,
    nullptr,
    nullptr,
    processMixEffect,
    nullptr,
    SA_MIX_NUM_PARAMS,
    gMixEffectParams,
    nullptr,
    nullptr,
    setMixBool,
    nullptr,
    nullptr,
    nullptr,
    getMixBool,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr
};