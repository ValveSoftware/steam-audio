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
    SA_SPATIALIZE_PARAM_BYPASSDURINGINIT,
    SA_SPATIALIZE_PARAM_DIPOLEWEIGHT,
    SA_SPATIALIZE_PARAM_DIPOLEPOWER,
    SA_SPATIALIZE_PARAM_HRTFINDEX,
    SA_SPATIALIZE_PARAM_OVERRIDEHRTFINDEX,
    SA_SPATIALIZE_PARAM_DP_DISTANCEATTENUATION,
    SA_SPATIALIZE_PARAM_DP_AIRABSORPTIONLOW,
    SA_SPATIALIZE_PARAM_DP_AIRABSORPTIONMID,
    SA_SPATIALIZE_PARAM_DP_AIRABSORPTIONHIGH,
    SA_SPATIALIZE_PARAM_DP_PROPAGATIONDELAY,
    SA_SPATIALIZE_PARAM_DP_OCCLUSION,
    SA_SPATIALIZE_PARAM_DP_TRANSMISSIONLOW,
    SA_SPATIALIZE_PARAM_DP_TRANSMISSIONMID,
    SA_SPATIALIZE_PARAM_DP_TRANSMISSIONHIGH,
    SA_SPATIALIZE_PARAM_DP_DIRECTIVITY,
    SA_SPATIALIZE_PARAM_DA_TYPE,
    SA_SPATIALIZE_PARAM_DA_MINDISTANCE,
    SA_SPATIALIZE_PARAM_DA_CALLBACK_LOW,
    SA_SPATIALIZE_PARAM_DA_CALLBACK_HIGH,
    SA_SPATIALIZE_PARAM_DA_USERDATA_LOW,
    SA_SPATIALIZE_PARAM_DA_USERDATA_HIGH,
    SA_SPATIALIZE_PARAM_DA_DIRTY,
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

    float dipoleWeight;
    float dipolePower;
    
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
    IPLBakedDataIdentifier identifier;
    
    /** Whether or not the baked data we're using corresponds to a static listener node. Ignored if using real-time
        simulation. */
    bool usesStaticListener;

    /** Whether the effect should emit silence or dry audio while initialization is underway. */
    bool bypassDuringInitialization;

    int hrtfIndex;
    bool overrideHRTFIndex;

    IPLDirectSoundPath directPath;
    IPLDistanceAttenuationModel distanceAttenuationModel;

    /** The default constructor initializes parameters to default values.
     */
    SpatializeEffectState() :
        directBinaural{ true },
        hrtfInterpolation{ IPL_HRTFINTERPOLATION_NEAREST },
        distanceAttenuation{ false },
        airAbsorption{ false },
        dipoleWeight{ 0.0f },
        dipolePower{ 0.0f },
        occlusionMode{ IPL_DIRECTOCCLUSION_NONE },
        occlusionMethod{ IPL_DIRECTOCCLUSION_RAYCAST },
        sourceRadius{ 1.0f },
        directLevel{ 1.0f },
        indirect{ false },
        indirectBinaural{ false },
        indirectLevel{ 1.0f },
        indirectType{ IPL_SIMTYPE_REALTIME },
        identifier{},
        usesStaticListener{ false },
        bypassDuringInitialization{ false },
        hrtfIndex(0),
        overrideHRTFIndex(false),
        distanceAttenuationModel{IPL_DISTANCEATTENUATION_DEFAULT, 1.0f, nullptr},
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
        mIndirectEffectInputBuffer{},
        mIndirectEffectOutputBuffer{},
        mIndirectSpatializedOutputBuffer{},
        mUsedConvolutionEffect{ false },
        mUsedAmbisonicsPanningEffect{ false },
        mUsedAmbisonicsBinauralEffect{ false },
        mPreviousDirectMixLevel{ 0.0f },
        mPreviousIndirectMixLevel{ 0.0f },
        mBypassedInPreviousFrame{ false },
        mUnityDistanceAttenuation{ 1.0f }
    {
        directPath.distanceAttenuation = 1.0f;
        directPath.airAbsorption[0] = 1.0f;
        directPath.airAbsorption[1] = 1.0f;
        directPath.airAbsorption[2] = 1.0f;
        directPath.propagationDelay = 0.0f;
        directPath.occlusionFactor = 1.0f;
        directPath.transmissionFactor[0] = 1.0f;
        directPath.transmissionFactor[1] = 1.0f;
        directPath.transmissionFactor[2] = 1.0f;
        directPath.directivityFactor = 1.0f;
    }

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
        case SA_SPATIALIZE_PARAM_DIPOLEWEIGHT:
            value = dipoleWeight;
            return true;
        case SA_SPATIALIZE_PARAM_DIPOLEPOWER:
            value = dipolePower;
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
            value = *reinterpret_cast<float*>(&identifier.identifier);
            return true;
        case SA_SPATIALIZE_PARAM_BYPASSDURINGINIT:
            value = (bypassDuringInitialization) ? 1.0f : 0.0f;
            return true;
        case SA_SPATIALIZE_PARAM_HRTFINDEX:
            value = static_cast<float>(hrtfIndex);
            return true;
        case SA_SPATIALIZE_PARAM_OVERRIDEHRTFINDEX:
            value = (overrideHRTFIndex) ? 1.0f : 0.0f;
            return true;
        case SA_SPATIALIZE_PARAM_DP_DISTANCEATTENUATION:
            value = directPath.distanceAttenuation;
            return true;
        case SA_SPATIALIZE_PARAM_DP_AIRABSORPTIONLOW:
            value = directPath.airAbsorption[0];
            return true;
        case SA_SPATIALIZE_PARAM_DP_AIRABSORPTIONMID:
            value = directPath.airAbsorption[1];
            return true;
        case SA_SPATIALIZE_PARAM_DP_AIRABSORPTIONHIGH:
            value = directPath.airAbsorption[2];
            return true;
        case SA_SPATIALIZE_PARAM_DP_PROPAGATIONDELAY:
            value = directPath.propagationDelay;
            return true;
        case SA_SPATIALIZE_PARAM_DP_OCCLUSION:
            value = directPath.occlusionFactor;
            return true;
        case SA_SPATIALIZE_PARAM_DP_TRANSMISSIONLOW:
            value = directPath.transmissionFactor[0];
            return true;
        case SA_SPATIALIZE_PARAM_DP_TRANSMISSIONMID:
            value = directPath.transmissionFactor[1];
            return true;
        case SA_SPATIALIZE_PARAM_DP_TRANSMISSIONHIGH:
            value = directPath.transmissionFactor[2];
            return true;
        case SA_SPATIALIZE_PARAM_DP_DIRECTIVITY:
            value = directPath.directivityFactor;
            return true;
        
        case SA_SPATIALIZE_PARAM_DA_TYPE:
            value = static_cast<float>(static_cast<int>(distanceAttenuationModel.type));
            return true;
        case SA_SPATIALIZE_PARAM_DA_MINDISTANCE:
            value = distanceAttenuationModel.minDistance;
            return true;
