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
        mAudioEngineSettings{ nullptr },
        mEnvironmentalRenderer{ nullptr },
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
    bool getParameter(MixEffectParams index, float& value)
    {
        switch (index)
        {
        case SA_MIX_PARAM_BINAURAL:
            value = (binaural) ? 1.0f : 0.0f;
            return true;
        default:
            return false;
        }
    }

    /** Sets a parameter to a value provided by Unity. Returns true if the parameter index is valid.
     */
    bool setParameter(MixEffectParams index, float value)
    {
        switch (index)
        {
        case SA_MIX_PARAM_BINAURAL:
            binaural = (value == 1.0f);
            return true;
        default:
            return false;
        }
    }

    /** Attempts to initialize audio processing state. Returns true when it succeeds. Doesn't do anything if
     *  initialization has already happened once. This function should be called at the start of every frame to
     *  ensure that all necessary audio processing state has been initialized.
     */
    bool initialize(const int samplingRate, const int frameSize, const IPLAudioFormat inputFormat,
        const IPLAudioFormat outputFormat)
    {
        mInputFormat = inputFormat;
        mOutputFormat = outputFormat;

        // Make sure the audio engine global state has been initialized.
        if (!mAudioEngineSettings)
        {
            mAudioEngineSettings = AudioEngineSettings::get();
            if (!mAudioEngineSettings)
            {
                try
                {
                    IPLRenderingSettings renderingSettings{ samplingRate, frameSize, IPL_CONVOLUTIONTYPE_PHONON };
                    AudioEngineSettings::create(renderingSettings, outputFormat);
                }
                catch (std::exception)
                {
                    return false;
                }

                mAudioEngineSettings = AudioEngineSettings::get();
                if (!mAudioEngineSettings)
                    return false;
            }
        }

        mBinauralRenderer = mAudioEngineSettings->binauralRenderer();
        if (!mBinauralRenderer)
            return false;

        // Check to see if an environmental renderer has just been created.
        if (!mEnvironmentalRenderer)
            mEnvironmentalRenderer = EnvironmentProxy::get();

        // Adding a Mixer Return effect to any Mixer Group indicates that all Steam Audio Sources will be using
        // accelerated mixing.
        if (mEnvironmentalRenderer && !mEnvironmentalRenderer->isUsingAcceleratedMixing())
            mEnvironmentalRenderer->setUsingAcceleratedMixing(true);

        // Make sure the temporary buffer for storing the mixed indirect sound has been created.
        if (mEnvironmentalRenderer && mIndirectEffectOutputBufferData.empty())
        {
            auto ambisonicsOrder = mEnvironmentalRenderer->simulationSettings().ambisonicsOrder;
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
        if (mEnvironmentalRenderer)
            mEnvironmentalRenderer->setUsingAcceleratedMixing(false);

        gApi.iplDestroyAmbisonicsBinauralEffect(&mAmbisonicsBinauralEffect);
        gApi.iplDestroyAmbisonicsPanningEffect(&mAmbisonicsPanningEffect);

        mIndirectSpatializedOutputBufferData.clear();
        mIndirectEffectOutputBufferChannels.clear();
        mIndirectEffectOutputBufferData.clear();

        mBinauralRenderer = nullptr;
        mAudioEngineSettings = nullptr;
        mEnvironmentalRenderer = nullptr;
    }

    /** Applies the Mixer Return effect to audio flowing through a Mixer Group.
     */
    void process(float* inBuffer, float* outBuffer, unsigned int numSamples, int inChannels, int outChannels,
        int samplingRate, int frameSize, unsigned int flags)
    {
        // Assume that the number of input and output channels are the same.
        assert(inChannels == outChannels);

        // Start by clearing the output buffer.
        memset(outBuffer, 0, outChannels * numSamples * sizeof(float));

        // Unity can call the process callback even when not in play mode. In this case, destroy any audio processing
        // state that may exist, and emit silence.
        if (!(flags & UnityAudioEffectStateFlags_IsPlaying))
        {
            terminate();
            return;
        }

        // Prepare the input and output buffers.
        auto inputFormat = audioFormatForNumChannels(inChannels);
        auto outputFormat = audioFormatForNumChannels(outChannels);
        auto inputAudio = IPLAudioBuffer{ inputFormat, static_cast<IPLint32>(numSamples), inBuffer, nullptr };
        auto outputAudio = IPLAudioBuffer{ outputFormat, static_cast<IPLint32>(numSamples), outBuffer, nullptr };

        // Make sure that audio processing state has been initialized. If initialization fails, stop and emit silence.
        if (!initialize(samplingRate, frameSize, inputFormat, outputFormat) ||
            !mEnvironmentalRenderer || !mEnvironmentalRenderer->environmentalRenderer() || 
            !mAmbisonicsPanningEffect || !mAmbisonicsBinauralEffect)
        {
            return;
        }

        // Retrieve an Ambisonics buffer containing mixed indirect audio.
        gApi.iplGetMixedEnvironmentalAudio(mEnvironmentalRenderer->environmentalRenderer(),
            mEnvironmentalRenderer->listenerPosition(), mEnvironmentalRenderer->listenerAhead(), 
            mEnvironmentalRenderer->listenerUp(), mIndirectEffectOutputBuffer);

        // Spatialize the mixed indirect audio.
        if (binaural)
        {
            if (mAmbisonicsPanningEffect && mUsedAmbisonicsPanningEffect)
            {
                gApi.iplFlushAmbisonicsPanningEffect(mAmbisonicsPanningEffect);
                mUsedAmbisonicsPanningEffect = false;
            }

            gApi.iplApplyAmbisonicsBinauralEffect(mAmbisonicsBinauralEffect, mIndirectEffectOutputBuffer,
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

            gApi.iplApplyAmbisonicsPanningEffect(mAmbisonicsPanningEffect, mIndirectEffectOutputBuffer,
                mIndirectSpatializedOutputBuffer);

            mUsedAmbisonicsPanningEffect = true;
        }

        // Add the spatialized indirect audio to the input audio, and place the result in the output buffer.
        for (auto i = 0; i < outChannels * numSamples; ++i)
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
    std::shared_ptr<AudioEngineSettings> mAudioEngineSettings;

    /** An object that contains the environmental renderer for the current scene. */
    std::shared_ptr<EnvironmentProxy> mEnvironmentalRenderer;

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
UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK createMixEffect(UnityAudioEffectState* state)
{
    state->effectdata = new MixEffectState{};
    return UNITY_AUDIODSP_OK;
}

/** Callback that is called by Unity when the Mixer Return effect is destroyed. This may be called in the editor when
 *  the effect is removed from a Mixer Group, or just before entering play mode. It may not be called on exiting play
 *  mode, so audio processing state must be destroyed elsewhere.
 */
UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK releaseMixEffect(UnityAudioEffectState* state)
{
    delete state->GetEffectData<MixEffectState>();
    return UNITY_AUDIODSP_OK;
}

/** Callback that is called by Unity to process frames of audio data. Apart from in play mode, this may also be called
 *  in the editor when no sounds are playing, so this condition should be detected, and actual processing or
 *  initialization should be avoided in this case.
 */
UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK processMixEffect(UnityAudioEffectState* state, float* inBuffer,
    float* outBuffer, unsigned int numSamples, int inChannels, int outChannels)
{
    auto params = state->GetEffectData<MixEffectState>();
    params->process(inBuffer, outBuffer, numSamples, inChannels, outChannels, state->samplerate, state->dspbuffersize,
        state->flags);
    return UNITY_AUDIODSP_OK;
}

/** Callback that is called by Unity when the Mixer Group inspector needs to update the value of some user-facing
 *  parameter.
 */
UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK setMixEffectParam(UnityAudioEffectState* state, int index, float value)
{
    auto params = state->GetEffectData<MixEffectState>();
    return params->setParameter(static_cast<MixEffectParams>(index), value) ?
        UNITY_AUDIODSP_OK : UNITY_AUDIODSP_ERR_UNSUPPORTED;
}

/** Callback that is called by Unity when the Mixer Group inspector needs to query the value of some user-facing
 *  parameter.
 */
UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK getMixEffectParam(UnityAudioEffectState* state, int index, float* value,
    char*)
{
    auto params = state->GetEffectData<MixEffectState>();
    return params->getParameter(static_cast<MixEffectParams>(index), *value) ? 
        UNITY_AUDIODSP_OK : UNITY_AUDIODSP_ERR_UNSUPPORTED;
}

/** Descriptors for each user-facing parameter of the Mixer Return effect. */
UnityAudioParameterDefinition gMixEffectParams[] = 
{
    { "Binaural", "", "Spatialize mixed audio using HRTF.", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f }
};

/** Descriptor for the Mixer Return effect. */
UnityAudioEffectDefinition gMixEffect 
{
    sizeof(UnityAudioEffectDefinition), 
    sizeof(UnityAudioParameterDefinition),
    UNITY_AUDIO_PLUGIN_API_VERSION, 
    STEAM_AUDIO_PLUGIN_VERSION,
    0, 
    SA_MIX_NUM_PARAMS, 
    0,
    "Steam Audio Mixer Return",
    createMixEffect, 
    releaseMixEffect, 
    nullptr, 
    processMixEffect, 
    nullptr,
    gMixEffectParams,
    setMixEffectParam, 
    getMixEffectParam, 
    nullptr
};