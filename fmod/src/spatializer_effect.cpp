//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

#include <algorithm>
#include <cassert>
#include <vector>
#include "auto_load_library.h"
#include "audio_engine_settings.h"
#include "environment_proxy.h"

#include <string>

const int k_MaxHRTFIndices = 1000000;	// very big number for max # of HRTF data. FMod doesn't like this number to be too big.
const float k_maxPropagationDelay = 100000.0f;	// very big propagation delay since FMod doesn't like big numbers.

enum SpatializerEffectParams
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
	SA_SPATIALIZE_PARAM_SOURCEPOSITION,
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
	SA_SPATIALIZE_PARAM_OVERALLGAIN,
	SA_SPATIALIZE_NUM_PARAMS
};

FMOD_DSP_PARAMETER_DESC gSpatializerParamDirectBinaural = { FMOD_DSP_PARAMETER_TYPE_BOOL, "DirectBinaural", "", "Spatialize direct sound using HRTF." };
FMOD_DSP_PARAMETER_DESC gSpatializerParamInterpolation = { FMOD_DSP_PARAMETER_TYPE_INT, "Interpolation", "", "HRTF interpolation." };
FMOD_DSP_PARAMETER_DESC gSpatializerParamDistanceAttenuation = { FMOD_DSP_PARAMETER_TYPE_BOOL, "Distance", "", "Enable distance attenuation." };
FMOD_DSP_PARAMETER_DESC gSpatializerParamAirAbsorption = { FMOD_DSP_PARAMETER_TYPE_BOOL, "AirAbsorption", "", "Enable air absorption." };
FMOD_DSP_PARAMETER_DESC gSpatializerParamOcclusionMode = { FMOD_DSP_PARAMETER_TYPE_INT, "OcclusionMode", "", "Direct occlusion and transmission mode." };
FMOD_DSP_PARAMETER_DESC gSpatializerParamOcclusionMethod = { FMOD_DSP_PARAMETER_TYPE_INT, "OcclusionMethod", "", "Direct occlusion algorithm." };
FMOD_DSP_PARAMETER_DESC gSpatializerParamSourceRadius = { FMOD_DSP_PARAMETER_TYPE_FLOAT, "SourceRadius", "m", "Radius of the source." };
FMOD_DSP_PARAMETER_DESC gSpatializerParamDirectLevel = { FMOD_DSP_PARAMETER_TYPE_FLOAT, "DirectLevel", "", "Relative level of direct sound." };
FMOD_DSP_PARAMETER_DESC gSpatializerParamIndirect = { FMOD_DSP_PARAMETER_TYPE_BOOL, "Indirect", "", "Enable indirect sound." };
FMOD_DSP_PARAMETER_DESC gSpatializerParamIndirectBinaural = { FMOD_DSP_PARAMETER_TYPE_BOOL, "IndirBinaural", "", "Spatialize indirect sound using HRTF." };
FMOD_DSP_PARAMETER_DESC gSpatializerParamIndirectLevel = { FMOD_DSP_PARAMETER_TYPE_FLOAT, "IndirLevel", "", "Relative level of indirect sound." };
FMOD_DSP_PARAMETER_DESC gSpatializerParamIndirectType = { FMOD_DSP_PARAMETER_TYPE_INT, "IndirType", "", "Real-time or baked." };
FMOD_DSP_PARAMETER_DESC gSpatializerParamStaticListener = { FMOD_DSP_PARAMETER_TYPE_BOOL, "StaticListener", "", "Uses static listener." };
FMOD_DSP_PARAMETER_DESC gSpatializerParamName = { FMOD_DSP_PARAMETER_TYPE_DATA, "Name", "", "Unique identifier for the source." };
FMOD_DSP_PARAMETER_DESC gSpatializerParamSourcePosition = { FMOD_DSP_PARAMETER_TYPE_DATA, "SourcePos", "", "Position of the source." };
FMOD_DSP_PARAMETER_DESC gSpatializerParamDipoleWeight = { FMOD_DSP_PARAMETER_TYPE_FLOAT, "DipoleWeight", "", "Weighting of the dipole for the source." };
FMOD_DSP_PARAMETER_DESC gSpatializerParamDipolePower = { FMOD_DSP_PARAMETER_TYPE_FLOAT, "DipolePower", "", "Relative level of direct sound." };
FMOD_DSP_PARAMETER_DESC gSpatializerParamHRTFIndex = { FMOD_DSP_PARAMETER_TYPE_INT, "HRTFIndex", "", "Index of the HRTF data set." };
FMOD_DSP_PARAMETER_DESC gSpatializerParamOverrideHRTF = { FMOD_DSP_PARAMETER_TYPE_BOOL, "OverrideHRTF", "", "True if using HRTFIndex." };
FMOD_DSP_PARAMETER_DESC gSpatializerParamDP_Distance = { FMOD_DSP_PARAMETER_TYPE_FLOAT, "DP_Distance", "", "Internal directPatchValues" };
FMOD_DSP_PARAMETER_DESC gSpatializerParamDP_AirAbsLow = { FMOD_DSP_PARAMETER_TYPE_FLOAT, "DP_AirAbsLow", "", "Internal directPatchValues" };
FMOD_DSP_PARAMETER_DESC gSpatializerParamDP_AirAbsMid = { FMOD_DSP_PARAMETER_TYPE_FLOAT, "DP_AirAbsMid", "", "Internal directPatchValues" };
FMOD_DSP_PARAMETER_DESC gSpatializerParamDP_AirAbsHigh = { FMOD_DSP_PARAMETER_TYPE_FLOAT, "DP_AirAbsHigh", "", "Internal directPatchValues" };
FMOD_DSP_PARAMETER_DESC gSpatializerParamDP_Delay = { FMOD_DSP_PARAMETER_TYPE_FLOAT, "DP_Delay", "", "Internal directPatchValues" };
FMOD_DSP_PARAMETER_DESC gSpatializerParamDP_Occlusion = { FMOD_DSP_PARAMETER_TYPE_FLOAT, "DP_Occlusion", "", "Internal directPatchValues" };
FMOD_DSP_PARAMETER_DESC gSpatializerParamDP_TransLow = { FMOD_DSP_PARAMETER_TYPE_FLOAT, "DP_TransLow", "", "Internal directPatchValues" };
FMOD_DSP_PARAMETER_DESC gSpatializerParamDP_TransMid = { FMOD_DSP_PARAMETER_TYPE_FLOAT, "DP_TransMid", "", "Internal directPatchValues" };
FMOD_DSP_PARAMETER_DESC gSpatializerParamDP_TransHigh = { FMOD_DSP_PARAMETER_TYPE_FLOAT, "DP_TransHigh", "", "Internal directPatchValues" };
FMOD_DSP_PARAMETER_DESC gSpatializerParamDP_Directivity = { FMOD_DSP_PARAMETER_TYPE_FLOAT, "DP_Directivity", "", "Internal directPatchValues" };
FMOD_DSP_PARAMETER_DESC gSpatializerParamOverallGain = { FMOD_DSP_PARAMETER_TYPE_DATA, "OverallGain", "", "Gaining to the max." };