#if defined(IPL_CPU_X64)
        case SA_SPATIALIZE_PARAM_DA_CALLBACK_LOW:
            value = reinterpret_cast<float*>(&distanceAttenuationModel.callback)[0];
            return true;
        case SA_SPATIALIZE_PARAM_DA_CALLBACK_HIGH:
            value = reinterpret_cast<float*>(&distanceAttenuationModel.callback)[1];
            return true;
        case SA_SPATIALIZE_PARAM_DA_USERDATA_LOW:
            value = reinterpret_cast<float*>(&distanceAttenuationModel.userData)[0];
            return true;
        case SA_SPATIALIZE_PARAM_DA_USERDATA_HIGH:
            value = reinterpret_cast<float*>(&distanceAttenuationModel.userData)[1];
            return true;
#else
        case SA_SPATIALIZE_PARAM_DA_CALLBACK_LOW:
            value = reinterpret_cast<float*>(&distanceAttenuationModel.callback)[0];
            return true;
        case SA_SPATIALIZE_PARAM_DA_CALLBACK_HIGH:
            value = 0.0f;
            return true;
        case SA_SPATIALIZE_PARAM_DA_USERDATA_LOW:
            value = reinterpret_cast<float*>(&distanceAttenuationModel.userData)[0];
            return true;
        case SA_SPATIALIZE_PARAM_DA_USERDATA_HIGH:
            value = 0.0f;
            return true;
