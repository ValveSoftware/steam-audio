//
// Copyright 2018 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

#include <future>
#include <string>
#include <vector>
#include <math.h>
#include "environment_proxy.h"
#include "auto_load_library.h"

/** Parameters that can be set by the user on an Ambisonics Decoder effect.
*/
enum AmbisonicsDecoderEffectParams
{
    SA_AMBISONICSDECODER_PARAM_ENABLEBINAURAL,
    SA_AMBISONICSDECODER_PARAM_HRTFINDEX,
    SA_AMBISONICSDECODER_PARAM_OVERRIDEHRTFINDEX,
    SA_AMBISONICSDECODER_NUM_PARAMS
};

class AmbisonicDecoderState
{
public:

    /** User-facing parameters.
    */

    /** Flag to indicate if binaural rendering using HRTFs is applied to ambisonics audio. */
    bool enableBinaural;

    /** Flag to indicate whether custom HRTF should be used for ambisonics rendering. */
    bool overrideHRTFIndex;

    /** Index of custom HRTF to be used. */
    int hrtfIndex;

    AmbisonicDecoderState() :
        enableBinaural{ true },
        hrtfIndex{ 0 },
        overrideHRTFIndex{ false },
        mInputFormat{},
        mOutputFormat{},
        mBinauralRenderer{ nullptr },
        mAudioEngineSettings{ nullptr },
        mAmbisonicsRotator { nullptr },
        mAmbisonicsPanningEffect{ nullptr },
        mAmbisonicsBinauralEffect{ nullptr }
    {}

    /** The destructor ensures that audio processing state is destroyed.
    */
    ~AmbisonicDecoderState()
    {
        terminate();
    }

    /** Retrieves a parameter value and passes it to Unity. Returns true if the parameter index is valid.
    */
    bool getParameter(AmbisonicsDecoderEffectParams index, float& value)
    {
        switch (index)
        {
        case SA_AMBISONICSDECODER_PARAM_ENABLEBINAURAL:
            value = (enableBinaural) ? 1.0f : 0.0f;
            return true;
         case SA_AMBISONICSDECODER_PARAM_HRTFINDEX:
            value = static_cast<float>(hrtfIndex);
            return true;
        case SA_AMBISONICSDECODER_PARAM_OVERRIDEHRTFINDEX:
            value = (overrideHRTFIndex) ? 1.0f : 0.0f;
            return true;
         default:
            return false;
        }
    }

    /** Sets a parameter to a value provided by Unity. Returns true if the parameter index is valid.
    */
    bool setParameter(AmbisonicsDecoderEffectParams index, float value)
    {
        switch (index)
        {
        case SA_AMBISONICSDECODER_PARAM_ENABLEBINAURAL:
            enableBinaural = (value == 1.0f);
            return true;
        case SA_AMBISONICSDECODER_PARAM_HRTFINDEX:
            hrtfIndex = static_cast<int>(value);
            return true;
        case SA_AMBISONICSDECODER_PARAM_OVERRIDEHRTFINDEX:
            overrideHRTFIndex = (value == 1.0f);
            return true;
       default:
            return false;
        }
    }

