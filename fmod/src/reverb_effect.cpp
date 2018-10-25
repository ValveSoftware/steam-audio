//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

#include <vector>
#include "environment_proxy.h"
#include "auto_load_library.h"

/** Parameters that can be set by the user on a Reverb effect.
 */
enum ReverbEffectParams
{
    SA_REVERB_PARAM_BINAURAL,
    SA_REVERB_PARAM_SIMTYPE,
    SA_REVERB_NUM_PARAMS
};

FMOD_DSP_PARAMETER_DESC gReverbParamIndirectBinaural = { FMOD_DSP_PARAMETER_TYPE_BOOL, "IndirBinaural", "", "Spatialize reverb using HRTF." };
FMOD_DSP_PARAMETER_DESC gReverbParamSimulationType = { FMOD_DSP_PARAMETER_TYPE_INT, "IndirType", "", "Real-time or baked." };

FMOD_DSP_PARAMETER_DESC* gReverbEffectParams[] =
{
    &gReverbParamIndirectBinaural,
    &gReverbParamSimulationType
};

char* gSimulationTypeValues[] = { "Real-time", "Baked" };

void initReverbParamDescs()
{
    gReverbParamIndirectBinaural.booldesc = { false, nullptr };
    gReverbParamSimulationType.intdesc = { 0, 1, 0, false, gSimulationTypeValues };
}

/** A native audio mixer effect that applies listener-centric reverb to its input. This effect can be added to any
 *  Mixer Group, and it will add reverb to the input flowing through that Mixer Group.
 */
class ReverbEffectState
{
public:

    /** User-facing parameters.
     */

    /** Whether or not the reverb is rendered using binaural rendering. */
    bool binaural;

    /** Whether to use real-time simulation or baked data for reverb. */
    IPLSimulationType type;

    /** The default constructor initializes parameters to default values.
     */
    ReverbEffectState() :
        binaural{ false },
        type{ IPL_SIMTYPE_REALTIME },
        mInputFormat{},
        mOutputFormat{},
        mBinauralRenderer{ nullptr },
        mGlobalState{ nullptr },
        mSceneState{ nullptr },
        mConvolutionEffect{ nullptr },
        mAmbisonicsPanningEffect{ nullptr },
        mAmbisonicsBinauralEffect{ nullptr },
        mIndirectEffectOutputBuffer{},
        mIndirectSpatializedOutputBuffer{},
        mUsedAmbisonicsPanningEffect{ false },
        mUsedAmbisonicsBinauralEffect{ false }
    {}

    /** The destructor ensures that audio processing state is destroyed.
     */
    ~ReverbEffectState()
    {
        terminate();
    }

    /** Retrieves a parameter value and passes it to Unity. Returns true if the parameter index is valid.
     */
    bool getParameter(ReverbEffectParams index, 
                      bool& value)
    {
        switch (index)
        {
        case SA_REVERB_PARAM_BINAURAL:
            value = binaural;
            return true;
        default:
            return false;
        }
    }

    bool getParameter(ReverbEffectParams index, 
                      int& value)
    {
        switch (index)
        {
        case SA_REVERB_PARAM_SIMTYPE:
            value = type;
            return true;
        default:
            return false;
        }
    }

    /** Sets a parameter to a value provided by Unity. Returns true if the parameter index is valid.
     */
    bool setParameter(ReverbEffectParams index, 
                      bool value)
    {
        switch (index)
        {
        case SA_REVERB_PARAM_BINAURAL:
            binaural = value;
            return true;
        default:
            return false;
        }
    }

