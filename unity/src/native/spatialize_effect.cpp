//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

#include <future>
#include <string>
#include <vector>
#include "environment_proxy.h"
#include "auto_load_library.h"

/** Parameters that can be set by the user on a Spatializer effect.
 */
enum SpatializeEffectParams
{
    SA_SPATIALIZE_PARAM_DIRECTBINAURAL,
    SA_SPATIALIZE_PARAM_HRTFINTERPOLATION,
    SA_SPATIALIZE_PARAM_DISTANCEATTENUATION,
    SA_SPATIALIZE_PARAM_AIRABSORPTION,
    SA_SPATIALIZE_PARAM_OCCLUSIONMODE,
    SA_SPATIALIZE_PARAM_OCCLUSIONMETHOD,
    SA_SPATIALIZE_PARAM_SOURCERADIUS,
    SA_SPATIALIZE_PARAM_DIRECTLEVEL,
    SA_SPATIALIZE_PARAM_INDIRECT,
    SA_SPATIALIZE_PARAM_INDIRECTBINAURAL,
    SA_SPATIALIZE_PARAM_INDIRECTLEVEL,
    SA_SPATIALIZE_PARAM_SIMTYPE,
    SA_SPATIALIZE_PARAM_STATICLISTENER,
    SA_SPATIALIZE_PARAM_NAME,
    SA_SPATIALIZE_NUM_PARAMS
};

/** Callback that records the distance attenuation applied by Unity.
 */
UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK recordUnityDistanceAttenuation(UnityAudioEffectState* state,
    float distanceIn, float attenuationIn, float* attenuationOut);

/** A native audio spatializer effect that applies binaural rendering, direct path attenuation, occlusion, and
 *  source-centric sound propagation to its input.
 */
class SpatializeEffectState
{
public:

    /** User-facing parameters.
     */

    /** Whether or not to apply binaural rendering to direct sound. */
    bool directBinaural;
    
    /** The type of interpolation to use when applying binaural rendering to direct sound. */
    IPLHrtfInterpolation hrtfInterpolation;
    
    /** Whether or not to apply distance attenuation to direct sound. */
    bool distanceAttenuation;
    
    /** Whether or not to apply frequency-dependent air absorption to direct sound. */
    bool airAbsorption;
    
    /** What to do when the direct sound path is occluded. */
    IPLDirectOcclusionMode occlusionMode;
    
    /** How to check for occlusion along the direct sound path. */
    IPLDirectOcclusionMethod occlusionMethod;
    
    /** Source radius to use for volumetric occlusion tests. */
    float sourceRadius;
    
    /** Relative level of spatialized direct sound. */
    float directLevel;
    
    /** Whether or not to apply source-centric sound propagation. */
    bool indirect;
    
    /** Whether or not to apply binaural rendering to indirect sound. */
    bool indirectBinaural;
    
    /** Relative level of spatialized indirect sound. */
    float indirectLevel;
    
    /** Whether to use real-time simulation or baked data to model indirect sound. */
    IPLSimulationType indirectType;
    
    /** Unique identifier of the static source or static listener node whose baked data is to be used for rendering
        indirect sound. Ignored if using real-time simulation. */
    std::string name;
    
    /** Whether or not the baked data we're using corresponds to a static listener node. Ignored if using real-time
        simulation. */
    bool usesStaticListener;

