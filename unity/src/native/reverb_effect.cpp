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
    SA_REVERB_PARAM_BYPASSDURINGINIT,
    SA_REVERB_NUM_PARAMS
};

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

    /** Whether the effect should emit silence or dry audio while initialization is underway. */
    bool bypassDuringInitialization;

    /** The default constructor initializes parameters to default values.
     */
    ReverbEffectState() :
        binaural{ false },
        type{ IPL_SIMTYPE_REALTIME },
        bypassDuringInitialization{ false },
        mInputFormat{},
        mOutputFormat{},
        mBinauralRenderer{ nullptr },
        mAudioEngineSettings{ nullptr },
        mEnvironmentalRenderer{ nullptr },
        mConvolutionEffect{ nullptr },
        mAmbisonicsPanningEffect{ nullptr },
        mAmbisonicsBinauralEffect{ nullptr },
        mIndirectEffectOutputBuffer{},
        mIndirectSpatializedOutputBuffer{},
        mUsedAmbisonicsPanningEffect{ false },
        mUsedAmbisonicsBinauralEffect{ false },
        mBypassedInPreviousFrame{ false }
    {}

    /** The destructor ensures that audio processing state is destroyed.
     */
    ~ReverbEffectState()
    {
        terminate();
    }

    /** Retrieves a parameter value and passes it to Unity. Returns true if the parameter index is valid.
     */
    bool getParameter(ReverbEffectParams index, float& value)
    {
        switch (index)
        {
        case SA_REVERB_PARAM_BINAURAL:
            value = (binaural) ? 1.0f : 0.0f;
            return true;
        case SA_REVERB_PARAM_SIMTYPE:
            value = static_cast<float>(static_cast<int>(type));
            return true;
        case SA_REVERB_PARAM_BYPASSDURINGINIT:
            value = (bypassDuringInitialization) ? 1.0f : 0.0f;
            return true;
        default:
            return false;
        }
    }

    /** Sets a parameter to a value provided by Unity. Returns true if the parameter index is valid.
     */
    bool setParameter(ReverbEffectParams index, float value)
    {
        switch (index)
        {
        case SA_REVERB_PARAM_BINAURAL:
            binaural = (value == 1.0f);
            return true;
        case SA_REVERB_PARAM_SIMTYPE:
            type = static_cast<IPLSimulationType>(static_cast<int>(value));
            return true;
        case SA_REVERB_PARAM_BYPASSDURINGINIT:
            bypassDuringInitialization = (value == 1.0f);
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

        // If the environment has recently been reset, release all everything that relates
        // to the environmental renderer.
        if (EnvironmentProxy::hasEnvironmentReset())
        {
            terminate();
            EnvironmentProxy::acknowledgeEnvironmentReset();
        }

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

        // Make sure the temporary buffer for storing the reverb has been created.
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
        if (mEnvironmentalRenderer && !mConvolutionEffect)
        {
            auto identifier = IPLBakedDataIdentifier{ 0, IPL_BAKEDDATATYPE_REVERB };
            if (gApi.iplCreateConvolutionEffect(mEnvironmentalRenderer->environmentalRenderer(), identifier, type,
                mInputFormat, mIndirectEffectOutputBuffer.format, &mConvolutionEffect) != IPL_STATUS_SUCCESS)
            {
                return false;
            }
        }

        // Make sure the Ambisonics panning effect has been created.
        if (mBinauralRenderer && mEnvironmentalRenderer && !mAmbisonicsPanningEffect)
        {
            if (gApi.iplCreateAmbisonicsPanningEffect(mBinauralRenderer, mIndirectEffectOutputBuffer.format, mOutputFormat,
                &mAmbisonicsPanningEffect) != IPL_STATUS_SUCCESS)
            {
                return false;
            }
        }

        // Make sure the Ambisonics binaural effect has been created.
        if (mBinauralRenderer && mEnvironmentalRenderer && !mAmbisonicsBinauralEffect)
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
        mAudioEngineSettings = nullptr;
        mEnvironmentalRenderer = nullptr;
    }

    /** Applies the Reverb effect to audio flowing through a Mixer Group.
     */
    void process(float* inBuffer, float* outBuffer, unsigned int numSamples, int inChannels, int outChannels,
        int samplingRate, int frameSize, unsigned int flags, UnityAudioSpatializerData* spatializerData)
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

        auto nothingToOutput = false;
        
        // Make sure that audio processing state has been initialized. If initialization fails, stop and emit silence.
        if (!initialize(samplingRate, frameSize, inputFormat, outputFormat) ||
            !mEnvironmentalRenderer || !mEnvironmentalRenderer->environmentalRenderer() ||
            !mConvolutionEffect || !mAmbisonicsPanningEffect || !mAmbisonicsBinauralEffect)
        {
            nothingToOutput = true;
        }

        if (nothingToOutput)
        {
            if (bypassDuringInitialization)
            {
                memcpy(outBuffer, inBuffer, outChannels * numSamples * sizeof(float));
                mBypassedInPreviousFrame = true;
            }

            return;
        }

        IPLVector3 listenerPosition, listenerUp, listenerAhead;
        if (spatializerData)
        {
            auto L = spatializerData->listenermatrix;

            auto listenerScaleSquared = 1.0f / (L[1] * L[1] + L[5] * L[5] + L[9] * L[9]);

            auto Lx = -listenerScaleSquared * (L[0] * L[12] + L[1] * L[13] + L[2] * L[14]);
            auto Ly = -listenerScaleSquared * (L[4] * L[12] + L[5] * L[13] + L[6] * L[14]);
            auto Lz = -listenerScaleSquared * (L[8] * L[12] + L[9] * L[13] + L[10] * L[14]);

            listenerPosition = convertVector(Lx, Ly, Lz);
            listenerUp = unitVector(convertVector(L[1], L[5], L[9]));
            listenerAhead = unitVector(convertVector(L[2], L[6], L[10]));
        }
        else
        {
            listenerPosition = mEnvironmentalRenderer->listenerPosition();
            listenerUp = mEnvironmentalRenderer->listenerUp();
            listenerAhead = mEnvironmentalRenderer->listenerAhead();
        }

        // Apply reverb to the input audio, resulting in an Ambisonics buffer containing the unspatialized reverb.
        IPLSource reverbSource = {};
        reverbSource.position = listenerPosition;
        reverbSource.ahead = listenerAhead;
        reverbSource.up = listenerAhead;
        reverbSource.directivity = IPLDirectivity{ 0.0f, 0.0f, nullptr, nullptr };
        reverbSource.distanceAttenuationModel = IPLDistanceAttenuationModel{IPL_DISTANCEATTENUATION_DEFAULT};
        reverbSource.airAbsorptionModel = IPLAirAbsorptionModel{IPL_AIRABSORPTION_DEFAULT};
        gApi.iplSetDryAudioForConvolutionEffect(mConvolutionEffect, reverbSource, inputAudio);
        gApi.iplGetWetAudioForConvolutionEffect(mConvolutionEffect, listenerPosition, listenerAhead,
            listenerUp, mIndirectEffectOutputBuffer);

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

            gApi.iplApplyAmbisonicsPanningEffect(mAmbisonicsPanningEffect, mBinauralRenderer, mIndirectEffectOutputBuffer,
                mIndirectSpatializedOutputBuffer);

            mUsedAmbisonicsPanningEffect = true;
        }

        // Add the spatialized reverb to the input audio, and place the result in the output buffer.
        for (auto i = 0u; i < outChannels * numSamples; ++i)
        {
            outputAudio.interleavedBuffer[i] = mIndirectSpatializedOutputBuffer.interleavedBuffer[i];
        }

        if (mBypassedInPreviousFrame)
        {
            crossfadeInputAndOutput(inBuffer, outChannels, numSamples, outBuffer);
            mBypassedInPreviousFrame = false;
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

    /** Did we emit dry audio in the previous frame? */
    bool mBypassedInPreviousFrame;
};