#endif
        case SA_SPATIALIZE_PARAM_DA_DIRTY:
            value = (distanceAttenuationModel.dirty) ? 1.0f : 0.0f;
            return true;

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
        case SA_SPATIALIZE_PARAM_DIPOLEWEIGHT:
            dipoleWeight = value;
            return true;
        case SA_SPATIALIZE_PARAM_DIPOLEPOWER:
            dipolePower = value;
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
            identifier.type = (usesStaticListener) ? IPL_BAKEDDATATYPE_STATICLISTENER : IPL_BAKEDDATATYPE_STATICSOURCE;
            return true;
        case SA_SPATIALIZE_PARAM_NAME:
            identifier.identifier = *reinterpret_cast<int*>(&value);
            return true;
        case SA_SPATIALIZE_PARAM_BYPASSDURINGINIT:
            bypassDuringInitialization = (value == 1.0f);
            return true;
        case SA_SPATIALIZE_PARAM_HRTFINDEX:
            hrtfIndex = (int) value;
            return true;
        case SA_SPATIALIZE_PARAM_OVERRIDEHRTFINDEX:
            overrideHRTFIndex = (value == 1.0f);
            return true;
        case SA_SPATIALIZE_PARAM_DP_DISTANCEATTENUATION:
            directPath.distanceAttenuation = value;
            return true;
        case SA_SPATIALIZE_PARAM_DP_AIRABSORPTIONLOW:
            directPath.airAbsorption[0] = value;
            return true;
        case SA_SPATIALIZE_PARAM_DP_AIRABSORPTIONMID:
            directPath.airAbsorption[1] = value;
            return true;
        case SA_SPATIALIZE_PARAM_DP_AIRABSORPTIONHIGH:
            directPath.airAbsorption[2] = value;
            return true;
        case SA_SPATIALIZE_PARAM_DP_PROPAGATIONDELAY:
            directPath.propagationDelay = value;
            return true;
        case SA_SPATIALIZE_PARAM_DP_OCCLUSION:
            directPath.occlusionFactor = value;
            return true;
        case SA_SPATIALIZE_PARAM_DP_TRANSMISSIONLOW:
            directPath.transmissionFactor[0] = value;
            return true;
        case SA_SPATIALIZE_PARAM_DP_TRANSMISSIONMID:
            directPath.transmissionFactor[1] = value;
            return true;
        case SA_SPATIALIZE_PARAM_DP_TRANSMISSIONHIGH:
            directPath.transmissionFactor[2] = value;
            return true;
        case SA_SPATIALIZE_PARAM_DP_DIRECTIVITY:
            directPath.directivityFactor = value;
            return true;
        
        case SA_SPATIALIZE_PARAM_DA_TYPE:
            distanceAttenuationModel.type = static_cast<IPLDistanceAttenuationModelType>(static_cast<int>(value));
            return true;
        case SA_SPATIALIZE_PARAM_DA_MINDISTANCE:
            distanceAttenuationModel.minDistance = value;
            return true;
#if defined(IPL_CPU_X64)
        case SA_SPATIALIZE_PARAM_DA_CALLBACK_LOW:
            reinterpret_cast<float*>(&distanceAttenuationModel.callback)[0] = value;
            return true;
        case SA_SPATIALIZE_PARAM_DA_CALLBACK_HIGH:
            reinterpret_cast<float*>(&distanceAttenuationModel.callback)[1] = value;
            return true;
        case SA_SPATIALIZE_PARAM_DA_USERDATA_LOW:
            reinterpret_cast<float*>(&distanceAttenuationModel.userData)[0] = value;
            return true;
        case SA_SPATIALIZE_PARAM_DA_USERDATA_HIGH:
            reinterpret_cast<float*>(&distanceAttenuationModel.userData)[1] = value;
            return true;
#else
        case SA_SPATIALIZE_PARAM_DA_CALLBACK_LOW:
            reinterpret_cast<float*>(&distanceAttenuationModel.callback)[0] = value;
            return true;
        case SA_SPATIALIZE_PARAM_DA_CALLBACK_HIGH:
            return true;
        case SA_SPATIALIZE_PARAM_DA_USERDATA_LOW:
            reinterpret_cast<float*>(&distanceAttenuationModel.userData)[0] = value;
            return true;
        case SA_SPATIALIZE_PARAM_DA_USERDATA_HIGH:
            return true;