    /** The default constructor initializes parameters to default values.
     */
    SpatializeEffectState() :
        directBinaural{ true },
        hrtfInterpolation{ IPL_HRTFINTERPOLATION_NEAREST },
        distanceAttenuation{ false },
        airAbsorption{ false },
        occlusionMode{ IPL_DIRECTOCCLUSION_NONE },
        occlusionMethod{ IPL_DIRECTOCCLUSION_RAYCAST },
        sourceRadius{ 1.0f },
        directLevel{ 1.0f },
        indirect{ false },
        indirectBinaural{ false },
        indirectLevel{ 1.0f },
        indirectType{ IPL_SIMTYPE_REALTIME },
        name{},
        usesStaticListener{ false },
        mInputFormat{},
        mOutputFormat{},
        mBinauralRenderer{ nullptr },
        mPanningEffect{ nullptr },
        mBinauralEffect{ nullptr },
        mAudioEngineSettings{ nullptr },
        mEnvironmentalRenderer{ nullptr },
        mDirectEffect{ nullptr },
        mIndirectEffect{ nullptr },
        mAmbisonicsPanningEffect{ nullptr },
        mAmbisonicsBinauralEffect{ nullptr },
        mDirectEffectOutputBuffer{},
        mUsedConvolutionEffect{ false },
        mUsedAmbisonicsPanningEffect{ false },
        mUsedAmbisonicsBinauralEffect{ false },
        mPreviousDirectMixLevel{ 0.0f },
        mPreviousIndirectMixLevel{ 0.0f }
    {}

    /** The destructor ensures that audio processing state is destroyed.
     */
    ~SpatializeEffectState()
    {
        terminate();
    }

    /** Retrieves a parameter value and passes it to Unity. Returns true if the parameter index is valid.
     */
    bool getParameter(SpatializeEffectParams index, float& value)
    {
        switch (index)
        {
        case SA_SPATIALIZE_PARAM_DIRECTBINAURAL:
            value = (directBinaural) ? 1.0f : 0.0f;
            return true;
        case SA_SPATIALIZE_PARAM_HRTFINTERPOLATION:
            value = static_cast<float>(static_cast<int>(hrtfInterpolation));
            return true;
        case SA_SPATIALIZE_PARAM_DISTANCEATTENUATION:
            value = (distanceAttenuation) ? 1.0f : 0.0f;
            return true;
        case SA_SPATIALIZE_PARAM_AIRABSORPTION:
            value = (airAbsorption) ? 1.0f : 0.0f;
            return true;
        case SA_SPATIALIZE_PARAM_OCCLUSIONMODE:
            value = static_cast<float>(static_cast<int>(occlusionMode));
            return true;
        case SA_SPATIALIZE_PARAM_OCCLUSIONMETHOD:
            value = static_cast<float>(static_cast<int>(occlusionMethod));
            return true;
        case SA_SPATIALIZE_PARAM_SOURCERADIUS:
            value = sourceRadius;
            return true;
        case SA_SPATIALIZE_PARAM_DIRECTLEVEL:
            value = directLevel;
            return true;
        case SA_SPATIALIZE_PARAM_INDIRECT:
            value = (indirect) ? 1.0f : 0.0f;
            return true;
        case SA_SPATIALIZE_PARAM_INDIRECTBINAURAL:
            value = (indirectBinaural) ? 1.0f : 0.0f;
            return true;
        case SA_SPATIALIZE_PARAM_INDIRECTLEVEL:
            value = indirectLevel;
            return true;
        case SA_SPATIALIZE_PARAM_SIMTYPE:
            value = static_cast<float>(static_cast<int>(indirectType));
            return true;
        case SA_SPATIALIZE_PARAM_STATICLISTENER:
            value = (usesStaticListener) ? 1.0f : 0.0f;
            return true;
        case SA_SPATIALIZE_PARAM_NAME:
            // Querying the name parameter is not supported.
            return false;
        default:
            return false;
        }
    }