FMOD_DSP_PARAMETER_DESC* gSpatializerEffectParams[] =
{
    &gSpatializerParamDirectBinaural,
    &gSpatializerParamInterpolation,
    &gSpatializerParamDistanceAttenuation,
    &gSpatializerParamAirAbsorption,
    &gSpatializerParamOcclusionMode,
    &gSpatializerParamOcclusionMethod,
    &gSpatializerParamSourceRadius,
    &gSpatializerParamDirectLevel,
    &gSpatializerParamIndirect,
    &gSpatializerParamIndirectBinaural,
    &gSpatializerParamIndirectLevel,
    &gSpatializerParamIndirectType,
    &gSpatializerParamStaticListener,
    &gSpatializerParamName,
    &gSpatializerParamSourcePosition,
	&gSpatializerParamDipoleWeight,
	&gSpatializerParamDipolePower,
	&gSpatializerParamHRTFIndex,
	&gSpatializerParamOverrideHRTF,
	&gSpatializerParamDP_Distance,
	&gSpatializerParamDP_AirAbsLow,
	&gSpatializerParamDP_AirAbsMid,
	&gSpatializerParamDP_AirAbsHigh,
	&gSpatializerParamDP_Delay,
	&gSpatializerParamDP_Occlusion,
	&gSpatializerParamDP_TransLow,
	&gSpatializerParamDP_TransMid,
	&gSpatializerParamDP_TransHigh,
	&gSpatializerParamDP_Directivity,
	&gSpatializerParamOverallGain,
};

char* gInterpolationValues[] = { "Nearest", "Bilinear" };
char* gOcclusionModeValues[] = { "Off", "On, No Transmission", "On, Frequency Independent Transmission", "On, Frequency Dependent Transmission" };
char* gOcclusionMethodValues[] = { "Raycast", "Partial" };
char* gIndirectTypeValues[] = { "Real-time", "Baked" };