    /** Attempts to initialize audio processing state. Returns true when it succeeds. Doesn't do anything if
    *  initialization has already happened once. This function should be called at the start of every frame to
    *  ensure that all necessary audio processing state has been initialized.
    */
    bool initialize(const int samplingRate, const int frameSize, const IPLAudioFormat inFormat,
        const IPLAudioFormat outFormat)
    {
        mInputFormat = inFormat;
        mOutputFormat = outFormat;

        // Make sure the audio engine global state has been initialized, and the binaural renderer has been created.
        if (!mAudioEngineSettings)
        {
            mAudioEngineSettings = AudioEngineSettings::get();
            if (!mAudioEngineSettings)
            {
                try
                {
                    IPLRenderingSettings renderingSettings{ samplingRate, frameSize, IPL_CONVOLUTIONTYPE_PHONON };
                    AudioEngineSettings::create(renderingSettings, outFormat);
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

        if (overrideHRTFIndex) {
            mBinauralRenderer = mAudioEngineSettings->binauralRenderer(hrtfIndex);
        }
        else {
            mBinauralRenderer = mAudioEngineSettings->binauralRenderer();
        }

        if (!mBinauralRenderer)
            return false;

        // Make sure the Ambisonics rotator has been created.
        if (!mAmbisonicsRotator)
        {
            if (gApi.iplCreateAmbisonicsRotator(mAudioEngineSettings->context(),
                inFormat.ambisonicsOrder, &mAmbisonicsRotator) != IPL_STATUS_SUCCESS)
            {
                return false;
            }
        }

        // Make sure the Ambisonics panning effect has been created.
        if (!mAmbisonicsPanningEffect)
        {
            if (gApi.iplCreateAmbisonicsPanningEffect(mBinauralRenderer, inFormat, outFormat,
                &mAmbisonicsPanningEffect) != IPL_STATUS_SUCCESS)
            {
                return false;
            }
        }

        // Make sure the Ambisonics binaural effect has been created.
        if (!mAmbisonicsBinauralEffect)
        {
            if (gApi.iplCreateAmbisonicsBinauralEffect(mBinauralRenderer, inFormat, outFormat,
                &mAmbisonicsBinauralEffect) != IPL_STATUS_SUCCESS)
            {
                return false;
            }
        }

        // Make sure internal buffers are created for Ambisonics calculation.
        if (mBinauralAmbisonicsBuffer.empty())
        {
            mBinauralAmbisonicsBuffer.resize(frameSize * outFormat.numSpeakers);
        }

        if (mAmbisonicsSN3DDeinterleavedChannels.empty())
        {
            mAmbisonicsSN3DDeinterleavedData.resize(frameSize * inFormat.numSpeakers);

            mAmbisonicsSN3DDeinterleavedChannels.resize(inFormat.numSpeakers);
            for (auto i = 0; i < inFormat.numSpeakers; ++i)
                mAmbisonicsSN3DDeinterleavedChannels[i] = &mAmbisonicsSN3DDeinterleavedData[i * frameSize];
        }

        if (mAmbisonicsN3DDeinterleavedChannels.empty())
        {
            mAmbisonicsN3DDeinterleavedData.resize(frameSize * inFormat.numSpeakers);

            mAmbisonicsN3DDeinterleavedChannels.resize(inFormat.numSpeakers);
            for (auto i = 0; i < inFormat.numSpeakers; ++i)
                mAmbisonicsN3DDeinterleavedChannels[i] = &mAmbisonicsN3DDeinterleavedData[i * frameSize];
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
        gApi.iplDestroyAmbisonicsRotator(&mAmbisonicsRotator);

        mBinauralAmbisonicsBuffer.clear();
        mAmbisonicsN3DDeinterleavedData.clear();
        mAmbisonicsN3DDeinterleavedChannels.clear();
        mAmbisonicsSN3DDeinterleavedData.clear();
        mAmbisonicsSN3DDeinterleavedChannels.clear();

        mBinauralRenderer = nullptr;
        mAudioEngineSettings = nullptr;
    }

    void remapAmbisonicsToOutChannels(const unsigned int numSamples, const int ambisonicsChannels, const int outChannels,
                                        float* outBuffer)
    {
        // Remapping is needed to output audio in a way that makes sense to Unity. Following is a note from Unity's
        // documentation (https://docs.unity3d.com/Manual/AmbisonicAudio.html) on this issue: 
        // UnityAudioAmbisonicData struct that is passed into ambisonic decoders, 
        // which is very similar to the UnityAudioSpatializerData struct that is passed into spatializers, but 
        // there is a new ambisonicOutChannels integer that has been added. This field will be set to the 
        // DefaultSpeakerMode’s channel count. Ambisonic decoders are placed very early in the audio pipeline, where 
        // we are running at the clip’s channel count, so ambisonicOutChannels tells the plugin how many of the 
        // output channels will actually be used. If we are playing back a first order ambisonic audio clip(4 channels) 
        // and our speaker mode is stereo (2 channels), an ambisonic decoder’s process callback will be passed in 4 for 
        // the in and out channel count. The ambisonicOutChannels field will be set to 2. In this common scenario, 
        // the plugin should output its spatialized data to the first 2 channels and zero out the other 2 channels.

        int minChannels = std::min(ambisonicsChannels, outChannels);
        for (unsigned int sample = 0; sample < numSamples; ++sample)
        {
            for (int channel = 0; channel < minChannels; ++channel)
            {
                outBuffer[sample * outChannels + channel] = mBinauralAmbisonicsBuffer[sample * ambisonicsChannels + channel];
            }
        }
    }

    /** Applies the Spatialize effect to audio flowing through an Audio Source.
    */
    void process(float* inBuffer, float* outBuffer, unsigned int numSamples, int inChannels, int outChannels,
        int samplingRate, int frameSize, unsigned int flags, UnityAudioAmbisonicData* ambisonicData)
    {
        // Assume that the number of input and output channels are the same.
        assert(inChannels == outChannels);

        // Start by clearing the output buffer.
        memset(outBuffer, 0, outChannels * frameSize * sizeof(float));

        // Unity can call the process callback even when not in play mode. In this case, destroy any audio processing
        // state that may exist, and emit silence.
        if (!(flags & UnityAudioEffectStateFlags_IsPlaying))
        {
            terminate();
            return;
        }

        // Input audio format.
        IPLAudioFormat inputAmbisonicsFormat;
        inputAmbisonicsFormat.channelLayoutType = IPL_CHANNELLAYOUTTYPE_AMBISONICS;
        inputAmbisonicsFormat.ambisonicsOrdering = IPL_AMBISONICSORDERING_ACN;
        inputAmbisonicsFormat.ambisonicsNormalization = IPL_AMBISONICSNORMALIZATION_SN3D;
        inputAmbisonicsFormat.ambisonicsOrder = static_cast<int>(sqrtf(static_cast<float>(inChannels))) - 1;
        inputAmbisonicsFormat.numSpeakers = inChannels;
        inputAmbisonicsFormat.channelOrder = IPL_CHANNELORDER_INTERLEAVED;

        // Binaural audio output format.
        auto binauralAmbisonicsFormat = audioFormatForNumChannels(ambisonicData->ambisonicOutChannels);

        // Intermediate format 1: convert input ambisonics audio from interleaved SN3D to deinterleaved SN3D
        IPLAudioFormat deinterleavedInputFormat = inputAmbisonicsFormat;
        deinterleavedInputFormat.channelOrder = IPL_CHANNELORDER_DEINTERLEAVED;

        // Intermediate format 2: convert deinterleaved SN3D to deinterleaved N3D for Steam Audio processing.
        auto deinterleavedN3DFormat = deinterleavedInputFormat;
        deinterleavedN3DFormat.ambisonicsNormalization = IPL_AMBISONICSNORMALIZATION_N3D;

        // Make sure that audio processing state has been initialized. If initialization fails, stop and emit silence.
        if (!initialize(samplingRate, frameSize, deinterleavedN3DFormat, binauralAmbisonicsFormat)
            || mBinauralAmbisonicsBuffer.empty())
        {
            return;
        }

        auto inputAmbisonicsAudio = IPLAudioBuffer{ inputAmbisonicsFormat, static_cast<IPLint32>(numSamples), 
                                                    inBuffer, nullptr };

        auto deinterleavedInputAudio = IPLAudioBuffer{ deinterleavedInputFormat, static_cast<IPLint32>(numSamples),
            nullptr, mAmbisonicsSN3DDeinterleavedChannels.data() };
        auto deinterleavedN3DAudio = IPLAudioBuffer{ deinterleavedN3DFormat, static_cast<IPLint32>(numSamples),
            nullptr, mAmbisonicsN3DDeinterleavedChannels.data() };

        // Convert input ambisonics audio from interleaved SN3D to deinterleaved SN3D
        gApi.iplDeinterleaveAudioBuffer(inputAmbisonicsAudio, deinterleavedInputAudio);

        // Convert deinterleaved SND3D to deinterleaved N3D for Steam Audio processing.
        gApi.iplConvertAudioBufferFormat(deinterleavedInputAudio, deinterleavedN3DAudio);

        auto S = ambisonicData->sourcematrix;
        auto sourceAhead = unitVector(IPLVector3{S[8], S[9], S[10]});
        auto sourceUp = unitVector(IPLVector3{S[4], S[5], S[6]});

        auto L = ambisonicData->listenermatrix; //Matrix that transforms source into the local space of the listener.
        auto ambisonicAheadX = L[0] * sourceAhead.x + L[4] * sourceAhead.y + L[8] * sourceAhead.z;
        auto ambisonicAheadY = L[1] * sourceAhead.x + L[5] * sourceAhead.y + L[9] * sourceAhead.z;
        auto ambisonicAheadZ = L[2] * sourceAhead.x + L[6] * sourceAhead.y + L[10] * sourceAhead.z;
        auto ambisonicUpX = L[0] * sourceUp.x + L[4] * sourceUp.y + L[8] * sourceUp.z;
        auto ambisonicUpY = L[1] * sourceUp.x + L[5] * sourceUp.y + L[9] * sourceUp.z;
        auto ambisonicUpZ = L[2] * sourceUp.x + L[6] * sourceUp.y + L[10] * sourceUp.z;

        auto ambisonicAhead = unitVector(convertVector(ambisonicAheadX, ambisonicAheadY, ambisonicAheadZ));
        auto ambisonicUp = unitVector(convertVector(ambisonicUpX, ambisonicUpY, ambisonicUpZ));
        auto ambisonicRight = unitVector(cross(ambisonicAhead, ambisonicUp));

        IPLVector3 listenerAhead;
        listenerAhead.x = -ambisonicRight.z;
        listenerAhead.y = -ambisonicUp.z;
        listenerAhead.z = ambisonicAhead.z;
        listenerAhead = unitVector(listenerAhead);

        IPLVector3 listenerUp;
        listenerUp.x = ambisonicRight.y;
        listenerUp.y = ambisonicUp.y;
        listenerUp.z = -ambisonicAhead.y;
        listenerUp = unitVector(listenerUp);

        gApi.iplSetAmbisonicsRotation(mAmbisonicsRotator, listenerAhead, listenerUp);
        gApi.iplRotateAmbisonicsAudioBuffer(mAmbisonicsRotator, deinterleavedN3DAudio, deinterleavedN3DAudio);

        // Apply Ambisonics binaural or panning effect.
        auto binauralAmbisonicsAudio = IPLAudioBuffer{ binauralAmbisonicsFormat, static_cast<IPLint32>(numSamples), 
            mBinauralAmbisonicsBuffer.data(), nullptr };

        if (enableBinaural)
        {
            gApi.iplApplyAmbisonicsBinauralEffect(mAmbisonicsBinauralEffect, mBinauralRenderer, deinterleavedN3DAudio,
                binauralAmbisonicsAudio);
        }
        else
        {
            gApi.iplApplyAmbisonicsPanningEffect(mAmbisonicsPanningEffect, mBinauralRenderer, deinterleavedN3DAudio,
                binauralAmbisonicsAudio);
        }

        remapAmbisonicsToOutChannels(numSamples, ambisonicData->ambisonicOutChannels, outChannels, outBuffer);

        // Normalize the output so that an Ambisonics order 0 clip with peak magnitude 1 is comparable to an
        // unspatialized mono clip of peak magnitude 1.
        const float kPi = 3.141592f; 
        float scalar = 1.0f / sqrtf(4.0f * kPi);

        for (unsigned int i = 0; i < numSamples * outChannels; ++i)
        {
            outBuffer[i] *= scalar;
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

    /** Holds the binaural Ambisonics audio data. */
    std::vector<float> mBinauralAmbisonicsBuffer;

    /** Contiguous, deinterleaved buffer for storing the internal buffers for ambisonics rendering. */
    std::vector<float*> mAmbisonicsSN3DDeinterleavedChannels;
    std::vector<float> mAmbisonicsSN3DDeinterleavedData;

    std::vector<float*> mAmbisonicsN3DDeinterleavedChannels;
    std::vector<float> mAmbisonicsN3DDeinterleavedData;

    /** Array of pointers to per-channel data in the above buffer. */
    std::vector<float*> mIndirectEffectOutputBufferChannels;



    /** Handle to the Ambisonics panning effect used by this effect. */
    IPLhandle mAmbisonicsPanningEffect;

    /** Handle to the Ambisonics binaural effect used by this effect. */
    IPLhandle mAmbisonicsBinauralEffect;

    /** Handle Ambisonics rotation based on listener orientation. */
    IPLhandle mAmbisonicsRotator;
};

/** Callback that is called by Unity when the Ambisonics Decoder effect is created. This may be called in the editor when
*  an Ambisonics source is entering play mode. Therefore, it should only initialize
*  user-controlled parameters.
*/
UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK createAmbisonicDecoder(UnityAudioEffectState* state)
{
    state->effectdata = new AmbisonicDecoderState{};
    return UNITY_AUDIODSP_OK;
}

/** Callback that is called by Unity when the Ambisonics Decoder effect is destroyed. This may be called in the editor when
*  an Ambisonics source is entering play mode. It may not be called on exiting play
*  mode, so audio processing state must be destroyed elsewhere.
*/
UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK releaseAmbisonicDecoder(UnityAudioEffectState* state)
{
    delete state->GetEffectData<AmbisonicDecoderState>();
    return UNITY_AUDIODSP_OK;
}

/** Callback that is called by Unity to process frames of audio data. Apart from in play mode, this may also be called
*  in the editor when no sounds are playing, so this condition should be detected, and actual processing or
*  initialization should be avoided in this case.
*/
UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK processAmbisonicDecoder(UnityAudioEffectState* state, float* inBuffer,
    float* outBuffer, unsigned int numSamples, int inChannels, int outChannels)
{
    auto params = state->GetEffectData<AmbisonicDecoderState>();
    params->process(inBuffer, outBuffer, numSamples, inChannels, outChannels, state->samplerate, state->dspbuffersize,
        state->flags, state->ambisonicdata);
    return UNITY_AUDIODSP_OK;
}

/** Callback that is called by Unity when the inspector needs to update the value of some user-facing parameter.
*/
UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK setAmbisonicDecoderEffectParam(UnityAudioEffectState* state, int index,
    float value)
{
    auto params = state->GetEffectData<AmbisonicDecoderState>();
    return params->setParameter(static_cast<AmbisonicsDecoderEffectParams>(index), value) ?
        UNITY_AUDIODSP_OK : UNITY_AUDIODSP_ERR_UNSUPPORTED;
}

/** Callback that is called by Unity when the inspector needs to query the value of some user-facing parameter.
*/
UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK getAmbisonicDecoderEffectParam(UnityAudioEffectState* state, int index,
    float* value, char*)
{
    auto params = state->GetEffectData<AmbisonicDecoderState>();
    return params->getParameter(static_cast<AmbisonicsDecoderEffectParams>(index), *value) ?
        UNITY_AUDIODSP_OK : UNITY_AUDIODSP_ERR_UNSUPPORTED;
}

/** Descriptors for each user-facing parameter of the Ambisonics Decoder effect. */
UnityAudioParameterDefinition gAmbisonicsDecoderEffectParams[] =
{
    { "EnableBinaural", "", "Spatialize ambisonics audio using HRTF.", 0.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { "HRTFIndex", "", "Index of the HRTF to use.", 0.0f, static_cast<float>(std::numeric_limits<int>::max()), 0.0f, 1.0f, 1.0f },
    { "OverrideHRTF", "", "Whether to override HRTF index per-source.", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f },
};

/** Descriptor for the Spatializer effect. */
UnityAudioEffectDefinition gAmbisonicDecoder
{
    sizeof(UnityAudioEffectDefinition),
    sizeof(UnityAudioParameterDefinition),
    UNITY_AUDIO_PLUGIN_API_VERSION,
    STEAMAUDIO_UNITY_VERSION,
    0,
    SA_AMBISONICSDECODER_NUM_PARAMS,
    UnityAudioEffectDefinitionFlags_IsAmbisonicDecoder,
    "Steam Audio Ambisonics",
    createAmbisonicDecoder,
    releaseAmbisonicDecoder,
    nullptr,
    processAmbisonicDecoder,
    nullptr,
    gAmbisonicsDecoderEffectParams,
    setAmbisonicDecoderEffectParam,
    getAmbisonicDecoderEffectParam,
    nullptr
};