    /** Sets a parameter to a value provided by Unity. Returns true if the parameter index is valid.
     */
    bool setParameter(SpatializeEffectParams index, float value)
    {
        switch (index)
        {
        case SA_SPATIALIZE_PARAM_DIRECTBINAURAL:
            directBinaural = (value == 1.0f);
            return true;
        case SA_SPATIALIZE_PARAM_HRTFINTERPOLATION:
            hrtfInterpolation = static_cast<IPLHrtfInterpolation>(static_cast<int>(value));
            return true;
        case SA_SPATIALIZE_PARAM_DISTANCEATTENUATION:
            distanceAttenuation = (value == 1.0f);
            return true;
        case SA_SPATIALIZE_PARAM_AIRABSORPTION:
            airAbsorption = (value == 1.0f);
            return true;
        case SA_SPATIALIZE_PARAM_OCCLUSIONMODE:
            occlusionMode = static_cast<IPLDirectOcclusionMode>(static_cast<int>(value));
            return true;
        case SA_SPATIALIZE_PARAM_OCCLUSIONMETHOD:
            occlusionMethod = static_cast<IPLDirectOcclusionMethod>(static_cast<int>(value));
            return true;
        case SA_SPATIALIZE_PARAM_SOURCERADIUS:
            sourceRadius = value;
            return true;
        case SA_SPATIALIZE_PARAM_DIRECTLEVEL:
            directLevel = value;
            return true;
        case SA_SPATIALIZE_PARAM_INDIRECT:
            indirect = (value == 1.0f);
            return true;
        case SA_SPATIALIZE_PARAM_INDIRECTBINAURAL:
            indirectBinaural = (value == 1.0f);
            return true;
        case SA_SPATIALIZE_PARAM_INDIRECTLEVEL:
            indirectLevel = value;
            return true;
        case SA_SPATIALIZE_PARAM_SIMTYPE:
            indirectType = static_cast<IPLSimulationType>(static_cast<int>(value));
            return true;
        case SA_SPATIALIZE_PARAM_STATICLISTENER:
            usesStaticListener = (value == 1.0f);
            return true;
        case SA_SPATIALIZE_PARAM_NAME:
            name = std::to_string(*reinterpret_cast<int*>(&value));
            if (usesStaticListener)
                name = "__staticlistener__" + name;
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

        mBinauralRenderer = mAudioEngineSettings->binauralRenderer();
        if (!mBinauralRenderer)
            return false;

        // Check to see if an environmental renderer has just been created.
        if (!mEnvironmentalRenderer)
            mEnvironmentalRenderer = EnvironmentProxy::get();

        // Makre sure the temporary buffer for storing the output of the direct sound effect has been created.
        if (mDirectEffectOutputBufferData.empty())
        {
            mDirectEffectOutputBufferData.resize(frameSize);

            mDirectEffectOutputBufferChannels.resize(1);
            mDirectEffectOutputBufferChannels[0] = mDirectEffectOutputBufferData.data();

            mDirectEffectOutputBuffer.format.channelLayoutType = IPL_CHANNELLAYOUTTYPE_SPEAKERS;
            mDirectEffectOutputBuffer.format.channelLayout = IPL_CHANNELLAYOUT_MONO;
            mDirectEffectOutputBuffer.format.channelOrder = IPL_CHANNELORDER_DEINTERLEAVED;
            mDirectEffectOutputBuffer.numSamples = frameSize;
            mDirectEffectOutputBuffer.deinterleavedBuffer = mDirectEffectOutputBufferChannels.data();
        }

        // Make sure the temporary buffer for storing the spatialized direct sound has been created.
        if (mDirectSpatializedOutputBufferData.empty())
        {
            auto numChannels = mOutputFormat.numSpeakers;

            mDirectSpatializedOutputBufferData.resize(numChannels * frameSize);

            mDirectSpatializedOutputBuffer.format = mOutputFormat;
            mDirectSpatializedOutputBuffer.numSamples = frameSize;
            mDirectSpatializedOutputBuffer.interleavedBuffer = mDirectSpatializedOutputBufferData.data();
        }

        // Make sure the temporary buffer for storing the indirect sound has been created.
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

        // Make sure the panning effect has been created.
        if (mBinauralRenderer && !mPanningEffect)
        {
            if (gApi.iplCreatePanningEffect(mBinauralRenderer, mDirectEffectOutputBuffer.format, mOutputFormat,
                &mPanningEffect) != IPL_STATUS_SUCCESS)
            {
                return false;
            }
        }

        // Make sure the binaural effect has been created.
        if (mBinauralRenderer && !mBinauralEffect)
        {
            if (gApi.iplCreateBinauralEffect(mBinauralRenderer, mDirectEffectOutputBuffer.format, mOutputFormat,
                &mBinauralEffect) != IPL_STATUS_SUCCESS)
            {
                return false;
            }
        }

        // Make sure the direct sound effect has been created.
        if (mEnvironmentalRenderer && !mDirectEffect)
        {
            if (gApi.iplCreateDirectSoundEffect(mEnvironmentalRenderer->environmentalRenderer(), mInputFormat,
                mDirectEffectOutputBuffer.format, &mDirectEffect) != IPL_STATUS_SUCCESS)
            {
                return false;
            }
        }

        // Make sure the convolution effect has been created.
        if (mEnvironmentalRenderer && indirect && !mIndirectEffect)
        {
            if (gApi.iplCreateConvolutionEffect(mEnvironmentalRenderer->environmentalRenderer(),
                const_cast<char*>(name.c_str()), indirectType, mInputFormat, mIndirectEffectOutputBuffer.format,
                &mIndirectEffect) != IPL_STATUS_SUCCESS)
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
        gApi.iplDestroyConvolutionEffect(&mIndirectEffect);
        gApi.iplDestroyDirectSoundEffect(&mDirectEffect);
        gApi.iplDestroyBinauralEffect(&mBinauralEffect);
        gApi.iplDestroyPanningEffect(&mPanningEffect);

        mDirectEffectOutputBufferChannels.clear();
        mDirectEffectOutputBufferData.clear();
        mDirectSpatializedOutputBufferData.clear();
        mIndirectEffectOutputBufferChannels.clear();
        mIndirectEffectOutputBufferData.clear();
        mIndirectSpatializedOutputBufferData.clear();

        mBinauralRenderer = nullptr;
        mAudioEngineSettings = nullptr;
        mEnvironmentalRenderer = nullptr;
    }

    /** Applies the Spatialize effect to audio flowing through an Audio Source.
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

        // Make sure that audio processing state has been initialized. If initialization fails, stop and emit silence.
        if (!initialize(samplingRate, frameSize, inputFormat, outputFormat))
            return;

        if (!mPanningEffect || !mBinauralEffect)
            return;

        if (occlusionMode != IPL_DIRECTOCCLUSION_NONE)
        {
            if (!mEnvironmentalRenderer || !mEnvironmentalRenderer->environmentalRenderer() || !mDirectEffect)
                return;
        }

        if (indirect)
        {
            if (!mEnvironmentalRenderer || !mEnvironmentalRenderer->environmentalRenderer() || !mIndirectEffect ||
                !mAmbisonicsPanningEffect || !mAmbisonicsBinauralEffect)
            {
                return;
            }
        }

        // Unity provides the world-space position of the source and the listener's transform matrix.
        auto S = spatializerData->sourcematrix;
        auto L = spatializerData->listenermatrix;
        auto sourcePosition = convertVector(S[12], S[13], S[14]);
        auto directionX = L[0] * S[12] + L[4] * S[13] + L[8] * S[14] + L[12];
        auto directionY = L[1] * S[12] + L[5] * S[13] + L[9] * S[14] + L[13];
        auto directionZ = L[2] * S[12] + L[6] * S[13] + L[10] * S[14] + L[14];
        auto direction = convertVector(directionX, directionY, directionZ);

        auto Lx = -(L[0] * L[12] + L[1] * L[13] + L[2] * L[14]);
        auto Ly = -(L[4] * L[12] + L[5] * L[13] + L[6] * L[14]);
        auto Lz = -(L[8] * L[12] + L[9] * L[13] + L[10] * L[14]);

        auto listenerPosition = convertVector(Lx, Ly, Lz);
        auto listenerUp = convertVector(L[1], L[5], L[9]);
        auto listenerAhead = convertVector(L[2], L[6], L[10]);

        if (mEnvironmentalRenderer != nullptr && mEnvironmentalRenderer->environmentalRenderer() != nullptr)
        {
            // Calculate the direct path parameters.
            auto directPath = gApi.iplGetDirectSoundPath(mEnvironmentalRenderer->environment(),
                listenerPosition, listenerAhead, listenerUp, sourcePosition, sourceRadius, occlusionMode, 
                occlusionMethod);

            // Apply direct sound modeling to the input audio, resulting in a mono buffer of audio.
            auto directOptions = IPLDirectSoundEffectOptions
            {
                distanceAttenuation ? IPL_TRUE : IPL_FALSE,
                airAbsorption ? IPL_TRUE : IPL_FALSE,
                occlusionMode
            };
            gApi.iplApplyDirectSoundEffect(mDirectEffect, inputAudio, directPath, directOptions, 
                mDirectEffectOutputBuffer);
        }
        else
        {
            // If we're using default settings (i.e., no components were created in Unity), we will have to downmix
            // the input audio manually. This would normally be done by the direct sound effect.
            memset(mDirectEffectOutputBuffer.deinterleavedBuffer[0], 0, frameSize * sizeof(float));
            for (auto i = 0, index = 0; i < frameSize; ++i)
            {
                for (auto j = 0; j < inChannels; ++j, ++index)
                    mDirectEffectOutputBuffer.deinterleavedBuffer[0][i] += inputAudio.interleavedBuffer[index];

                mDirectEffectOutputBuffer.deinterleavedBuffer[0][i] /= static_cast<float>(inChannels);
            }
        }

        // Spatialize the direct sound.
        if (directBinaural)
        {
            gApi.iplApplyBinauralEffect(mBinauralEffect, mDirectEffectOutputBuffer, direction, hrtfInterpolation,
                mDirectSpatializedOutputBuffer);
        }
        else
        {
            gApi.iplApplyPanningEffect(mPanningEffect, mDirectEffectOutputBuffer, direction,
                mDirectSpatializedOutputBuffer);
        }

        // Adjust the level of direct sound according to the user-specified parameter.
        for (auto i = 0; i < outChannels * numSamples; ++i)
        {
            auto sample = i / outChannels;
            auto fraction = sample / (numSamples - 1.0f);
            auto level = fraction * directLevel + (1.0f - fraction) * mPreviousDirectMixLevel;

            outputAudio.interleavedBuffer[i] = level * mDirectSpatializedOutputBuffer.interleavedBuffer[i];
        }
        mPreviousDirectMixLevel = directLevel;

        // If we're not rendering indirect sound, stop here.
        if (!indirect)
        {
            if (mIndirectEffect && mUsedConvolutionEffect)
            {
                gApi.iplFlushConvolutionEffect(mIndirectEffect);
                mUsedConvolutionEffect = false;
            }

            return;
        }

        // We need to cancel out any distance attenuation applied by Unity before applying indirect effects to the
        // input audio.
        auto adjustedIndirectLevel = indirectLevel;
        adjustedIndirectLevel /= ((1.0f - spatializerData->spatialblend) + 
            spatializerData->spatialblend * mUnityDistanceAttenuation);

        // Adjust the level of indirect sound according to the user-specified parameter.
        for (auto i = 0; i < inChannels * numSamples; ++i)
        {
            auto sample = i / inChannels;
            auto fraction = i / (numSamples - 1.0f);
            auto level = fraction * adjustedIndirectLevel + (1.0f - fraction) * mPreviousIndirectMixLevel;

            inputAudio.interleavedBuffer[i] *= level;
        }
        mPreviousIndirectMixLevel = adjustedIndirectLevel;

        // Send audio to the convolution effect.
        gApi.iplSetConvolutionEffectName(mIndirectEffect, const_cast<IPLstring>(name.c_str()));
        gApi.iplSetDryAudioForConvolutionEffect(mIndirectEffect, sourcePosition, inputAudio);
        mUsedConvolutionEffect = true;

        // If we're using accelerated mixing, stop here.
        if (mEnvironmentalRenderer->isUsingAcceleratedMixing())
            return;

        // Retrieve the indirect sound for this source.
        gApi.iplGetWetAudioForConvolutionEffect(mIndirectEffect, listenerPosition, listenerAhead, listenerUp,
            mIndirectEffectOutputBuffer);

        // Spatialize the indirect sound.
        if (indirectBinaural)
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

        // Add the indirect sound to the output buffer (which already contains the direct sound).
        for (auto i = 0; i < outChannels * numSamples; ++i)
            outputAudio.interleavedBuffer[i] += mIndirectSpatializedOutputBuffer.interleavedBuffer[i];
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

    /** Handle to the panning effect used by this effect. */
    IPLhandle mPanningEffect;

    /** Handle to the object-based binaural effect used by this effect. */
    IPLhandle mBinauralEffect;

    /** An object that contains the rendering settings and binaural renderer used globally. */
    std::shared_ptr<AudioEngineSettings> mAudioEngineSettings;

    /** An object that contains the environmental renderer for the current scene. */
    std::shared_ptr<EnvironmentProxy> mEnvironmentalRenderer;

    /** Handle to the direct sound effect used by this effect. */
    IPLhandle mDirectEffect;

    /** Handle to the convolution effect used by this effect. */
    IPLhandle mIndirectEffect;

    /** Handle to the Ambisonics panning effect used by this effect. */
    IPLhandle mAmbisonicsPanningEffect;

    /** Handle to the Ambisonics binaural effect used by this effect. */
    IPLhandle mAmbisonicsBinauralEffect;

    /** Contiguous, deinterleaved buffer for storing the direct sound, before spatialization. */
    std::vector<float> mDirectEffectOutputBufferData;

    /** Array of pointers to per-channel data in the above buffer. */
    std::vector<float*> mDirectEffectOutputBufferChannels;

    /** Steam Audio buffer descriptor for the above buffer. */
    IPLAudioBuffer mDirectEffectOutputBuffer;

    /** Interleaved buffer for storing the direct sound, after spatialization. */
    std::vector<float> mDirectSpatializedOutputBufferData;

    /** Steam Audio buffer descriptor for the above buffer. */
    IPLAudioBuffer mDirectSpatializedOutputBuffer;

    /** Contiguous, deinterleaved buffer for storing the indirect sound, before spatialization. */
    std::vector<float> mIndirectEffectOutputBufferData;

    /** Array of pointers to per-channel data in the above buffer. */
    std::vector<float*> mIndirectEffectOutputBufferChannels;

    /** Steam Audio buffer descriptor for the above buffer. */
    IPLAudioBuffer mIndirectEffectOutputBuffer;

    /** Interleaved buffer for storing the indirect sound, after spatialization. */
    std::vector<float> mIndirectSpatializedOutputBufferData;

    /** Steam Audio buffer descriptor for the above buffer. */
    IPLAudioBuffer mIndirectSpatializedOutputBuffer;

    /** Have we used the convolution effect in the previous frame? */
    bool mUsedConvolutionEffect;

    /** Have we used the Ambisonics panning effect in the previous frame? */
    bool mUsedAmbisonicsPanningEffect;

    /** Have we used the Ambisonics binaural effect in the previous frame? */
    bool mUsedAmbisonicsBinauralEffect;

    /** Value of direct mix level used in the previous frame. */
    float mPreviousDirectMixLevel;

    /** Value of indirect mix level used in the previous frame. */
    float mPreviousIndirectMixLevel;

public:
    /** Distance attenuation applied by Unity. */
    float mUnityDistanceAttenuation;
};

/** Callback that is called by Unity when the Spatializer effect is created. This may be called in the editor when
 *  Spatialize is checked on an Audio Source, or when entering play mode. Therefore, it should only initialize
 *  user-controlled parameters.
 */
UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK createSpatializeEffect(UnityAudioEffectState* state)
{
    state->effectdata = new SpatializeEffectState{};
    state->spatializerdata->distanceattenuationcallback = recordUnityDistanceAttenuation;
    return UNITY_AUDIODSP_OK;
}

/** Callback that is called by Unity when the Spatializer effect is destroyed. This may be called in the editor when
 *  Spatialize is unchecked on an Audio Source, or just before entering play mode. It may not be called on exiting play
 *  mode, so audio processing state must be destroyed elsewhere.
 */
UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK releaseSpatializeEffect(UnityAudioEffectState* state)
{
    delete state->GetEffectData<SpatializeEffectState>();
    return UNITY_AUDIODSP_OK;
}

/** Callback that is called by Unity to process frames of audio data. Apart from in play mode, this may also be called
 *  in the editor when no sounds are playing, so this condition should be detected, and actual processing or
 *  initialization should be avoided in this case.
 */
UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK processSpatializeEffect(UnityAudioEffectState* state, float* inBuffer,
    float* outBuffer, unsigned int numSamples, int inChannels, int outChannels)
{
    auto params = state->GetEffectData<SpatializeEffectState>();
    params->process(inBuffer, outBuffer, numSamples, inChannels, outChannels, state->samplerate, state->dspbuffersize,
        state->flags, state->spatializerdata);
    return UNITY_AUDIODSP_OK;
}

/** Callback that is called by Unity when the inspector needs to update the value of some user-facing parameter.
 */
UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK setSpatializeEffectParam(UnityAudioEffectState* state, int index,
    float value)
{
    auto params = state->GetEffectData<SpatializeEffectState>();
    return params->setParameter(static_cast<SpatializeEffectParams>(index), value) ?
        UNITY_AUDIODSP_OK : UNITY_AUDIODSP_ERR_UNSUPPORTED;
}

/** Callback that is called by Unity when the inspector needs to query the value of some user-facing parameter.
 */
UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK getSpatializeEffectParam(UnityAudioEffectState* state, int index,
    float* value, char*)
{
    auto params = state->GetEffectData<SpatializeEffectState>();
    return params->getParameter(static_cast<SpatializeEffectParams>(index), *value) ?
        UNITY_AUDIODSP_OK : UNITY_AUDIODSP_ERR_UNSUPPORTED;
}

UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK recordUnityDistanceAttenuation(UnityAudioEffectState* state,
    float distanceIn, float attenuationIn, float* attenuationOut)
{
    *attenuationOut = attenuationIn;
    state->GetEffectData<SpatializeEffectState>()->mUnityDistanceAttenuation = attenuationIn;
    return UNITY_AUDIODSP_OK;
}

/** Descriptors for each user-facing parameter of the Spatializer effect. */
UnityAudioParameterDefinition gSpatializeEffectParams[] =
{
    { "DirectBinaural", "", "Spatialize direct sound using HRTF.", 0.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { "Interpolation", "", "HRTF interpolation.", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f },
    { "Distance", "", "Enable distance attenuation.", 0.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { "AirAbsorption", "", "Enable air absorption.", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f },
    { "OcclusionMode", "", "Direct occlusion and transmission mode.", 0.0f, 3.0f, 0.0f, 1.0f, 1.0f },
    { "OcclusionMethod", "", "Direct occlusion algorithm.", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f },
    { "SourceRadius", "", "Radius of the source.", 0.1f, 10.0f, 1.0f, 1.0f, 1.0f },
    { "DirectLevel", "", "Relative level of direct sound.", 0.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { "Indirect", "", "Enable indirect sound.", 0.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { "IndirBinaural", "", "Spatialize indirect sound using HRTF.", 0.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { "IndirLevel", "", "Relative level of indirect sound.", 0.0f, 10.0f, 1.0f, 1.0f, 1.0f },
    { "IndirType", "", "Real-time or baked.", 0.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { "StaticListener", "", "Uses static listener.", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f },
    { "Name", "", "Unique identifier for the source.", -std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), 1.0f, 1.0f, 1.0f }
};

/** Descriptor for the Spatializer effect. */
UnityAudioEffectDefinition gSpatializeEffect
{
    sizeof(UnityAudioEffectDefinition), 
    sizeof(UnityAudioParameterDefinition),
    UNITY_AUDIO_PLUGIN_API_VERSION, 
    STEAM_AUDIO_PLUGIN_VERSION,
    0, 
    SA_SPATIALIZE_NUM_PARAMS, 
    UnityAudioEffectDefinitionFlags_IsSpatializer,
    "Steam Audio Spatializer",
    createSpatializeEffect, 
    releaseSpatializeEffect, 
    nullptr, 
    processSpatializeEffect, 
    nullptr,
    gSpatializeEffectParams,
    setSpatializeEffectParam, 
    getSpatializeEffectParam, 
    nullptr
};
