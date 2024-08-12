//
// Copyright 2017-2023 Valve Corporation.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "steamaudio_unity_native.h"

namespace SteamAudioUnity {

#if !defined(IPL_OS_UNSUPPORTED)
extern std::shared_ptr<SourceManager> gSourceManager;
#endif

namespace SpatializeEffect {

enum Params
{
    APPLY_DISTANCEATTENUATION,
    APPLY_AIRABSORPTION,
    APPLY_DIRECTIVITY,
    APPLY_OCCLUSION,
    APPLY_TRANSMISSION,
    APPLY_REFLECTIONS,
    APPLY_PATHING,
    HRTF_INTERPOLATION,
    DISTANCEATTENUATION,
    DISTANCEATTENUATION_USECURVE,
    AIRABSORPTION_LOW,
    AIRABSORPTION_MID,
    AIRABSORPTION_HIGH,
    AIRABSORPTION_USERDEFINED,
    DIRECTIVITY,
    DIRECTIVITY_DIPOLEWEIGHT,
    DIRECTIVITY_DIPOLEPOWER,
    DIRECTIVITY_USERDEFINED,
    OCCLUSION,
    TRANSMISSION_TYPE,
    TRANSMISSION_LOW,
    TRANSMISSION_MID,
    TRANSMISSION_HIGH,
    DIRECT_MIXLEVEL,
    REFLECTIONS_BINAURAL,
    REFLECTIONS_MIXLEVEL,
    PATHING_BINAURAL,
    PATHING_MIXLEVEL,
    SIMULATION_OUTPUTS_PTR_LOW, // DEPRECATED
    SIMULATION_OUTPUTS_PTR_HIGH, // DEPRECATED
    DIRECT_BINAURAL,
    SIMULATION_OUTPUTS_HANDLE,
    PERSPECTIVE_CORRECTION,
    NUM_PARAMS
};

UnityAudioParameterDefinition gParamDefinitions[] =
{
    { "ApplyDA", "", "Apply distance attenuation.", 0.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { "ApplyAA", "", "Apply air absorption.", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f },
    { "ApplyDir", "", "Apply directivity.", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f },
    { "ApplyOccl", "", "Apply occlusion.", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f },
    { "ApplyTrans", "", "Apply transmission.", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f },
    { "ApplyRefl", "", "Apply reflections.", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f },
    { "ApplyPath", "", "Apply pathing.", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f },
    { "Interpolation", "", "HRTF interpolation.", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f },
    { "DistAtt", "", "Distance attenuation.", 0.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { "DistAttCurve", "", "Use Unity's built-in distance attenuation curve.", 0.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { "AirAbsLow", "", "Air absorption (low frequency).", 0.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { "AirAbsMid", "", "Air absorption (mid frequency).", 0.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { "AirAbsHigh", "", "Air absorption (high frequency).", 0.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { "AirAbsUD", "", "Air absorption is user-defined.", 0.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { "Directivity", "", "Directivity.", 0.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { "DirectivityDW", "", "Dipole weight.", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f },
    { "DirectivityDP", "", "Dipole power.", 0.0f, 4.0f, 0.0f, 1.0f, 1.0f },
    { "DirectivityUD", "", "Directivity is user-defined.", 0.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { "Occlusion", "", "Occlusion.", 0.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { "TransType", "", "Transmission type.", 0.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { "TransLow", "", "Transmission (low frequency).", 0.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { "TransMid", "", "Transmission (mid frequency).", 0.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { "TransHigh", "", "Transmission (high frequency).", 0.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { "DirMixLevel", "", "Direct mix level.", 0.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { "ReflBinaural", "", "Apply HRTF to reflections.", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f },
    { "ReflMixLevel", "", "Reflections mix level.", 0.0f, 10.0f, 1.0f, 1.0f, 1.0f },
    { "PathBinaural", "", "Apply HRTF to pathing.", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f },
    { "PathMixLevel", "", "Pathing mix level.", 0.0f, 10.0f, 1.0f, 1.0f, 1.0f },
    { "SimOutLow", "", "Simulation outputs (lower 32 bits).", -std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), 0.0f, 1.0f, 1.0f },
    { "SimOutHigh", "", "Simulation outputs (upper 32 bits).", -std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), 0.0f, 1.0f, 1.0f },
    { "DirectBinaural", "", "Apply HRTF to direct path.", 0.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { "SimOutHandle", "", "Simulation outputs handle.", std::numeric_limits<float>::min(), std::numeric_limits<float>::max(), -1.0f, 1.0f, 1.0f },
    { "PerspectiveCorr", "", "Apply perspective correction to direct path.", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f },
};

#if !defined(IPL_OS_UNSUPPORTED)

struct State
{
    bool directBinaural;
    bool perspectiveCorrection;
    bool applyDistanceAttenuation;
    bool applyAirAbsorption;
    bool applyDirectivity;
    bool applyOcclusion;
    bool applyTransmission;
    bool applyReflections;
    bool applyPathing;
    IPLHRTFInterpolation hrtfInterpolation;
    bool useDistanceAttenuationCurve;
    float distanceAttenuation;
    float distanceAttenuationCurveValue;
    float airAbsorption[3];
    bool airAbsorptionUserDefined;
    float directivity;
    float dipoleWeight;
    float dipolePower;
    bool directivityUserDefined;
    float occlusion;
    IPLTransmissionType transmissionType;
    float transmission[3];
    float directMixLevel;
    bool reflectionsBinaural;
    float reflectionsMixLevel;
    bool pathingBinaural;
    float pathingMixLevel;

    bool inputStarted;

    IPLSource simulationSource[2];
    std::atomic<bool> newSimulationSourceWritten;

    float prevDirectMixLevel;
    float prevReflectionsMixLevel;
    float prevPathingMixLevel;

    IPLAudioBuffer inBuffer;
    IPLAudioBuffer outBuffer;
    IPLAudioBuffer directBuffer;
    IPLAudioBuffer monoBuffer;
    IPLAudioBuffer reflectionsBuffer;
    IPLAudioBuffer reflectionsSpatializedBuffer;

    IPLPanningEffect panningEffect;
    IPLBinauralEffect binauralEffect;
    IPLDirectEffect directEffect;
    IPLReflectionEffect reflectionEffect;
    IPLPathEffect pathEffect;
    IPLAmbisonicsDecodeEffect ambisonicsEffect;
};

enum InitFlags
{
    INIT_NONE = 0,
    INIT_DIRECTAUDIOBUFFERS = 1 << 0,
    INIT_REFLECTIONAUDIOBUFFERS = 1 << 1,
    INIT_DIRECTEFFECT = 1 << 2,
    INIT_BINAURALEFFECT = 1 << 3,
    INIT_REFLECTIONEFFECT = 1 << 4,
    INIT_PATHEFFECT = 1 << 5,
    INIT_AMBISONICSEFFECT = 1 << 6
};

InitFlags lazyInit(UnityAudioEffectState* state,
                   int numChannelsIn,
                   int numChannelsOut)
{
    assert(state);

    auto initFlags = INIT_NONE;

    if (!gContext)
        return initFlags;

    if (!gHRTF[1])
        return initFlags;

    auto effect = state->GetEffectData<State>();
    if (!effect)
        return initFlags;

    IPLAudioSettings audioSettings;
    audioSettings.samplingRate = state->samplerate;
    audioSettings.frameSize = state->dspbuffersize;

    auto status = IPL_STATUS_SUCCESS;

    if (numChannelsOut > 0)
    {
        if (!effect->panningEffect)
        {
            IPLPanningEffectSettings effectSettings{};
            effectSettings.speakerLayout = speakerLayoutForNumChannels(numChannelsOut);

            status = iplPanningEffectCreate(gContext, &audioSettings, &effectSettings, &effect->panningEffect);
        }

        if (status == IPL_STATUS_SUCCESS)
        {
            if (!effect->binauralEffect)
            {
                IPLBinauralEffectSettings effectSettings;
                effectSettings.hrtf = gHRTF[1];

                status = iplBinauralEffectCreate(gContext, &audioSettings, &effectSettings, &effect->binauralEffect);
            }
        }

        if (status == IPL_STATUS_SUCCESS)
            initFlags = static_cast<InitFlags>(initFlags | INIT_BINAURALEFFECT);
    }

    if (numChannelsIn > 0)
    {
        status = IPL_STATUS_SUCCESS;
        if (!effect->directEffect)
        {
            IPLDirectEffectSettings effectSettings;
            effectSettings.numChannels = numChannelsIn;

            status = iplDirectEffectCreate(gContext, &audioSettings, &effectSettings, &effect->directEffect);
        }

        if (status == IPL_STATUS_SUCCESS)
            initFlags = static_cast<InitFlags>(initFlags | INIT_DIRECTEFFECT);
    }

    if (effect->applyReflections && gIsSimulationSettingsValid)
    {
        status = IPL_STATUS_SUCCESS;

        if (!effect->reflectionEffect)
        {
            IPLReflectionEffectSettings effectSettings;
            effectSettings.type = gSimulationSettings.reflectionType;
            effectSettings.numChannels = numChannelsForOrder(gSimulationSettings.maxOrder);
            effectSettings.irSize = numSamplesForDuration(gSimulationSettings.maxDuration, audioSettings.samplingRate);

            status = iplReflectionEffectCreate(gContext, &audioSettings, &effectSettings, &effect->reflectionEffect);
        }

        if (status == IPL_STATUS_SUCCESS)
            initFlags = static_cast<InitFlags>(initFlags | INIT_REFLECTIONEFFECT);
    }

    if (effect->applyPathing && gIsSimulationSettingsValid)
    {
        status = IPL_STATUS_SUCCESS;

        if (!effect->pathEffect)
        {
            IPLPathEffectSettings effectSettings{};
            effectSettings.maxOrder = gSimulationSettings.maxOrder;
            effectSettings.spatialize = IPL_TRUE;
            effectSettings.speakerLayout = speakerLayoutForNumChannels(numChannelsOut);
            effectSettings.hrtf = gHRTF[1];

            status = iplPathEffectCreate(gContext, &audioSettings, &effectSettings, &effect->pathEffect);
        }

        if (status == IPL_STATUS_SUCCESS)
            initFlags = static_cast<InitFlags>(initFlags | INIT_PATHEFFECT);
    }

    if (numChannelsOut > 0 && gIsSimulationSettingsValid)
    {
        status = IPL_STATUS_SUCCESS;

        if (!effect->ambisonicsEffect)
        {
            IPLAmbisonicsDecodeEffectSettings effectSettings;
            effectSettings.speakerLayout = speakerLayoutForNumChannels(numChannelsOut);
            effectSettings.hrtf = gHRTF[1];
            effectSettings.maxOrder = gSimulationSettings.maxOrder;

            status = iplAmbisonicsDecodeEffectCreate(gContext, &audioSettings, &effectSettings, &effect->ambisonicsEffect);
        }

        if (status == IPL_STATUS_SUCCESS)
            initFlags = static_cast<InitFlags>(initFlags | INIT_AMBISONICSEFFECT);
    }

    if (numChannelsIn > 0 && numChannelsOut > 0)
    {
        if (!effect->inBuffer.data)
            iplAudioBufferAllocate(gContext, numChannelsIn, audioSettings.frameSize, &effect->inBuffer);

        if (!effect->outBuffer.data)
            iplAudioBufferAllocate(gContext, numChannelsOut, audioSettings.frameSize, &effect->outBuffer);

        if (!effect->directBuffer.data)
            iplAudioBufferAllocate(gContext, numChannelsIn, audioSettings.frameSize, &effect->directBuffer);

        if (!effect->monoBuffer.data)
            iplAudioBufferAllocate(gContext, 1, audioSettings.frameSize, &effect->monoBuffer);

        initFlags = static_cast<InitFlags>(initFlags | INIT_DIRECTAUDIOBUFFERS);

        if ((effect->applyReflections || effect->applyPathing) && gIsSimulationSettingsValid)
        {
            auto numAmbisonicChannels = numChannelsForOrder(gSimulationSettings.maxOrder);

            if (!effect->reflectionsBuffer.data)
                iplAudioBufferAllocate(gContext, numAmbisonicChannels, audioSettings.frameSize, &effect->reflectionsBuffer);

            if (!effect->reflectionsSpatializedBuffer.data)
                iplAudioBufferAllocate(gContext, numChannelsOut, audioSettings.frameSize, &effect->reflectionsSpatializedBuffer);

            initFlags = static_cast<InitFlags>(initFlags | INIT_REFLECTIONAUDIOBUFFERS);
        }
    }

    return initFlags;
}

UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK recordDistanceAttenuation(UnityAudioEffectState* state,
                                                                        float distanceIn,
                                                                        float attenuationIn,
                                                                        float* attenuationOut)
{
    assert(state);

    if (attenuationOut)
    {
        *attenuationOut = 1.0f;
    }

    auto effect = state->GetEffectData<State>();
    if (effect)
    {
        effect->distanceAttenuationCurveValue = attenuationIn;
    }

    return UNITY_AUDIODSP_OK;
}

void reset(UnityAudioEffectState* state)
{
    assert(state);

    auto effect = state->GetEffectData<State>();
    if (!effect)
        return;

    effect->inputStarted = false;

    effect->directBinaural = true;
    effect->perspectiveCorrection = false;
    effect->applyDistanceAttenuation = true;
    effect->applyAirAbsorption = false;
    effect->applyDirectivity = false;
    effect->applyOcclusion = false;
    effect->applyTransmission = false;
    effect->applyReflections = false;
    effect->applyPathing = false;
    effect->hrtfInterpolation = IPL_HRTFINTERPOLATION_NEAREST;
    effect->useDistanceAttenuationCurve = true;
    effect->distanceAttenuation = 1.0f;
    effect->distanceAttenuationCurveValue = 1.0f;
    effect->airAbsorption[0] = 1.0f;
    effect->airAbsorption[1] = 1.0f;
    effect->airAbsorption[2] = 1.0f;
    effect->airAbsorptionUserDefined = false;
    effect->directivity = 1.0f;
    effect->dipoleWeight = 0.0f;
    effect->dipolePower = 0.0f;
    effect->directivityUserDefined = false;
    effect->occlusion = 1.0f;
    effect->transmissionType = IPL_TRANSMISSIONTYPE_FREQINDEPENDENT;
    effect->transmission[0] = 1.0f;
    effect->transmission[1] = 1.0f;
    effect->transmission[2] = 1.0f;
    effect->directMixLevel = 1.0f;
    effect->reflectionsBinaural = false;
    effect->reflectionsMixLevel = 1.0f;
    effect->pathingMixLevel = 1.0f;
    effect->pathingBinaural = false;

    iplSourceRelease(&effect->simulationSource[0]);
    iplSourceRelease(&effect->simulationSource[1]);
    effect->newSimulationSourceWritten = false;

    effect->prevDirectMixLevel = 0.0f;
    effect->prevReflectionsMixLevel = 0.0f;
    effect->prevPathingMixLevel = 0.0f;
}

UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK create(UnityAudioEffectState* state)
{
    assert(state);

    state->effectdata = new State();

    if (state->spatializerdata)
    {
        state->spatializerdata->distanceattenuationcallback = recordDistanceAttenuation;
    }

    reset(state);
    lazyInit(state, 0, 0);
    return UNITY_AUDIODSP_OK;
}

UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK release(UnityAudioEffectState* state)
{
    assert(state);

    auto effect = state->GetEffectData<State>();
    if (!effect)
        return UNITY_AUDIODSP_OK;

    iplAudioBufferFree(gContext, &effect->inBuffer);
    iplAudioBufferFree(gContext, &effect->outBuffer);
    iplAudioBufferFree(gContext, &effect->directBuffer);
    iplAudioBufferFree(gContext, &effect->monoBuffer);
    iplAudioBufferFree(gContext, &effect->reflectionsBuffer);
    iplAudioBufferFree(gContext, &effect->reflectionsSpatializedBuffer);

    iplPanningEffectRelease(&effect->panningEffect);
    iplBinauralEffectRelease(&effect->binauralEffect);
    iplDirectEffectRelease(&effect->directEffect);
    iplReflectionEffectRelease(&effect->reflectionEffect);
    iplPathEffectRelease(&effect->pathEffect);
    iplAmbisonicsDecodeEffectRelease(&effect->ambisonicsEffect);

    effect->newSimulationSourceWritten = false;
    iplSourceRelease(&effect->simulationSource[0]);
    iplSourceRelease(&effect->simulationSource[1]);

    delete state->effectdata;

    return UNITY_AUDIODSP_OK;
}

UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK getParam(UnityAudioEffectState* state,
                                                       int index,
                                                       float* value,
                                                       char* valueStr)
{
    assert(state);
    assert(value);

    auto effect = state->GetEffectData<State>();
    if (!effect)
        return UNITY_AUDIODSP_OK;

    switch (index)
    {
    case DIRECT_BINAURAL:
        *value = (effect->directBinaural) ? 1.0f : 0.0f;
        break;
    case PERSPECTIVE_CORRECTION:
        *value = (effect->perspectiveCorrection) ? 1.0f : 0.0f;
        break;
    case APPLY_DISTANCEATTENUATION:
        *value = (effect->applyDistanceAttenuation) ? 1.0f : 0.0f;
        break;
    case APPLY_AIRABSORPTION:
        *value = (effect->applyAirAbsorption) ? 1.0f : 0.0f;
        break;
    case APPLY_DIRECTIVITY:
        *value = (effect->applyDirectivity) ? 1.0f : 0.0f;
        break;
    case APPLY_OCCLUSION:
        *value = (effect->applyOcclusion) ? 1.0f : 0.0f;
        break;
    case APPLY_TRANSMISSION:
        *value = (effect->applyTransmission) ? 1.0f : 0.0f;
        break;
    case APPLY_REFLECTIONS:
        *value = (effect->applyReflections) ? 1.0f : 0.0f;
        break;
    case APPLY_PATHING:
        *value = (effect->applyPathing) ? 1.0f : 0.0f;
        break;
    case HRTF_INTERPOLATION:
        *value = static_cast<float>(effect->hrtfInterpolation);
        break;
    case DISTANCEATTENUATION:
        *value = effect->distanceAttenuation;
        break;
    case DISTANCEATTENUATION_USECURVE:
        *value = (effect->useDistanceAttenuationCurve) ? 1.0f : 0.0f;
        break;
    case AIRABSORPTION_LOW:
        *value = effect->airAbsorption[0];
        break;
    case AIRABSORPTION_MID:
        *value = effect->airAbsorption[1];
        break;
    case AIRABSORPTION_HIGH:
        *value = effect->airAbsorption[2];
        break;
    case AIRABSORPTION_USERDEFINED:
        *value = (effect->airAbsorptionUserDefined) ? 1.0f : 0.0f;
        break;
    case DIRECTIVITY:
        *value = effect->directivity;
        break;
    case DIRECTIVITY_DIPOLEWEIGHT:
        *value = effect->dipoleWeight;
        break;
    case DIRECTIVITY_DIPOLEPOWER:
        *value = effect->dipolePower;
        break;
    case DIRECTIVITY_USERDEFINED:
        *value = (effect->directivityUserDefined) ? 1.0f : 0.0f;
        break;
    case OCCLUSION:
        *value = effect->occlusion;
        break;
    case TRANSMISSION_TYPE:
        *value = static_cast<float>(effect->transmissionType);
        break;
    case TRANSMISSION_LOW:
        *value = effect->transmission[0];
        break;
    case TRANSMISSION_MID:
        *value = effect->transmission[1];
        break;
    case TRANSMISSION_HIGH:
        *value = effect->transmission[2];
        break;
    case DIRECT_MIXLEVEL:
        *value = effect->directMixLevel;
        break;
    case REFLECTIONS_BINAURAL:
        *value = (effect->reflectionsBinaural) ? 1.0f : 0.0f;
        break;
    case REFLECTIONS_MIXLEVEL:
        *value = effect->reflectionsMixLevel;
        break;
    case PATHING_BINAURAL:
        *value = (effect->pathingBinaural) ? 1.0f : 0.0f;
        break;
    case PATHING_MIXLEVEL:
        *value = effect->pathingMixLevel;
        break;
    }

    return UNITY_AUDIODSP_OK;
}

void setSource(UnityAudioEffectState* state,
               IPLSource source)
{
    assert(state);

    auto effect = state->GetEffectData<State>();
    if (!effect)
        return;

    if (source == effect->simulationSource[1])
        return;

    if (!effect->newSimulationSourceWritten)
    {
        iplSourceRelease(&effect->simulationSource[1]);
        effect->simulationSource[1] = iplSourceRetain(source);

        effect->newSimulationSourceWritten = true;
    }
}

void getLatestSource(UnityAudioEffectState* state)
{
    assert(state);

    auto effect = state->GetEffectData<State>();
    if (!effect)
        return;

    if (effect->newSimulationSourceWritten)
    {
        iplSourceRelease(&effect->simulationSource[0]);
        effect->simulationSource[0] = iplSourceRetain(effect->simulationSource[1]);

        effect->newSimulationSourceWritten = false;
    }
}

UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK setParam(UnityAudioEffectState* state,
                                                       int index,
                                                       float value)
{
    assert(state);

    auto effect = state->GetEffectData<State>();
    if (!effect)
        return UNITY_AUDIODSP_OK;

    switch (index)
    {
    case DIRECT_BINAURAL:
        effect->directBinaural = (value == 1.0f);
        break;
    case PERSPECTIVE_CORRECTION:
        effect->perspectiveCorrection = (value == 1.0f);
        break;
    case APPLY_DISTANCEATTENUATION:
        effect->applyDistanceAttenuation = (value == 1.0f);
        break;
    case APPLY_AIRABSORPTION:
        effect->applyAirAbsorption = (value == 1.0f);
        break;
    case APPLY_DIRECTIVITY:
        effect->applyDirectivity = (value == 1.0f);
        break;
    case APPLY_OCCLUSION:
        effect->applyOcclusion = (value == 1.0f);
        break;
    case APPLY_TRANSMISSION:
        effect->applyTransmission = (value == 1.0f);
        break;
    case APPLY_REFLECTIONS:
        effect->applyReflections = (value == 1.0f);
        break;
    case APPLY_PATHING:
        effect->applyPathing = (value == 1.0f);
        break;
    case HRTF_INTERPOLATION:
        effect->hrtfInterpolation = static_cast<IPLHRTFInterpolation>(static_cast<int>(value));
        break;
    case DISTANCEATTENUATION:
        effect->distanceAttenuation = value;
        break;
    case DISTANCEATTENUATION_USECURVE:
        effect->useDistanceAttenuationCurve = (value == 1.0f);
        break;
    case AIRABSORPTION_LOW:
        effect->airAbsorption[0] = value;
        break;
    case AIRABSORPTION_MID:
        effect->airAbsorption[1] = value;
        break;
    case AIRABSORPTION_HIGH:
        effect->airAbsorption[2] = value;
        break;
    case AIRABSORPTION_USERDEFINED:
        effect->airAbsorptionUserDefined = (value == 1.0f);
        break;
    case DIRECTIVITY:
        effect->directivity = value;
        break;
    case DIRECTIVITY_DIPOLEWEIGHT:
        effect->dipoleWeight = value;
        break;
    case DIRECTIVITY_DIPOLEPOWER:
        effect->dipolePower = value;
        break;
    case DIRECTIVITY_USERDEFINED:
        effect->directivityUserDefined = (value == 1.0f);
        break;
    case OCCLUSION:
        effect->occlusion = value;
        break;
    case TRANSMISSION_TYPE:
        effect->transmissionType = static_cast<IPLTransmissionType>(static_cast<int>(value));
        break;
    case TRANSMISSION_LOW:
        effect->transmission[0] = value;
        break;
    case TRANSMISSION_MID:
        effect->transmission[1] = value;
        break;
    case TRANSMISSION_HIGH:
        effect->transmission[2] = value;
        break;
    case DIRECT_MIXLEVEL:
        effect->directMixLevel = value;
        break;
    case REFLECTIONS_BINAURAL:
        effect->reflectionsBinaural = (value == 1.0f);
        break;
    case REFLECTIONS_MIXLEVEL:
        effect->reflectionsMixLevel = value;
        break;
    case PATHING_BINAURAL:
        effect->pathingBinaural = (value == 1.0f);
        break;
    case PATHING_MIXLEVEL:
        effect->pathingMixLevel = value;
        break;
    case SIMULATION_OUTPUTS_HANDLE:
        if (gSourceManager)
        {
            setSource(state, gSourceManager->getSource(static_cast<int>(value)));
        }
        break;
    }

    return UNITY_AUDIODSP_OK;
}

UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK process(UnityAudioEffectState* state,
                                                      float* in,
                                                      float* out,
                                                      unsigned int numSamples,
                                                      int numChannelsIn,
                                                      int numChannelsOut)
{
    assert(state);
    assert(in);
    assert(out);

    // Assume that the number of input and output channels are the same.
    assert(numChannelsIn == numChannelsOut);

    // Start by clearing the output buffer.
    memset(out, 0, numChannelsOut * numSamples * sizeof(float));

    // Unity can call the process callback even when not in play mode. In this case, emit silence.
    if (!(state->flags & UnityAudioEffectStateFlags_IsPlaying))
    {
        return UNITY_AUDIODSP_OK;
    }

    auto effect = state->GetEffectData<State>();
    if (!effect)
        return UNITY_AUDIODSP_OK;

    // If Unity is passing us a mono output buffer, do nothing.
    if (numChannelsOut < 2)
        return UNITY_AUDIODSP_OK;

    // Unity can call the process callback even when the audio source is not actually playing. When it does so, it
    // sends incorrect values for spatial blend, distance attenuation, and all the other parameters. Because the
    // direct effect performs a smooth ramp between gain values across multiple frames, it can try to smoothly ramp from
    // incorrect to correct values once playback actually starts. This will result in an audible artifact: the first
    // few frames of audio may be unexpectedly loud. To work around this, we don't perform any audio processing until
    // we see the first non-zero input audio sample, at which point parameters should be correct as well.
    if (!effect->inputStarted)
    {
        for (auto i = 0u; i < numChannelsIn * numSamples; ++i)
        {
            if (fabsf(in[i]) != 0.0f)
            {
                effect->inputStarted = true;
                break;
            }
        }

        if (!effect->inputStarted)
            return UNITY_AUDIODSP_OK;
    }

    // Make sure that audio processing state has been initialized. If initialization fails, stop and emit silence.
    // TODO: if nothing is initialized, do some fallback processing (passthrough, panning, or something like that).
    auto initFlags = lazyInit(state, numChannelsIn, numChannelsOut);
    if (!(initFlags & INIT_DIRECTAUDIOBUFFERS) || !(initFlags & INIT_BINAURALEFFECT) || !(initFlags & INIT_DIRECTEFFECT))
        return UNITY_AUDIODSP_OK;

    getLatestPerspectiveCorrection();
    getLatestHRTF();
    getLatestSource(state);

    // Local-to-world transform matrix for the source.
    auto S = state->spatializerdata->sourcematrix;

    // World-to-local transform matrix for the listener.
    auto L = state->spatializerdata->listenermatrix;

    auto listenerCoordinates = calcListenerCoordinates(L);
    auto sourceCoordinates = calcSourceCoordinates(S);

    if (effect->applyDistanceAttenuation && !effect->useDistanceAttenuationCurve)
    {
        IPLDistanceAttenuationModel distanceAttenuationModel{};
        distanceAttenuationModel.type = IPL_DISTANCEATTENUATIONTYPE_DEFAULT;

        effect->distanceAttenuation = iplDistanceAttenuationCalculate(gContext, sourceCoordinates.origin, listenerCoordinates.origin, &distanceAttenuationModel);
    }

    if (effect->applyAirAbsorption && !effect->airAbsorptionUserDefined)
    {
        IPLAirAbsorptionModel airAbsorptionModel{};
        airAbsorptionModel.type = IPL_AIRABSORPTIONTYPE_DEFAULT;

        iplAirAbsorptionCalculate(gContext, sourceCoordinates.origin, listenerCoordinates.origin, &airAbsorptionModel, effect->airAbsorption);
    }

    if (effect->applyDirectivity && !effect->directivityUserDefined)
    {
        IPLDirectivity directivity{};
        directivity.dipoleWeight = effect->dipoleWeight;
        directivity.dipolePower = effect->dipolePower;

        effect->directivity = iplDirectivityCalculate(gContext, sourceCoordinates, listenerCoordinates.origin, &directivity);
    }

    // Retrieve the spatial blend value.
    auto spatialBlend = state->spatializerdata->spatialblend;

    // Retrieve the distance attenuation value calculated by Unity OR the value
    // explicitly passed in as a parameter.
    auto distanceAttenuation = (effect->useDistanceAttenuationCurve) ? effect->distanceAttenuationCurveValue : effect->distanceAttenuation;

    // Modify spatial blend and distance attenuation, so as to allow distance attenuation to be
    // affected by spatial blend.
    auto _distanceAttenuation = (1.0f - spatialBlend) + spatialBlend * distanceAttenuation;
    auto _spatialBlend = (spatialBlend == 1.0f && distanceAttenuation == 0.0f) ? 1.0f : spatialBlend * distanceAttenuation / _distanceAttenuation;

    iplAudioBufferDeinterleave(gContext, in, &effect->inBuffer);

    IPLDirectEffectParams directParams;
    directParams.flags = static_cast<IPLDirectEffectFlags>(0);
    directParams.distanceAttenuation = _distanceAttenuation;
    directParams.airAbsorption[0] = effect->airAbsorption[0];
    directParams.airAbsorption[1] = effect->airAbsorption[1];
    directParams.airAbsorption[2] = effect->airAbsorption[2];
    directParams.directivity = effect->directivity;
    directParams.occlusion = effect->occlusion;
    directParams.transmissionType = effect->transmissionType;
    directParams.transmission[0] = effect->transmission[0];
    directParams.transmission[1] = effect->transmission[1];
    directParams.transmission[2] = effect->transmission[2];

    if (effect->applyDistanceAttenuation)
        directParams.flags = static_cast<IPLDirectEffectFlags>(directParams.flags | IPL_DIRECTEFFECTFLAGS_APPLYDISTANCEATTENUATION);
    if (effect->applyAirAbsorption)
        directParams.flags = static_cast<IPLDirectEffectFlags>(directParams.flags | IPL_DIRECTEFFECTFLAGS_APPLYAIRABSORPTION);
    if (effect->applyDirectivity)
        directParams.flags = static_cast<IPLDirectEffectFlags>(directParams.flags | IPL_DIRECTEFFECTFLAGS_APPLYDIRECTIVITY);
    if (effect->applyOcclusion)
        directParams.flags = static_cast<IPLDirectEffectFlags>(directParams.flags | IPL_DIRECTEFFECTFLAGS_APPLYOCCLUSION);
    if (effect->applyTransmission)
        directParams.flags = static_cast<IPLDirectEffectFlags>(directParams.flags | IPL_DIRECTEFFECTFLAGS_APPLYTRANSMISSION);

    iplDirectEffectApply(effect->directEffect, &directParams, &effect->inBuffer, &effect->directBuffer);

    IPLVector3 direction{ 0.0f, 1.0f, 0.0f };
    if (gPerspectiveCorrection[0].enabled && effect->perspectiveCorrection)
    {
        auto M = gPerspectiveCorrection[0].transform.elements[0];
        auto directionX = M[0] * S[12] + M[1] * S[13] + M[2] * S[14] + M[3];
        auto directionY = M[4] * S[12] + M[5] * S[13] + M[6] * S[14] + M[7];
        auto directionZ = M[8] * S[12] + M[9] * S[13] + M[10] * S[14] + M[11];
        auto directionW = M[12] * S[12] + M[13] * S[13] + M[14] * S[14] + M[15];

        float xfactor = gPerspectiveCorrection[0].xfactor;
        float yfactor = gPerspectiveCorrection[0].yfactor;

        if (fabs(directionW) > 1e-6f)
        {
            // Should always hit this. Zero check just to be safe.
            direction = convertVector(.5f * directionX * xfactor / fabsf(directionW), .5f * directionY * yfactor / fabsf(directionW), directionZ / fabsf(directionW));
        }
    }
    else
    {
        auto directionX = L[0] * S[12] + L[4] * S[13] + L[8] * S[14] + L[12];
        auto directionY = L[1] * S[12] + L[5] * S[13] + L[9] * S[14] + L[13];
        auto directionZ = L[2] * S[12] + L[6] * S[13] + L[10] * S[14] + L[14];
        direction = convertVector(directionX, directionY, directionZ);
    }

    if (dot(direction, direction) < 1e-6f)
        direction = IPLVector3{ 0.0f, 1.0f, 0.0f };

    bool directBinaural = numChannelsOut == 2 && effect->directBinaural && !gHRTFDisabled;
    if (directBinaural)
    {
        IPLBinauralEffectParams binauralParams{};
        binauralParams.direction = direction;
        binauralParams.interpolation = effect->hrtfInterpolation;
        binauralParams.spatialBlend = _spatialBlend;
        binauralParams.hrtf = gHRTF[0];

        iplBinauralEffectApply(effect->binauralEffect, &binauralParams, &effect->directBuffer, &effect->outBuffer);
    }
    else
    {
        iplAudioBufferDownmix(gContext, &effect->directBuffer, &effect->monoBuffer);

        IPLPanningEffectParams panningParams{};
        panningParams.direction = direction;

        iplPanningEffectApply(effect->panningEffect, &panningParams, &effect->monoBuffer, &effect->outBuffer);
    }

    for (auto i = 0; i < numChannelsOut; ++i)
    {
        applyVolumeRamp(effect->prevDirectMixLevel, effect->directMixLevel, numSamples, effect->outBuffer.data[i]);
    }
    effect->prevDirectMixLevel = effect->directMixLevel;

    if (effect->simulationSource[0])
    {
        IPLSimulationOutputs simulationOutputs{};
        iplSourceGetOutputs(effect->simulationSource[0], static_cast<IPLSimulationFlags>(IPL_SIMULATIONFLAGS_REFLECTIONS | IPL_SIMULATIONFLAGS_PATHING), &simulationOutputs);

        if (effect->applyReflections &&
            (initFlags & INIT_REFLECTIONAUDIOBUFFERS) && (initFlags & INIT_REFLECTIONEFFECT) && (initFlags && INIT_AMBISONICSEFFECT))
        {
            iplAudioBufferDownmix(gContext, &effect->inBuffer, &effect->monoBuffer);

            applyVolumeRamp(effect->prevReflectionsMixLevel, effect->reflectionsMixLevel, numSamples, effect->monoBuffer.data[0]);
            effect->prevReflectionsMixLevel = effect->reflectionsMixLevel;

            IPLReflectionEffectParams reflectionParams = simulationOutputs.reflections;
            reflectionParams.type = gSimulationSettings.reflectionType;
            reflectionParams.numChannels = numChannelsForOrder(gSimulationSettings.maxOrder);
            reflectionParams.irSize = numSamplesForDuration(gSimulationSettings.maxDuration, static_cast<int>(state->samplerate));
            reflectionParams.tanDevice = gSimulationSettings.tanDevice;

            if (gNewReflectionMixerWritten)
            {
                iplReflectionMixerRelease(&gReflectionMixer[0]);
                gReflectionMixer[0] = iplReflectionMixerRetain(gReflectionMixer[1]);

                gNewReflectionMixerWritten = false;
            }

            iplReflectionEffectApply(effect->reflectionEffect, &reflectionParams, &effect->monoBuffer, &effect->reflectionsBuffer, gReflectionMixer[0]);

            if (gSimulationSettings.reflectionType != IPL_REFLECTIONEFFECTTYPE_TAN && !gReflectionMixer[0])
            {
                IPLAmbisonicsDecodeEffectParams ambisonicsParams;
                ambisonicsParams.order = gSimulationSettings.maxOrder;
                ambisonicsParams.hrtf = gHRTF[0];
                ambisonicsParams.orientation = listenerCoordinates;
                ambisonicsParams.binaural = numChannelsOut == 2 && !gHRTFDisabled && (effect->reflectionsBinaural) ? IPL_TRUE : IPL_FALSE;

                iplAmbisonicsDecodeEffectApply(effect->ambisonicsEffect, &ambisonicsParams, &effect->reflectionsBuffer, &effect->reflectionsSpatializedBuffer);

                iplAudioBufferMix(gContext, &effect->reflectionsSpatializedBuffer, &effect->outBuffer);
            }
        }

        if (effect->applyPathing &&
            (initFlags & INIT_REFLECTIONAUDIOBUFFERS) && (initFlags & INIT_PATHEFFECT) && (initFlags && INIT_AMBISONICSEFFECT))
        {
            iplAudioBufferDownmix(gContext, &effect->inBuffer, &effect->monoBuffer);

            applyVolumeRamp(effect->prevPathingMixLevel, effect->pathingMixLevel, numSamples, effect->monoBuffer.data[0]);
            effect->prevPathingMixLevel = effect->pathingMixLevel;

            IPLPathEffectParams pathParams = simulationOutputs.pathing;
            pathParams.order = gSimulationSettings.maxOrder;
            pathParams.binaural = numChannelsOut == 2 && !gHRTFDisabled && (effect->pathingBinaural) ? IPL_TRUE : IPL_FALSE;
            pathParams.hrtf = gHRTF[0];
            pathParams.listener = listenerCoordinates;

            iplPathEffectApply(effect->pathEffect, &pathParams, &effect->monoBuffer, &effect->reflectionsSpatializedBuffer);

            iplAudioBufferMix(gContext, &effect->reflectionsSpatializedBuffer, &effect->outBuffer);
        }
    }

    iplAudioBufferInterleave(gContext, &effect->outBuffer, out);

    return UNITY_AUDIODSP_OK;
}

#else

UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK create(UnityAudioEffectState* state)
{
    return UNITY_AUDIODSP_OK;
}

UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK release(UnityAudioEffectState* state)
{
    return UNITY_AUDIODSP_OK;
}

UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK getParam(UnityAudioEffectState* state,
                                                       int index,
                                                       float* value,
                                                       char* valueStr)
{
    *value = 0.0f;
    return UNITY_AUDIODSP_OK;
}

UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK setParam(UnityAudioEffectState* state,
                                                       int index,
                                                       float value)
{
    return UNITY_AUDIODSP_OK;
}

UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK process(UnityAudioEffectState* state,
                                                      float* in,
                                                      float* out,
                                                      unsigned int numSamples,
                                                      int numChannelsIn,
                                                      int numChannelsOut)
{
    assert(numChannelsIn == numChannelsOut);

    memset(out, 0, numChannelsOut * numSamples * sizeof(float));

    if (state->flags & UnityAudioEffectStateFlags_IsPlaying)
    {
        memcpy(out, in, numChannelsOut * numSamples * sizeof(float));
    }

    return UNITY_AUDIODSP_OK;
}

#endif

}

UnityAudioEffectDefinition gSpatializeEffectDefinition
{
    sizeof(UnityAudioEffectDefinition),
    sizeof(UnityAudioParameterDefinition),
    UNITY_AUDIO_PLUGIN_API_VERSION,
    STEAMAUDIO_UNITY_VERSION,
    0,
    SpatializeEffect::NUM_PARAMS,
    UnityAudioEffectDefinitionFlags_IsSpatializer,
    "Steam Audio Spatializer",
    SpatializeEffect::create,
    SpatializeEffect::release,
    nullptr,
    SpatializeEffect::process,
    nullptr,
    SpatializeEffect::gParamDefinitions,
    SpatializeEffect::setParam,
    SpatializeEffect::getParam,
    nullptr
};

}