    bool setParameter(ReverbEffectParams index, 
                      int value)
    {
        switch (index)
        {
        case SA_REVERB_PARAM_SIMTYPE:
            type = static_cast<IPLSimulationType>(value);
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

        // Check to see if an environmental renderer has just been created.
        if (!mSceneState)
            mSceneState = SceneState::get();

        // Make sure the temporary buffer for storing the reverb has been created.
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

        // Make sure the temporary buffer for storing the spatialized reverb has been created.
        if (mIndirectSpatializedOutputBufferData.empty())
        {
            auto numChannels = mOutputFormat.numSpeakers;

            mIndirectSpatializedOutputBufferData.resize(numChannels * frameSize);

            mIndirectSpatializedOutputBuffer.format = mOutputFormat;
            mIndirectSpatializedOutputBuffer.numSamples = frameSize;
            mIndirectSpatializedOutputBuffer.interleavedBuffer = mIndirectSpatializedOutputBufferData.data();
        }

        // Make sure the convolution effect has been created.
        if (mSceneState && !mConvolutionEffect)
        {
            auto identifier = IPLBakedDataIdentifier{ 0, IPL_BAKEDDATATYPE_REVERB };
            if (gApi.iplCreateConvolutionEffect(mSceneState->environmentalRenderer(), identifier, type,
                mInputFormat, mIndirectEffectOutputBuffer.format, &mConvolutionEffect) != IPL_STATUS_SUCCESS)
            {
                return false;
            }
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
        gApi.iplDestroyAmbisonicsBinauralEffect(&mAmbisonicsBinauralEffect);
        gApi.iplDestroyAmbisonicsPanningEffect(&mAmbisonicsPanningEffect);
        gApi.iplDestroyConvolutionEffect(&mConvolutionEffect);

        mIndirectEffectOutputBufferChannels.clear();
        mIndirectEffectOutputBufferData.clear();
        mIndirectSpatializedOutputBufferData.clear();

        mBinauralRenderer = nullptr;
        mGlobalState = nullptr;
        mSceneState = nullptr;
    }

    /** Applies the Reverb effect to audio flowing through a Mixer Group.
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
            !mConvolutionEffect || !mAmbisonicsPanningEffect || !mAmbisonicsBinauralEffect)
        {
            return;
        }

        // Apply reverb to the input audio, resulting in an Ambisonics buffer containing the unspatialized reverb.
		IPLSource reverbSource = {};
		reverbSource.position = listenerPosition;
		reverbSource.ahead = listenerAhead;
		reverbSource.up = listenerAhead;
		reverbSource.directivity = IPLDirectivity{ 0.0f, 0.0f, nullptr, nullptr };
		gApi.iplSetDryAudioForConvolutionEffect(mConvolutionEffect, reverbSource, inputAudio);
        gApi.iplGetWetAudioForConvolutionEffect(mConvolutionEffect, listenerPosition, listenerAhead, listenerUp, 
                                                mIndirectEffectOutputBuffer);

        // Spatialize the reverb.
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

            gApi.iplApplyAmbisonicsPanningEffect(mAmbisonicsPanningEffect, mAmbisonicsBinauralEffect, 
				mIndirectEffectOutputBuffer, mIndirectSpatializedOutputBuffer);

            mUsedAmbisonicsPanningEffect = true;
        }

        // Add the spatialized reverb to the input audio, and place the result in the output buffer.
        for (auto i = 0u; i < outChannels * numSamples; ++i)
        {
            outputAudio.interleavedBuffer[i] = mIndirectSpatializedOutputBuffer.interleavedBuffer[i];
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
    
    /** Handle to the convolution effect used by this effect. */
    IPLhandle mConvolutionEffect;
    
    /** Handle to the Ambisonics panning effect used by this effect. */
    IPLhandle mAmbisonicsPanningEffect;
    
    /** Handle to the Ambisonics binaural effect used by this effect. */
    IPLhandle mAmbisonicsBinauralEffect;

    /** Contiguous, deinterleaved buffer for storing the reverb, before spatialization. */
    std::vector<float> mIndirectEffectOutputBufferData;
    
    /** Array of pointers to per-channel data in the above buffer. */
    std::vector<float*> mIndirectEffectOutputBufferChannels;
    
    /** Steam Audio buffer descriptor for the above buffer. */
    IPLAudioBuffer mIndirectEffectOutputBuffer;

    /** Interleaved buffer for storing the reverb, after spatialization. */
    std::vector<float> mIndirectSpatializedOutputBufferData;
    
    /** Steam Audio buffer descriptor for the above buffer. */
    IPLAudioBuffer mIndirectSpatializedOutputBuffer;

    /** Have we used the Ambisonics panning effect in the previous frame? */
    bool mUsedAmbisonicsPanningEffect;

    /** Have we used the Ambisonics binaural effect in the previous frame? */
    bool mUsedAmbisonicsBinauralEffect;
};

/** Callback that is called by Unity when the Reverb effect is created. This may be called in the editor when
 *  a Reverb effect is added to a Mixer Group, or when entering play mode. Therefore, it should only initialize
 *  user-controlled parameters.
 */
FMOD_RESULT F_CALL createReverbEffect(FMOD_DSP_STATE* state)
{
    state->plugindata = new ReverbEffectState{};
    return FMOD_OK;
}

/** Callback that is called by Unity when the Reverb effect is destroyed. This may be called in the editor when
 *  the effect is removed from a Mixer Group, or just before entering play mode. It may not be called on exiting play
 *  mode, so audio processing state must be destroyed elsewhere.
 */
FMOD_RESULT F_CALL releaseReverbEffect(FMOD_DSP_STATE* state)
{
    delete reinterpret_cast<ReverbEffectState*>(state->plugindata);
    return FMOD_OK;
}

/** Callback that is called by Unity to process frames of audio data. Apart from in play mode, this may also be called
 *  in the editor when no sounds are playing, so this condition should be detected, and actual processing or
 *  initialization should be avoided in this case.
 */
FMOD_RESULT F_CALL processReverbEffect(FMOD_DSP_STATE* state, 
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
        auto params = reinterpret_cast<ReverbEffectState*>(state->plugindata);

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

FMOD_RESULT F_CALL getReverbBool(FMOD_DSP_STATE* state, 
                                 int index, 
                                 FMOD_BOOL* value, 
                                 char*)
{
    auto* params = reinterpret_cast<ReverbEffectState*>(state->plugindata);
    auto boolValue = false;
    auto status = params->getParameter(static_cast<ReverbEffectParams>(index), boolValue);
    *value = boolValue;
    return status ? FMOD_OK : FMOD_ERR_INVALID_PARAM;
}

FMOD_RESULT F_CALL getReverbInt(FMOD_DSP_STATE* state, 
                                int index, 
                                int* value, 
                                char*)
{
    auto* params = reinterpret_cast<ReverbEffectState*>(state->plugindata);
    return params->getParameter(static_cast<ReverbEffectParams>(index), *value) ? FMOD_OK : FMOD_ERR_INVALID_PARAM;
}

FMOD_RESULT F_CALL setReverbBool(FMOD_DSP_STATE* state, 
                                 int index, 
                                 FMOD_BOOL value)
{
    auto* params = reinterpret_cast<ReverbEffectState*>(state->plugindata);
    return params->setParameter(static_cast<ReverbEffectParams>(index), (value != 0)) ? FMOD_OK : FMOD_ERR_INVALID_PARAM;
}

FMOD_RESULT F_CALL setReverbInt(FMOD_DSP_STATE* state, 
                                int index, 
                                int value)
{
    auto* params = reinterpret_cast<ReverbEffectState*>(state->plugindata);
    return params->setParameter(static_cast<ReverbEffectParams>(index), value) ? FMOD_OK : FMOD_ERR_INVALID_PARAM;
}

/** Descriptor for the Reverb effect. */
FMOD_DSP_DESCRIPTION gReverbEffect
{
    FMOD_PLUGIN_SDK_VERSION,
    "Steam Audio Reverb",
    STEAMAUDIO_FMOD_VERSION,
    1, 1,
    createReverbEffect,
    releaseReverbEffect,
    nullptr,
    nullptr,
    processReverbEffect,
    nullptr,
    SA_REVERB_NUM_PARAMS,
    gReverbEffectParams,
    nullptr,
    setReverbInt,
    setReverbBool,
    nullptr,
    nullptr,
    getReverbInt,
    getReverbBool,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr
};