/** Callback that is called by Unity when the Reverb effect is created. This may be called in the editor when
 *  a Reverb effect is added to a Mixer Group, or when entering play mode. Therefore, it should only initialize
 *  user-controlled parameters.
 */
UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK createReverbEffect(UnityAudioEffectState* state)
{
    state->effectdata = new ReverbEffectState{};
    return UNITY_AUDIODSP_OK;
}

/** Callback that is called by Unity when the Reverb effect is destroyed. This may be called in the editor when
 *  the effect is removed from a Mixer Group, or just before entering play mode. It may not be called on exiting play
 *  mode, so audio processing state must be destroyed elsewhere.
 */
UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK releaseReverbEffect(UnityAudioEffectState* state)
{
    delete state->GetEffectData<ReverbEffectState>();
    return UNITY_AUDIODSP_OK;
}

/** Callback that is called by Unity to process frames of audio data. Apart from in play mode, this may also be called
 *  in the editor when no sounds are playing, so this condition should be detected, and actual processing or
 *  initialization should be avoided in this case.
 */
UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK processReverbEffect(UnityAudioEffectState* state, float* inBuffer,
    float* outBuffer, unsigned int numSamples, int inChannels, int outChannels)
{
    auto params = state->GetEffectData<ReverbEffectState>();
    params->process(inBuffer, outBuffer, numSamples, inChannels, outChannels, state->samplerate, state->dspbuffersize,
        state->flags, state->spatializerdata);
    return UNITY_AUDIODSP_OK;
}