#endif
        case SA_SPATIALIZE_PARAM_DA_DIRTY:
            distanceAttenuationModel.dirty = (value == 1.0f) ? IPL_TRUE : IPL_FALSE;
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
            IPLRenderingSettings renderingSettings{ samplingRate, frameSize, IPL_CONVOLUTIONTYPE_PHONON };

            if (!mAudioEngineSettings || mAudioEngineSettings->settingsChanged(renderingSettings))
            {
                try
                {
                    if (mAudioEngineSettings)
                        mAudioEngineSettings->destroy();

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
        } else {
            mBinauralRenderer = mAudioEngineSettings->binauralRenderer();
        }
        if (!mBinauralRenderer)
            return false;

        // Check to see if an environmental renderer has just been created.
        if (!mEnvironmentalRenderer)
            mEnvironmentalRenderer = EnvironmentProxy::get();

        if (mIndirectEffectInputBufferData.empty()) {
            mIndirectEffectInputBufferData.resize(mInputFormat.numSpeakers * frameSize);
            mIndirectEffectInputBuffer.format = mInputFormat;
            mIndirectEffectInputBuffer.numSamples = frameSize;
            mIndirectEffectInputBuffer.interleavedBuffer = mIndirectEffectInputBufferData.data();
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
            if (gApi.iplCreatePanningEffect(mBinauralRenderer, mInputFormat, mOutputFormat,
                &mPanningEffect) != IPL_STATUS_SUCCESS)
            {
                return false;
            }
        }

        // Make sure the binaural effect has been created.
        if (mBinauralRenderer && !mBinauralEffect)
        {
            if (gApi.iplCreateBinauralEffect(mBinauralRenderer, mInputFormat, mOutputFormat,
                &mBinauralEffect) != IPL_STATUS_SUCCESS)
            {
                return false;
            }
        }

        // Make sure the direct sound effect has been created.
        if (!mDirectEffect)
        {
            if (gApi.iplCreateDirectSoundEffect(mInputFormat, mInputFormat, 
                mAudioEngineSettings->renderingSettings(), &mDirectEffect) != IPL_STATUS_SUCCESS)
            {
                return false;
            }
        }

        // Make sure the convolution effect has been created.
        if (mEnvironmentalRenderer && mEnvironmentalRenderer->environmentalRenderer() && indirect && !mIndirectEffect)
        {
            if (gApi.iplCreateConvolutionEffect(mEnvironmentalRenderer->environmentalRenderer(),
                identifier, indirectType, mInputFormat, mIndirectEffectOutputBuffer.format,
                &mIndirectEffect) != IPL_STATUS_SUCCESS)
            {
                return false;
            }
        }

        // Make sure the Ambisonics panning effect has been created.
        if (mBinauralRenderer && mEnvironmentalRenderer && mEnvironmentalRenderer->environmentalRenderer() && !mAmbisonicsPanningEffect)
        {
            if (gApi.iplCreateAmbisonicsPanningEffect(mBinauralRenderer, mIndirectEffectOutputBuffer.format, mOutputFormat,
                &mAmbisonicsPanningEffect) != IPL_STATUS_SUCCESS)
            {
                return false;
            }
        }

        // Make sure the Ambisonics binaural effect has been created.
        if (mBinauralRenderer && mEnvironmentalRenderer && mEnvironmentalRenderer->environmentalRenderer() && !mAmbisonicsBinauralEffect)
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

        auto nothingToOutput = false;

        // Make sure that audio processing state has been initialized. If initialization fails, stop and emit silence.
        if (!initialize(samplingRate, frameSize, inputFormat, outputFormat))
            nothingToOutput = true;

        if (!mPanningEffect || !mBinauralEffect || !mDirectEffect)
            nothingToOutput = true;

        if (indirect)
        {
            if (!mEnvironmentalRenderer || !mEnvironmentalRenderer->environmentalRenderer() || !mIndirectEffect ||
                !mAmbisonicsPanningEffect || !mAmbisonicsBinauralEffect)
            {
                nothingToOutput = true;
            }
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

        // Unity provides the world-space position of the source and the listener's transform matrix.
        auto S = spatializerData->sourcematrix;
        auto L = spatializerData->listenermatrix;
        auto sourcePosition = convertVector(S[12], S[13], S[14]);
        auto directionX = L[0] * S[12] + L[4] * S[13] + L[8] * S[14] + L[12];
        auto directionY = L[1] * S[12] + L[5] * S[13] + L[9] * S[14] + L[13];
        auto directionZ = L[2] * S[12] + L[6] * S[13] + L[10] * S[14] + L[14];

        auto lengthSquared = directionX * directionX + directionY * directionY + directionZ * directionZ;
        auto direction = (lengthSquared < 1e-4) ? IPLVector3{ .0f, 1.0f, .0f } : convertVector(directionX, directionY, directionZ);

        auto sourceAhead = unitVector(convertVector(S[8], S[9], S[10]));
        auto sourceUp = unitVector(convertVector(S[4], S[5], S[6]));
        auto sourceDirectivity = IPLDirectivity{ dipoleWeight, dipolePower, nullptr, nullptr };

        IPLSource source;
        source.position = sourcePosition;
        source.ahead = sourceAhead;
        source.up = sourceUp;
        source.directivity = sourceDirectivity;
        source.distanceAttenuationModel = distanceAttenuationModel;
        source.airAbsorptionModel = IPLAirAbsorptionModel{IPL_AIRABSORPTION_DEFAULT};

        if (indirect)
        {
            // Adjust the level of indirect sound according to the user-specified parameter.
            for (auto i = 0u; i < inChannels * numSamples; ++i)
            {
                auto fraction = i / (numSamples - 1.0f);
                auto level = fraction * indirectLevel + (1.0f - fraction) * mPreviousIndirectMixLevel;

                mIndirectEffectInputBuffer.interleavedBuffer[i] = level * inputAudio.interleavedBuffer[i];
            }
            mPreviousIndirectMixLevel = indirectLevel;
        }

        auto _spatialBlend = spatializerData->spatialblend;
        auto _distanceAttenuation = (distanceAttenuation) ? 
                                    directPath.distanceAttenuation : mUnityDistanceAttenuation;

        auto _distanceAttenuationPrime = 1.0f - _spatialBlend + _spatialBlend * _distanceAttenuation;
        auto _spatialBlendPrime = (_spatialBlend == 1.0f && _distanceAttenuation == .0f) ? 
                                  1.0f : _spatialBlend * _distanceAttenuation / _distanceAttenuationPrime;

        {
            // Apply direct sound modeling to the input audio, resulting in a mono buffer of audio.
            auto directOptions = IPLDirectSoundEffectOptions
            {
                IPL_TRUE,   // Distance attenuation is always applied, Steam Audio's or Unity's.
                airAbsorption ? IPL_TRUE : IPL_FALSE,
                (dipoleWeight > 0.0f) ? IPL_TRUE : IPL_FALSE,
                occlusionMode
            };

            IPLDirectSoundPath _directPath = directPath;
            _directPath.distanceAttenuation = _distanceAttenuationPrime;
            gApi.iplApplyDirectSoundEffect(mDirectEffect, inputAudio, _directPath, directOptions, inputAudio);
        }


        // Spatialize the direct sound.
        if (directBinaural)
        {
            gApi.iplApplyBinauralEffect(mBinauralEffect, mBinauralRenderer, inputAudio, direction, hrtfInterpolation, 
                                        _spatialBlendPrime, outputAudio);
        }
        else
        {
            gApi.iplApplyPanningEffect(mPanningEffect, mBinauralRenderer, inputAudio, direction, outputAudio);

            float _rawBlend = 1.0f - _spatialBlendPrime;
            for (auto i = 0u; i < outChannels * numSamples; ++i)
            {
                outputAudio.interleavedBuffer[i] = _spatialBlendPrime * outputAudio.interleavedBuffer[i] +
                                                   _rawBlend * inputAudio.interleavedBuffer[i];
            }
        }

        // Adjust the level of direct sound according to the user-specified parameter.
        for (auto i = 0u; i < outChannels * numSamples; ++i)
        {
            auto sample = i / outChannels;
            auto fraction = sample / (numSamples - 1.0f);
            auto level = fraction * directLevel + (1.0f - fraction) * mPreviousDirectMixLevel;

            outputAudio.interleavedBuffer[i] *= level;
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

            if (mBypassedInPreviousFrame)
            {
                crossfadeInputAndOutput(inBuffer, outChannels, numSamples, outBuffer);
                mBypassedInPreviousFrame = false;
            }

            return;
        }

        auto environment = IPLhandle{ nullptr };
        auto environmentalRenderer = IPLhandle{ nullptr };
        if (mEnvironmentalRenderer)
        {
            environment = mEnvironmentalRenderer->environment();
            environmentalRenderer = mEnvironmentalRenderer->environmentalRenderer();
        }

        // Send audio to the convolution effect.
        gApi.iplSetConvolutionEffectIdentifier(mIndirectEffect, identifier);
        gApi.iplSetDryAudioForConvolutionEffect(mIndirectEffect, source, mIndirectEffectInputBuffer);
        mUsedConvolutionEffect = true;

        // If we're using accelerated mixing, stop here.
        if (mEnvironmentalRenderer->isUsingAcceleratedMixing())
            return;

        auto listenerScaleSquared = 1.0f / (L[1] * L[1] + L[5] * L[5] + L[9] * L[9]);

        auto Lx = -listenerScaleSquared * (L[0] * L[12] + L[1] * L[13] + L[2] * L[14]);
        auto Ly = -listenerScaleSquared * (L[4] * L[12] + L[5] * L[13] + L[6] * L[14]);
        auto Lz = -listenerScaleSquared * (L[8] * L[12] + L[9] * L[13] + L[10] * L[14]);

        auto listenerPosition = convertVector(Lx, Ly, Lz);
        auto listenerUp = unitVector(convertVector(L[1], L[5], L[9]));
        auto listenerAhead = unitVector(convertVector(L[2], L[6], L[10]));

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

        // Add the indirect sound to the output buffer (which already contains the direct sound).
        for (auto i = 0u; i < outChannels * numSamples; ++i)
            outputAudio.interleavedBuffer[i] += mIndirectSpatializedOutputBuffer.interleavedBuffer[i];

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

    std::vector<float> mIndirectEffectInputBufferData;
    IPLAudioBuffer mIndirectEffectInputBuffer;

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

    std::vector<float> mDelayLine;

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

    /** Did we emit dry audio in the previous frame? */
    bool mBypassedInPreviousFrame;

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
    *attenuationOut = 1.0f;
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
    { "Name", "", "Unique identifier for the source.", -std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), 1.0f, 1.0f, 1.0f },
    { "BypassAtInit", "", "Bypass the effect during initialization.", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f },
    { "DipoleWeight", "", "Blends between omni and dipole directivity.", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f },
    { "DipolePower", "", "Sharpness of the dipole directivity.", 0.0f, 10.0f, 0.0f, 1.0f, 1.0f },
    { "HRTFIndex", "", "Index of the HRTF to use.", 0.0f, static_cast<float>(std::numeric_limits<int>::max()), 0.0f, 1.0f, 1.0f },
    { "OverrideHRTF", "", "Whether to override HRTF index per-source.", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f },
    { "DP_Distance", "", "DirectPath distance attenuation.", 0.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { "DP_AirAbsLow", "", "DirectPath air absorption, low.", 0.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { "DP_AirAbsMid", "", "DirectPath air absorption, mid.", 0.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { "DP_AirAbsHigh", "", "DirectPath air absorption, high.", 0.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { "DP_Delay", "", "DirectPath propagation delay.", 0.0f, std::numeric_limits<float>::max(), 0.0f, 1.0f, 1.0f },
    { "DP_Occlusion", "", "DirectPath occlusion.", 0.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { "DP_TransLow", "", "DirectPath transmission, low.", 0.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { "DP_TransMid", "", "DirectPath transmission, mid.", 0.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { "DP_TransHigh", "", "DirectPath transmission, high.", 0.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { "DP_Directivity", "", "DirectPath directivity.", 0.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { "DA_Type", "", "Distance attenuation type.", 0.0f, 2.0f, 0.0f, 1.0f, 1.0f },
    { "DA_MinDistance", "", "Distance attenuation minimum distance.", 0.1f, 100.0f, 1.0f, 1.0f, 1.0f },
    { "DA_CallbackLow", "", "Distance attenuation callback, low dword.", -std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), 1.0f, 1.0f, 1.0f },
    { "DA_CallbackHigh", "", "Distance attenuation callback, high dword.", -std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), 1.0f, 1.0f, 1.0f },
    { "DA_UserDataLow", "", "Distance attenuation userdata, low dword.", -std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), 1.0f, 1.0f, 1.0f },
    { "DA_UserDataHigh", "", "Distance attenuation userdata, high dword.", -std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), 1.0f, 1.0f, 1.0f },
    { "DA_Dirty", "", "Distance attenuation dirty flag.", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f }
};

/** Descriptor for the Spatializer effect. */
UnityAudioEffectDefinition gSpatializeEffect
{
    sizeof(UnityAudioEffectDefinition), 
    sizeof(UnityAudioParameterDefinition),
    UNITY_AUDIO_PLUGIN_API_VERSION, 
    STEAMAUDIO_UNITY_VERSION,
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