void initSpatializerParamDescs()
{
    gSpatializerParamDirectBinaural.booldesc = { true, nullptr };
    gSpatializerParamInterpolation.intdesc = { 0, 1, 0, false, gInterpolationValues };
    gSpatializerParamDistanceAttenuation.booldesc = { false, nullptr };
    gSpatializerParamAirAbsorption.booldesc = { false, nullptr };
    gSpatializerParamOcclusionMode.intdesc = { 0, 3, 0, false, gOcclusionModeValues };
    gSpatializerParamOcclusionMethod.intdesc = { 0, 1, 0, false, gOcclusionMethodValues };
    gSpatializerParamSourceRadius.floatdesc = { 0.1f, 10.0f, 1.0f };
    gSpatializerParamDirectLevel.floatdesc = { 0.0f, 1.0f, 1.0f };
    gSpatializerParamIndirect.booldesc = { false, nullptr };
    gSpatializerParamIndirectBinaural.booldesc = { false, nullptr };
    gSpatializerParamIndirectLevel.floatdesc = { 0.0f, 10.0f, 1.0f };
    gSpatializerParamIndirectType.intdesc = { 0, 1, 0, false, gIndirectTypeValues };
    gSpatializerParamStaticListener.booldesc = { false, nullptr };
    gSpatializerParamName.datadesc = { FMOD_DSP_PARAMETER_DATA_TYPE_USER };
    gSpatializerParamSourcePosition.datadesc = { FMOD_DSP_PARAMETER_DATA_TYPE_3DATTRIBUTES };
	gSpatializerParamDipoleWeight.floatdesc = { 0.0f, 1.0f , 0.0f};
	gSpatializerParamDipolePower.floatdesc = { 0.0f, 4.0f, 0.0f };
	gSpatializerParamHRTFIndex.intdesc = { 0, k_MaxHRTFIndices, 0, false };
	gSpatializerParamOverrideHRTF.booldesc = { false, nullptr };
	gSpatializerParamDP_Distance.floatdesc = { 0.0f, 1.0f, 1.0f };
	gSpatializerParamDP_AirAbsLow.floatdesc = { 0.0f, 1.0f, 1.0f };
	gSpatializerParamDP_AirAbsMid.floatdesc = { 0.0f, 1.0f, 1.0f };
	gSpatializerParamDP_AirAbsHigh.floatdesc = { 0.0f, 1.0f, 1.0f };
	gSpatializerParamDP_Delay.floatdesc = { 0.0f, k_maxPropagationDelay, 0.0f };
	gSpatializerParamDP_Occlusion.floatdesc = { 0.0f, 1.0f, 1.0f };
	gSpatializerParamDP_TransLow.floatdesc = { 0.0f, 1.0f, 1.0f };
	gSpatializerParamDP_TransMid.floatdesc = { 0.0f, 1.0f, 1.0f };
	gSpatializerParamDP_TransHigh.floatdesc = { 0.0f, 1.0f, 1.0f };
	gSpatializerParamDP_Directivity.floatdesc = { 0.0f, 1.0f, 1.0f };
	gSpatializerParamOverallGain.datadesc = { FMOD_DSP_PARAMETER_DATA_TYPE_OVERALLGAIN };
}

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
    IPLBakedDataIdentifier identifier;

	/** Weighting for the dipole (directivity) function. Values from 0.0 (no effect) to 1.0f (full effect) */
	float dipoleWeight;

	/** Power for the dipole (directivity) function. Values from 0.0 to 4.0f */
	float dipolePower;

    /** Whether or not the baked data we're using corresponds to a static listener node. Ignored if using real-time
    simulation. */
    bool usesStaticListener;

    int hrtfIndex;
    bool overrideHRTFIndex;

	FMOD_DSP_PARAMETER_3DATTRIBUTES sourcePosition;
    FMOD_DSP_PARAMETER_OVERALLGAIN overallGain;
	IPLDirectSoundPath directPath;

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
		dipoleWeight{ 0.0f },
		dipolePower{ 0.0f }, 
        indirectType{ IPL_SIMTYPE_REALTIME },
        identifier{},
        usesStaticListener{ false },
		hrtfIndex(0),
		overrideHRTFIndex(false),
		mInputFormat{},
        mOutputFormat{},
        mBinauralRenderer{ nullptr },
        mPanningEffect{ nullptr },
        mBinauralEffect{ nullptr },
        mGlobalState{ nullptr },
        mSceneState{ nullptr },
        mDirectEffect{ nullptr },
        mIndirectEffect{ nullptr },
        mAmbisonicsPanningEffect{ nullptr },
        mAmbisonicsBinauralEffect{ nullptr },
        mDirectEffectOutputBuffer{},
        mUsedConvolutionEffect{ false },
        mUsedAmbisonicsPanningEffect{ false },
        mUsedAmbisonicsBinauralEffect{ false },
        mPreviousDirectMixLevel{ 0.0f },
        mPreviousIndirectMixLevel{ 0.0f },
        overallGain{ 1.0f, 0.0f }
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
    bool getParameter(SpatializerEffectParams index, 
                      bool& value)
    {
        switch (index)
        {
        case SA_SPATIALIZE_PARAM_DIRECTBINAURAL:
            value = directBinaural;
            return true;
        case SA_SPATIALIZE_PARAM_DISTANCEATTENUATION:
            value = distanceAttenuation;
            return true;
        case SA_SPATIALIZE_PARAM_AIRABSORPTION:
            value = airAbsorption;
            return true;
        case SA_SPATIALIZE_PARAM_INDIRECT:
            value = indirect;
            return true;
        case SA_SPATIALIZE_PARAM_INDIRECTBINAURAL:
            value = indirectBinaural;
            return true;
        case SA_SPATIALIZE_PARAM_STATICLISTENER:
            value = usesStaticListener;
            return true;
		case SA_SPATIALIZE_PARAM_OVERRIDEHRTFINDEX:
			value = overrideHRTFIndex;
			return true;
		default:
            return false;
        }
    }

    bool getParameter(SpatializerEffectParams index, 
                      int& value)
    {
        switch (index)
        {
        case SA_SPATIALIZE_PARAM_HRTFINTERPOLATION:
            value = static_cast<int>(hrtfInterpolation);
            return true;
        case SA_SPATIALIZE_PARAM_OCCLUSIONMODE:
            value = static_cast<int>(occlusionMode);
            return true;
        case SA_SPATIALIZE_PARAM_OCCLUSIONMETHOD:
            value = static_cast<int>(occlusionMethod);
            return true;
        case SA_SPATIALIZE_PARAM_SIMTYPE:
            value = static_cast<int>(indirectType);
            return true;
		case SA_SPATIALIZE_PARAM_HRTFINDEX:
			value = hrtfIndex;
			return true;
		default:
            return false;
        }
    }

    bool getParameter(SpatializerEffectParams index, 
                      float& value)
    {
        switch (index)
        {
        case SA_SPATIALIZE_PARAM_SOURCERADIUS:
            value = sourceRadius;
            return true;
        case SA_SPATIALIZE_PARAM_DIRECTLEVEL:
            value = directLevel;
            return true;
        case SA_SPATIALIZE_PARAM_INDIRECTLEVEL:
            value = indirectLevel;
            return true;
		case SA_SPATIALIZE_PARAM_DIPOLEPOWER:
			value = dipolePower;
			return true;
		case SA_SPATIALIZE_PARAM_DIPOLEWEIGHT:
			value = dipoleWeight;
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
		default:
            return false;
        }
    }

    bool getParameter(SpatializerEffectParams index,
                        void** value, unsigned int *length)
    {
        switch (index)
        {
        case SA_SPATIALIZE_PARAM_OVERALLGAIN:
            *value = static_cast<void*>(&overallGain);
            *length = sizeof(overallGain);
            return true;
        default:
            return false;
        }
    }
	
	/** Sets a parameter to a value provided by Unity. Returns true if the parameter index is valid.
    */
    bool setParameter(SpatializerEffectParams index, 
                      bool value)
    {
        switch (index)
        {
        case SA_SPATIALIZE_PARAM_DIRECTBINAURAL:
            directBinaural = value;
            return true;
        case SA_SPATIALIZE_PARAM_DISTANCEATTENUATION:
            distanceAttenuation = value;
            return true;
        case SA_SPATIALIZE_PARAM_AIRABSORPTION:
            airAbsorption = value;
            return true;
        case SA_SPATIALIZE_PARAM_INDIRECT:
            indirect = value;
            return true;
        case SA_SPATIALIZE_PARAM_INDIRECTBINAURAL:
            indirectBinaural = value;
            return true;
        case SA_SPATIALIZE_PARAM_STATICLISTENER:
            usesStaticListener = value;
            identifier.type = (usesStaticListener) ? IPL_BAKEDDATATYPE_STATICLISTENER : IPL_BAKEDDATATYPE_STATICSOURCE;
            return true;
		case SA_SPATIALIZE_PARAM_OVERRIDEHRTFINDEX:
			overrideHRTFIndex = value;
			return true;
		default:
            return false;
        }
    }

    bool setParameter(SpatializerEffectParams index, 
                      int value)
    {
        switch (index)
        {
        case SA_SPATIALIZE_PARAM_HRTFINTERPOLATION:
            hrtfInterpolation = static_cast<IPLHrtfInterpolation>(value);
            return true;
        case SA_SPATIALIZE_PARAM_OCCLUSIONMODE:
            occlusionMode = static_cast<IPLDirectOcclusionMode>(value);
            return true;
        case SA_SPATIALIZE_PARAM_OCCLUSIONMETHOD:
            occlusionMethod = static_cast<IPLDirectOcclusionMethod>(value);
            return true;
        case SA_SPATIALIZE_PARAM_SIMTYPE:
            indirectType = static_cast<IPLSimulationType>(value);
			return true;
		case SA_SPATIALIZE_PARAM_HRTFINDEX:
			hrtfIndex = value;
			return true;
		default:
            return false;
        }
    }

    bool setParameter(SpatializerEffectParams index, 
                      float value)
    {
        switch (index)
        {
        case SA_SPATIALIZE_PARAM_SOURCERADIUS:
            sourceRadius = value;
            return true;
        case SA_SPATIALIZE_PARAM_DIRECTLEVEL:
            directLevel = value;
            return true;
		case SA_SPATIALIZE_PARAM_INDIRECTLEVEL:
			indirectLevel = value;
			return true;
		case SA_SPATIALIZE_PARAM_DIPOLEWEIGHT:
			dipoleWeight = value;
			return true;
		case SA_SPATIALIZE_PARAM_DIPOLEPOWER:
			dipolePower = value;
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
                    const IPLAudioFormat inFormat,
                    const IPLAudioFormat outFormat)
    {
        mInputFormat = inFormat;
        mOutputFormat = outFormat;

        // Make sure the audio engine global state has been initialized, and the binaural renderer has been created.
        if (!mGlobalState)
        {
            mGlobalState = AudioEngineSettings::get();
            if (!mGlobalState)
                return false;
        }

		if (overrideHRTFIndex) {
			mBinauralRenderer = mGlobalState->binauralRenderer(hrtfIndex);
		}
		else {
			mBinauralRenderer = mGlobalState->binauralRenderer();
		}
		if (!mBinauralRenderer)
            return false;

        // Check to see if an environmental renderer has just been created.
        if (!mSceneState)
            mSceneState = SceneState::get();

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
        if (mSceneState && !mDirectEffect)
        {
            if (gApi.iplCreateDirectSoundEffect(mInputFormat, mDirectEffectOutputBuffer.format,
                                                mGlobalState->renderingSettings(), &mDirectEffect) != IPL_STATUS_SUCCESS)
            {
                return false;
            }
        }

        // Make sure the convolution effect has been created.
        if (mSceneState && indirect && !mIndirectEffect)
        {
            if (gApi.iplCreateConvolutionEffect(mSceneState->environmentalRenderer(),
                identifier, indirectType, mInputFormat, mIndirectEffectOutputBuffer.format,
                &mIndirectEffect) != IPL_STATUS_SUCCESS)
            {
                return false;
            }
        }

        // Make sure the Ambisonics panning effect has been created.
        if (mBinauralRenderer && mSceneState && !mAmbisonicsPanningEffect)
        {
            if (gApi.iplCreateAmbisonicsPanningEffect(mBinauralRenderer, mIndirectEffectOutputBuffer.format, mOutputFormat,
                &mAmbisonicsPanningEffect) != IPL_STATUS_SUCCESS)
            {
                return false;
            }
        }

        // Make sure the Ambisonics binaural effect has been created.
        if (mBinauralRenderer && mSceneState && !mAmbisonicsBinauralEffect)
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

        mSceneState = nullptr;
        mGlobalState = nullptr;
    }

    float calcDirectPathVolume(const IPLDirectSoundPath& directPath)
    {
        auto GetMaxFromArray = [](const float* myArray, int numElements)
        {
            float max = 0.0f;

            for (int i = 1; i < numElements ; ++i)
            {
	            max = std::max(max, myArray[i]);
            }

            return max;
        };
	
        float directPathVolume = (distanceAttenuation ? directPath.distanceAttenuation : 1.0f) *
                        (dipoleWeight > 0.0f ? directPath.directivityFactor : 1.0f);

        if (occlusionMode != IPL_DIRECTOCCLUSION_NONE)
        {
            float occlusionAttenuationFactor = 1.0f;
            
            if (occlusionMode == IPL_DIRECTOCCLUSION_NOTRANSMISSION)
            {
                occlusionAttenuationFactor = directPath.occlusionFactor;
            }
            else
            {
                occlusionAttenuationFactor = (directPath.occlusionFactor + (1 - directPath.occlusionFactor) * GetMaxFromArray(directPath.transmissionFactor, 3));
            }
	        directPathVolume *= occlusionAttenuationFactor;
        }

        if (airAbsorption)
        {
	        directPathVolume *= GetMaxFromArray(directPath.airAbsorption, 3);
        }

        return directPathVolume;

    }

    void setOverallGain(float gain)
    {
	    overallGain.linear_gain = std::min(1.0f, gain);
	    overallGain.linear_gain_additive = 0.0f;	// this is 0, as this is a volume Fmod sends to "behind the scenes" cooperative plugins, and we don't currently have that
    }

	IPLSource getSource()
	{

		auto CrossProduct = [](const IPLVector3& lhs, const IPLVector3& rhs)
		{
			IPLVector3 cross;

			cross.x = lhs.y * rhs.z - lhs.z * rhs.y;
			cross.y = lhs.z * rhs.x - lhs.x * rhs.z;
			cross.z = lhs.x * rhs.y - lhs.y * rhs.x;

			return cross;
		};

		auto sourcePos = convertVector(sourcePosition.absolute.position.x, sourcePosition.absolute.position.y,
			sourcePosition.absolute.position.z);

		// Create the source structure
		IPLSource source;
		source.position = sourcePos;
		source.ahead = convertVector(sourcePosition.absolute.forward.x, sourcePosition.absolute.forward.y, sourcePosition.absolute.forward.z);
		source.up = convertVector(sourcePosition.absolute.up.x, sourcePosition.absolute.up.y, sourcePosition.absolute.up.z);
		source.right = CrossProduct(source.ahead, source.up);
		source.directivity = IPLDirectivity{ dipoleWeight, dipolePower, nullptr, nullptr };
        source.distanceAttenuationModel = IPLDistanceAttenuationModel{ IPL_DISTANCEATTENUATION_DEFAULT };
        source.airAbsorptionModel = IPLAirAbsorptionModel{ IPL_AIRABSORPTION_DEFAULT };

		return source;
    }

	/** Updates the overall gain for Fmod to determine if the sound should be audible when it's trying to manage limited
     *  channel usage. If we don't update it, Fmod will never know it's louder than any currently playing sounds.
     */
    void updateOverallGain()
    {
        float spatializerLevel = directLevel;

        if (distanceAttenuation || airAbsorption || (dipoleWeight > 0.0f) || (occlusionMode != IPL_DIRECTOCCLUSION_NONE))
        {
            spatializerLevel *= calcDirectPathVolume(directPath);
        }

        mPreviousDirectMixLevel = directLevel;
        setOverallGain(spatializerLevel +  ((float) indirect) * indirectLevel);
    }

    /** Applies the Spatialize effect to audio flowing through an Audio Source.
     */
    void process(float* inBuffer, 
                 float* outBuffer, 
                 unsigned int numSamples, 
                 int inChannels, 
                 int outChannels,
                 int samplingRate, 
                 int frameSize, 
                 FMOD_DSP_STATE* dspState,
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
        if (!initialize(samplingRate, frameSize, inputFormat, outputFormat))
            return;

        if (!mPanningEffect || !mBinauralEffect)
            return;

        if (occlusionMode != IPL_DIRECTOCCLUSION_NONE)
        {
            if (!mSceneState || !mSceneState->environmentalRenderer() || !mDirectEffect)
                return;
        }

        if (indirect)
        {
            if (!mSceneState || !mSceneState->environmentalRenderer() || !mIndirectEffect ||
                !mAmbisonicsPanningEffect || !mAmbisonicsBinauralEffect)
            {
                return;
            }
        }

        auto source = getSource();
        auto direction = gApi.iplCalculateRelativeDirection(source.position, listenerPosition, listenerAhead, listenerUp);

        if (mSceneState && mSceneState->environmentalRenderer())
        {
            // Calculate the direct path parameters.
            // NOTE - this is now done in unity, and updated via the SetParameters settings. directPath is now a member variable
            //auto directPath = gApi.iplGetDirectSoundPath(mSceneState->environment(),
            //    listenerPosition, listenerAhead, listenerUp, source, sourceRadius, occlusionMode,
            //    occlusionMethod);

            // Apply direct sound modeling to the input audio, resulting in a mono buffer of audio.
            auto directOptions = IPLDirectSoundEffectOptions
            {
                distanceAttenuation ? IPL_TRUE : IPL_FALSE,
                airAbsorption ? IPL_TRUE : IPL_FALSE,
                (dipoleWeight > 0.0f) ? IPL_TRUE : IPL_FALSE,
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
            gApi.iplApplyBinauralEffect(mBinauralEffect, mBinauralRenderer, mDirectEffectOutputBuffer, direction,
                                        hrtfInterpolation, 1.0f, mDirectSpatializedOutputBuffer);
		}
        else
        {
            gApi.iplApplyPanningEffect(mPanningEffect, mBinauralRenderer, mDirectEffectOutputBuffer, direction,
                mDirectSpatializedOutputBuffer);
        }

        // Adjust the level of direct sound according to the user-specified parameter.
        for (auto i = 0u; i < outChannels * numSamples; ++i)
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

        // Adjust the level of indirect sound according to the user-specified parameter.
        for (auto i = 0u; i < inChannels * numSamples; ++i)
        {
            auto sample = i / inChannels;
            auto fraction = sample / (numSamples - 1.0f);
            auto level = fraction * adjustedIndirectLevel + (1.0f - fraction) * mPreviousIndirectMixLevel;

            inputAudio.interleavedBuffer[i] *= level;
        }
        mPreviousIndirectMixLevel = adjustedIndirectLevel;

        // Send audio to the convolution effect.
        gApi.iplSetConvolutionEffectIdentifier(mIndirectEffect, identifier);
        gApi.iplSetDryAudioForConvolutionEffect(mIndirectEffect, source, inputAudio);
        mUsedConvolutionEffect = true;

        // If we're using accelerated mixing, stop here.
        if (mSceneState->isUsingAcceleratedMixing())
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

        // Add the indirect sound to the output buffer (which already contains the direct sound).
        for (auto i = 0u; i < outChannels * numSamples; ++i)
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
    std::shared_ptr<AudioEngineSettings> mGlobalState;

    /** An object that contains the environmental renderer for the current scene. */
    std::shared_ptr<SceneState> mSceneState;

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

FMOD_RESULT F_CALL createSpatializerEffect(FMOD_DSP_STATE* state)
{
    state->plugindata = new SpatializeEffectState{};
    return FMOD_OK;
}

FMOD_RESULT F_CALL releaseSpatializerEffect(FMOD_DSP_STATE* state)
{
    delete reinterpret_cast<SpatializeEffectState*>(state->plugindata);
    return FMOD_OK;
}

FMOD_RESULT F_CALL getSpatializerBool(FMOD_DSP_STATE* state, 
                                      int index, 
                                      FMOD_BOOL* value, 
                                      char*)
{
    auto* params = reinterpret_cast<SpatializeEffectState*>(state->plugindata);
    auto boolValue = false;
    auto status = params->getParameter(static_cast<SpatializerEffectParams>(index), boolValue);
    *value = boolValue;
    return status ? FMOD_OK : FMOD_ERR_INVALID_PARAM;
}

FMOD_RESULT F_CALL getSpatializerData(FMOD_DSP_STATE* state,
                                      int index, 
                                      void** value, 
                                      unsigned int *length, 
                                      char *valuestr)
{
    auto* params = reinterpret_cast<SpatializeEffectState*>(state->plugindata);
    return params->getParameter(static_cast<SpatializerEffectParams>(index), value, length) ? FMOD_OK : FMOD_ERR_INVALID_PARAM;
}

FMOD_RESULT F_CALL getSpatializerInt(FMOD_DSP_STATE* state, 
                                     int index, 
                                     int* value, 
                                     char*)
{
    auto* params = reinterpret_cast<SpatializeEffectState*>(state->plugindata);
    return params->getParameter(static_cast<SpatializerEffectParams>(index), *value) ? FMOD_OK : FMOD_ERR_INVALID_PARAM;
}

FMOD_RESULT F_CALL getSpatializerFloat(FMOD_DSP_STATE* state, 
                                       int index, 
                                       float* value, 
                                       char*)
{
    auto* params = reinterpret_cast<SpatializeEffectState*>(state->plugindata);
    return params->getParameter(static_cast<SpatializerEffectParams>(index), *value) ? FMOD_OK : FMOD_ERR_INVALID_PARAM;
}

FMOD_RESULT F_CALL setSpatializerBool(FMOD_DSP_STATE* state, 
                                      int index, 
                                      FMOD_BOOL value)
{
    auto* params = reinterpret_cast<SpatializeEffectState*>(state->plugindata);
    return params->setParameter(static_cast<SpatializerEffectParams>(index), (value != 0)) ? FMOD_OK : FMOD_ERR_INVALID_PARAM;
}

FMOD_RESULT F_CALL setSpatializerInt(FMOD_DSP_STATE* state, 
                                     int index, 
                                     int value)
{
    auto* params = reinterpret_cast<SpatializeEffectState*>(state->plugindata);
    return params->setParameter(static_cast<SpatializerEffectParams>(index), value) ? FMOD_OK : FMOD_ERR_INVALID_PARAM;
}

FMOD_RESULT F_CALL setSpatializerFloat(FMOD_DSP_STATE* state, 
                                       int index, 
                                       float value)
{
    auto* params = reinterpret_cast<SpatializeEffectState*>(state->plugindata);
    return params->setParameter(static_cast<SpatializerEffectParams>(index), value) ? FMOD_OK : FMOD_ERR_INVALID_PARAM;
}

FMOD_RESULT F_CALL setSpatializerData(FMOD_DSP_STATE* state, 
                                      int index, 
                                      void* data, 
                                      unsigned int length)
{
    auto* params = reinterpret_cast<SpatializeEffectState*>(state->plugindata);

    if (static_cast<SpatializerEffectParams>(index) == SA_SPATIALIZE_PARAM_SOURCEPOSITION)
    {
        memcpy(&params->sourcePosition, data, length);
        return FMOD_OK;
    }
    else if (static_cast<SpatializerEffectParams>(index) == SA_SPATIALIZE_PARAM_NAME)
    {
        memcpy(&params->identifier.identifier, data, length);
        return FMOD_OK;
    }
    else
    {
        return FMOD_ERR_INVALID_PARAM;
    }
}

FMOD_RESULT F_CALL processSpatializerEffect(FMOD_DSP_STATE* state, 
                                            unsigned int length,
                                            const FMOD_DSP_BUFFER_ARRAY* inputBuffers, 
                                            FMOD_DSP_BUFFER_ARRAY* outputBuffers, 
                                            FMOD_BOOL inputsIdle,
                                            FMOD_DSP_PROCESS_OPERATION operation)
{
    auto GetListenerVectors = [](FMOD_DSP_STATE* state, IPLVector3& listenerPosition, IPLVector3& listenerAhead, IPLVector3& listenerUp)
    {
        auto numListeners = 1;
        FMOD_3D_ATTRIBUTES listener;
        state->functions->getlistenerattributes(state, &numListeners, &listener);

        listenerPosition = convertVector(listener.position.x, listener.position.y, listener.position.z);
        listenerAhead = convertVector(listener.forward.x, listener.forward.y, listener.forward.z);
        listenerUp = convertVector(listener.up.x, listener.up.y, listener.up.z);
    };

    if (operation == FMOD_DSP_PROCESS_QUERY)
    {
        if (outputBuffers)
        {
            outputBuffers->speakermode = FMOD_SPEAKERMODE_STEREO;
            outputBuffers->buffernumchannels[0] = 2;
            outputBuffers->bufferchannelmask[0] = 0;
        }

        if (inputsIdle)
        {
            // if the sound is idle, we still need to check the expected overall gain to help manage
            // channel counts. updateOverallGain won't do any processing - just determine how loud
            // the sound would be (according to attenuation, etc) if it were playing.
            // Note: the SteamAudio Unity plugin now calculates iplGetDirectSoundPath so this is even lighter
            auto* params = reinterpret_cast<SpatializeEffectState*>(state->plugindata);
            params->updateOverallGain();

            return FMOD_ERR_DSP_DONTPROCESS;
        }
        else
            return FMOD_OK;
    }
    else if (operation == FMOD_DSP_PROCESS_PERFORM)
    {
        auto* params = reinterpret_cast<SpatializeEffectState*>(state->plugindata);
        params->updateOverallGain();

        auto samplingRate = 0;
        auto frameSize = 0u;
        state->functions->getsamplerate(state, &samplingRate);
        state->functions->getblocksize(state, &frameSize);

        IPLVector3 listenerPosition;
        IPLVector3 listenerAhead;
        IPLVector3 listenerUp;
        GetListenerVectors(state, listenerPosition, listenerAhead, listenerUp);

        params->process(inputBuffers->buffers[0], outputBuffers->buffers[0], length, 
                        inputBuffers->buffernumchannels[0], outputBuffers->buffernumchannels[0], samplingRate, 
                        frameSize, state, listenerPosition, listenerAhead, listenerUp);
    }

    return FMOD_OK;
}

FMOD_RESULT F_CALL registerSpatializerEffect(FMOD_DSP_STATE* state)
{
    auto samplingRate = 0;
    auto frameSize = 0u;
    state->functions->getsamplerate(state, &samplingRate);
    state->functions->getblocksize(state, &frameSize);

    IPLRenderingSettings renderingSettings{ samplingRate, static_cast<int>(frameSize), IPL_CONVOLUTIONTYPE_PHONON };

    IPLAudioFormat outputFormat;
    outputFormat.channelLayoutType = IPL_CHANNELLAYOUTTYPE_SPEAKERS;
    outputFormat.channelLayout = IPL_CHANNELLAYOUT_STEREO;
    outputFormat.channelOrder = IPL_CHANNELORDER_INTERLEAVED;

	AudioEngineSettings::create(renderingSettings, outputFormat);

    return FMOD_OK;
}

FMOD_RESULT F_CALL deregisterSpatializerEffect(FMOD_DSP_STATE* state)
{
	AudioEngineSettings::destroy();

    return FMOD_OK;
}

FMOD_DSP_DESCRIPTION gSpatializerEffect =
{
    FMOD_PLUGIN_SDK_VERSION,
    "Steam Audio Spatializer",
    STEAMAUDIO_FMOD_VERSION,
    1, 1,
    createSpatializerEffect,
    releaseSpatializerEffect,
    nullptr,
    nullptr,
    processSpatializerEffect,
    nullptr,
    SA_SPATIALIZE_NUM_PARAMS,
    gSpatializerEffectParams,
    setSpatializerFloat,
    setSpatializerInt,
    setSpatializerBool,
    setSpatializerData,
    getSpatializerFloat,
    getSpatializerInt,
    getSpatializerBool,
    getSpatializerData,
    nullptr,
    nullptr,
    registerSpatializerEffect,
    deregisterSpatializerEffect,
    nullptr
};