/** Callback that is called by Unity when the Mixer Group inspector needs to update the value of some user-facing
 *  parameter.
 */
UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK setReverbEffectParam(UnityAudioEffectState* state, int index, 
    float value)
{
    auto params = state->GetEffectData<ReverbEffectState>();
    return params->setParameter(static_cast<ReverbEffectParams>(index), value) ?
        UNITY_AUDIODSP_OK : UNITY_AUDIODSP_ERR_UNSUPPORTED;
}

/** Callback that is called by Unity when the Mixer Group inspector needs to query the value of some user-facing
 *  parameter.
 */
UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK getReverbEffectParam(UnityAudioEffectState* state, int index,
    float* value, char*)
{
    auto params = state->GetEffectData<ReverbEffectState>();
    return params->getParameter(static_cast<ReverbEffectParams>(index), *value) ?
        UNITY_AUDIODSP_OK : UNITY_AUDIODSP_ERR_UNSUPPORTED;
}

/** Descriptors for each user-facing parameter of the Reverb effect. */
UnityAudioParameterDefinition gReverbEffectParams[] =
{
    { "Binaural", "", "Spatialize reverb using HRTF.", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f },
    { "Type", "", "Real-time or baked.", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f },
    { "BypassAtInit", "", "Bypass the effect during initialization.", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f }
};

/** Descriptor for the Reverb effect. */
UnityAudioEffectDefinition gReverbEffect
{
    sizeof(UnityAudioEffectDefinition), 
    sizeof(UnityAudioParameterDefinition),
    UNITY_AUDIO_PLUGIN_API_VERSION, 
    STEAMAUDIO_UNITY_VERSION,
    0, 
    SA_REVERB_NUM_PARAMS, 
    UnityAudioEffectDefinitionFlags_NeedsSpatializerData,
    "Steam Audio Reverb",
    createReverbEffect, 
    releaseReverbEffect, 
    nullptr, 
    processReverbEffect, 
    nullptr,
    gReverbEffectParams,
    setReverbEffectParam, 
    getReverbEffectParam, 
    nullptr
};