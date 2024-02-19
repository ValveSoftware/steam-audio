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

#include "phonon.h"

#define STEAMAUDIO_SKIP_API_FUNCTIONS
#include "phonon_interfaces.h"

#include "api_context.h"
#include "api_serialized_object.h"
#include "api_embree_device.h"
#include "api_opencl_device.h"
#include "api_radeonrays_device.h"
#include "api_tan_device.h"
#include "api_scene.h"
#include "api_hrtf.h"
#include "api_panning_effect.h"
#include "api_binaural_effect.h"
#include "api_virtual_surround_effect.h"
#include "api_ambisonics_encode_effect.h"
#include "api_ambisonics_panning_effect.h"
#include "api_ambisonics_binaural_effect.h"
#include "api_ambisonics_rotate_effect.h"
#include "api_ambisonics_decode_effect.h"
#include "api_direct_effect.h"
#include "api_indirect_effect.h"
#include "api_path_effect.h"
#include "api_probes.h"
#include "api_simulator.h"

namespace api {

// --------------------------------------------------------------------------------------------------------------------
// API Object Helpers
// --------------------------------------------------------------------------------------------------------------------

template <typename T, typename F, typename I, typename... Args>
IPLerror apiObjectAllocate(I** object, F* factory, Args&&... args)
{
    try
    {
        auto _object = reinterpret_cast<T*>(gMemory().allocate(sizeof(T), Memory::kDefaultAlignment));
        new (_object) T(factory, std::forward<Args>(args)...);
        *object = static_cast<I*>(_object);
    }
    catch (Exception exception)
    {
        return static_cast<IPLerror>(exception.status());
    }

    return IPL_STATUS_SUCCESS;
}


// --------------------------------------------------------------------------------------------------------------------
// Validation Helpers
// --------------------------------------------------------------------------------------------------------------------

// --- main validation macro

template <typename T>
std::string to_string(T value)
{
    return std::to_string(value);
}

template <typename T>
std::string to_string(T* value)
{
    char result[32] = {0};
    snprintf(result, 32, "%p", value);
    return std::string(result);
}

#define VALIDATE(type, value, test) { \
    if (!(test)) { \
        gLog().message(MessageSeverity::Warning, "%s: invalid %s: %s = %s", __func__, #type, #value, to_string(value).c_str()); \
    } \
}

// --- basic data types

#define VALIDATE_IPLfloat32(value) { \
    VALIDATE(IPLfloat32, value, Math::isFinite(value)); \
}

#define VALIDATE_IPLsize(value) { \
}

#define VALIDATE_POINTER(value) { \
    VALIDATE(void*, value, (value != nullptr)); \
}

// --- arrays of basic types

#define VALIDATE_ARRAY_IPLfloat32(value, size) { \
    VALIDATE_POINTER(value); \
    if (value) { \
        for (auto iArray = 0; iArray < size; ++iArray) { \
            auto isFinite = Math::isFinite(value[iArray]); \
            VALIDATE(IPLfloat32, value[iArray], isFinite); \
            if (!isFinite) { \
                break; \
            } \
        } \
    } \
}

// --- api enums

#define VALIDATE_IPLbool(value) { \
    VALIDATE(IPLbool, value, (IPL_FALSE <= value && value <= IPL_TRUE)); \
}

#define VALIDATE_IPLSIMDLevel(value) { \
    VALIDATE(IPLSIMDLevel, value, (IPL_SIMDLEVEL_SSE2 <= value && value <= IPL_SIMDLEVEL_AVX512)); \
}

#define VALIDATE_IPLAmbisonicsType(value) { \
    VALIDATE(IPLAmbisonicsType, value, (IPL_AMBISONICSTYPE_N3D <= value && value <= IPL_AMBISONICSTYPE_FUMA)); \
}

#define VALIDATE_IPLOpenCLDeviceType(value) { \
    VALIDATE(IPLOpenCLDeviceType, value, (IPL_OPENCLDEVICETYPE_ANY <= value && value <= IPL_OPENCLDEVICETYPE_GPU)); \
}

#define VALIDATE_IPLSceneType(value) { \
    VALIDATE(IPLSceneType, value, (IPL_SCENETYPE_DEFAULT <= value && value <= IPL_SCENETYPE_CUSTOM)); \
}

#define VALIDATE_IPLHRTFType(value) { \
    VALIDATE(IPLHRTFType, value, (IPL_HRTFTYPE_DEFAULT <= value && value <= IPL_HRTFTYPE_SOFA)); \
}

#define VALIDATE_IPLHRTFNormType(value) { \
    VALIDATE(IPLHRTFNormType, value, (IPL_HRTFNORMTYPE_NONE <= value && value <= IPL_HRTFNORMTYPE_RMS)); \
}

#define VALIDATE_IPLAudioEffectState(value) { \
    VALIDATE(IPLAudioEffectState, value, (IPL_AUDIOEFFECTSTATE_TAILREMAINING <= value && value <= IPL_AUDIOEFFECTSTATE_TAILCOMPLETE)); \
}

#define VALIDATE_IPLSpeakerLayoutType(value) { \
    VALIDATE(IPLSpeakerLayoutType, value, (IPL_SPEAKERLAYOUTTYPE_MONO <= value && value <= IPL_SPEAKERLAYOUTTYPE_CUSTOM)); \
}

#define VALIDATE_IPLHRTFInterpolation(value) { \
    VALIDATE(IPLHRTFInterpolation, value, (IPL_HRTFINTERPOLATION_NEAREST <= value && value <= IPL_HRTFINTERPOLATION_BILINEAR)); \
}

#define VALIDATE_IPLTransmissionType(value) { \
    VALIDATE(IPLTransmissionType, value, (IPL_TRANSMISSIONTYPE_FREQINDEPENDENT <= value && value <= IPL_TRANSMISSIONTYPE_FREQDEPENDENT)); \
}

#define VALIDATE_IPLReflectionEffectType(value) { \
    VALIDATE(IPLReflectionEffectType, value, (IPL_REFLECTIONEFFECTTYPE_CONVOLUTION <= value && value <= IPL_REFLECTIONEFFECTTYPE_TAN)); \
}

#define VALIDATE_IPLProbeGenerationType(value) { \
    VALIDATE(IPLProbeGenerationType, value, (IPL_PROBEGENERATIONTYPE_CENTROID <= value && value <= IPL_PROBEGENERATIONTYPE_UNIFORMFLOOR)); \
}

#define VALIDATE_IPLBakedDataType(value) { \
    VALIDATE(IPLBakedDataType, value, (IPL_BAKEDDATATYPE_REFLECTIONS <= value && value <= IPL_BAKEDDATATYPE_PATHING)); \
}

#define VALIDATE_IPLBakedDataVariation(value) { \
    VALIDATE(IPLBakedDataVariation, value, (IPL_BAKEDDATAVARIATION_REVERB <= value && value <= IPL_BAKEDDATAVARIATION_DYNAMIC)); \
}

#define VALIDATE_IPLOcclusionType(value) { \
    VALIDATE(IPLOcclusionType, value, (IPL_OCCLUSIONTYPE_RAYCAST <= value && value <= IPL_OCCLUSIONTYPE_VOLUMETRIC)); \
}

#define VALIDATE_IPLDistanceAttenuationModelType(value) { \
    VALIDATE(IPLDistanceAttenuationModelType, value, (IPL_DISTANCEATTENUATIONTYPE_DEFAULT <= value && value <= IPL_DISTANCEATTENUATIONTYPE_CALLBACK)); \
}

#define VALIDATE_IPLAirAbsorptionModelType(value) { \
    VALIDATE(IPLAirAbsorptionModelType, value, (IPL_AIRABSORPTIONTYPE_DEFAULT <= value && value <= IPL_AIRABSORPTIONTYPE_CALLBACK)); \
}

// -- api flag enums

#define VALIDATE_IPLDirectEffectFlags(value) { \
    VALIDATE(IPLDirectEffectFlags, value, ((value & ~(IPL_DIRECTEFFECTFLAGS_APPLYDISTANCEATTENUATION | IPL_DIRECTEFFECTFLAGS_APPLYAIRABSORPTION | IPL_DIRECTEFFECTFLAGS_APPLYDIRECTIVITY | IPL_DIRECTEFFECTFLAGS_APPLYOCCLUSION | IPL_DIRECTEFFECTFLAGS_APPLYTRANSMISSION)) == 0)); \
}

#define VALIDATE_IPLReflectionsBakeFlags(value) { \
    VALIDATE(IPLReflectionsBakeFlags, value, ((value & ~(IPL_REFLECTIONSBAKEFLAGS_BAKECONVOLUTION | IPL_REFLECTIONSBAKEFLAGS_BAKEPARAMETRIC)) == 0)); \
}

#define VALIDATE_IPLSimulationFlags(value) { \
    VALIDATE(IPLSimulationFlags, value, ((value & ~(IPL_SIMULATIONFLAGS_DIRECT | IPL_SIMULATIONFLAGS_REFLECTIONS | IPL_SIMULATIONFLAGS_PATHING)) == 0)); \
}

#define VALIDATE_IPLDirectSimulationFlags(value) { \
    VALIDATE(IPLDirectSimulationFlags, value, ((value & ~(IPL_DIRECTSIMULATIONFLAGS_DISTANCEATTENUATION | IPL_DIRECTSIMULATIONFLAGS_AIRABSORPTION | IPL_DIRECTSIMULATIONFLAGS_DIRECTIVITY | IPL_DIRECTSIMULATIONFLAGS_OCCLUSION | IPL_DIRECTSIMULATIONFLAGS_TRANSMISSION)) == 0)); \
}

// --- api structs

#define VALIDATE_IPLVector3(value) { \
    VALIDATE_IPLfloat32(value.x); \
    VALIDATE_IPLfloat32(value.y); \
    VALIDATE_IPLfloat32(value.z); \
}

#define VALIDATE_IPLMatrix4x4(value) { \
    for (int iRow = 0; iRow < 4; ++iRow) { \
        for (int iColumn = 0; iColumn < 4; ++iColumn) { \
            VALIDATE_IPLfloat32(value.elements[iRow][iColumn]); \
        } \
    } \
}

#define VALIDATE_IPLCoordinateSpace3(value) { \
    VALIDATE_IPLVector3(value.origin); \
    VALIDATE_IPLVector3(value.right); \
    VALIDATE_IPLVector3(value.up); \
    VALIDATE_IPLVector3(value.ahead); \
}

#define VALIDATE_IPLSphere(value) { \
    VALIDATE_IPLVector3(value.center); \
    VALIDATE(IPLfloat32, value.radius, (value.radius >= 0.0f)); \
}

#define VALIDATE_IPLAudioBuffer(value, validateData) { \
    VALIDATE_POINTER(value); \
    if (value) { \
        VALIDATE(IPLint32, value->numChannels, (value->numChannels > 0)); \
        VALIDATE(IPLint32, value->numSamples, (value->numSamples > 0)); \
        VALIDATE_POINTER(value->data); \
        if ((validateData) && value->data) { \
            for (auto iChannel = 0; iChannel < value->numChannels; ++iChannel) { \
                VALIDATE_IPLfloat32(value->data[iChannel][value->numSamples - 1]); \
            } \
        } \
    } \
}

#define VALIDATE_IPLContextSettings(value) { \
    VALIDATE_POINTER(value); \
    if (value) { \
        if (value->allocateCallback) { \
            VALIDATE_POINTER(value->freeCallback); \
        } \
        VALIDATE_IPLSIMDLevel(value->simdLevel); \
    } \
}

#define VALIDATE_IPLSerializedObjectSettings(value) { \
    VALIDATE_POINTER(value); \
    if (value) { \
        VALIDATE(IPLbyte*, value->data, (value->size == 0 || value->data != nullptr)); \
    } \
}

#define VALIDATE_IPLEmbreeDeviceSettings(value) { \
    VALIDATE_POINTER(value); \
}

#define VALIDATE_IPLOpenCLDeviceSettings(value) { \
    VALIDATE_POINTER(value); \
    if (value) { \
        VALIDATE_IPLOpenCLDeviceType(value->type); \
        VALIDATE(IPLint32, value->numCUsToReserve, (value->numCUsToReserve >= 0)); \
        VALIDATE(IPLfloat32, value->fractionCUsForIRUpdate, (0.0f <= value->fractionCUsForIRUpdate && value->fractionCUsForIRUpdate <= 1.0f)); \
        VALIDATE_IPLbool(value->requiresTAN); \
    } \
}

#define VALIDATE_IPLRadeonRaysDeviceSettings(value) { \
    VALIDATE_POINTER(value); \
}

#define VALIDATE_IPLTrueAudioNextDeviceSettings(value) { \
    VALIDATE_POINTER(value); \
    if (value) { \
        VALIDATE(IPLint32, value->frameSize, (value->frameSize > 0)); \
        VALIDATE(IPLint32, value->irSize, (value->irSize > 0)); \
        VALIDATE(IPLint32, value->order, (value->order >= 0)); \
        VALIDATE(IPLint32, value->maxSources, (value->maxSources > 0)); \
    } \
}

#define VALIDATE_IPLSceneSettings(value) { \
    VALIDATE_POINTER(value);  \
    if (value) { \
        VALIDATE_IPLSceneType(value->type); \
        if (value->type == IPL_SCENETYPE_CUSTOM) { \
            VALIDATE_POINTER(value->closestHitCallback); \
            VALIDATE_POINTER(value->anyHitCallback); \
        } \
        else if (value->type == IPL_SCENETYPE_EMBREE) { \
            VALIDATE_POINTER(value->embreeDevice); \
        } \
        else if (value->type == IPL_SCENETYPE_RADEONRAYS) { \
            VALIDATE_POINTER(value->radeonRaysDevice); \
        } \
    } \
}

#define VALIDATE_IPLMaterial(value) { \
    for (int iBand = 0; iBand < 3; ++iBand) { \
        VALIDATE(IPLfloat32, value.absorption[iBand], (0.0f <= value.absorption[iBand] && value.absorption[iBand] <= 1.0f)); \
        VALIDATE(IPLfloat32, value.transmission[iBand], (0.0f <= value.transmission[iBand] && value.transmission[iBand] <= 1.0f)); \
    } \
    VALIDATE(IPLfloat32, value.scattering, (0.0f <= value.scattering && value.scattering <= 1.0f)); \
}

#define VALIDATE_IPLStaticMeshSettings(value) { \
    VALIDATE_POINTER(value); \
    if (value) { \
        VALIDATE(IPLint32, value->numVertices, (value->numVertices > 0)); \
        VALIDATE(IPLint32, value->numTriangles, (value->numTriangles > 0)); \
        VALIDATE(IPLint32, value->numMaterials, (value->numMaterials > 0)); \
        VALIDATE_POINTER(value->vertices); \
        VALIDATE_POINTER(value->triangles); \
        VALIDATE_POINTER(value->materialIndices); \
        VALIDATE_POINTER(value->materials); \
        for (int iVertex = 0; iVertex < value->numVertices; ++iVertex) { \
            VALIDATE_IPLVector3(value->vertices[iVertex]); \
        } \
        for (int iTriangle = 0; iTriangle < value->numTriangles; ++iTriangle) { \
            for (int iTriangleVertex = 0; iTriangleVertex < 3; ++iTriangleVertex) { \
                VALIDATE(IPLint32, value->triangles[iTriangle].indices[iTriangleVertex], (0 <= value->triangles[iTriangle].indices[iTriangleVertex] && value->triangles[iTriangle].indices[iTriangleVertex] < value->numVertices)); \
            } \
            VALIDATE(IPLint32, value->materialIndices[iTriangle], (0 <= value->materialIndices[iTriangle] && value->materialIndices[iTriangle] < value->numMaterials)); \
        } \
        for (int iMaterial = 0; iMaterial < value->numMaterials; ++iMaterial) { \
            VALIDATE_IPLMaterial(value->materials[iMaterial]); \
        } \
    } \
}

#define VALIDATE_IPLInstancedMeshSettings(value) { \
    VALIDATE_POINTER(value); \
    if (value) { \
        VALIDATE_POINTER(value->subScene); \
        VALIDATE_IPLMatrix4x4(value->transform); \
    } \
}

#define VALIDATE_IPLAudioSettings(value) { \
    VALIDATE_POINTER(value); \
    if (value) { \
        VALIDATE(IPLint32, value->samplingRate, (value->samplingRate > 0)); \
        VALIDATE(IPLint32, value->frameSize, (value->frameSize > 0)); \
    } \
}

#define VALIDATE_IPLHRTFSettings(value) { \
    VALIDATE_POINTER(value); \
    if (value) { \
        VALIDATE_IPLHRTFType(value->type); \
        if (value->type == IPL_HRTFTYPE_SOFA) { \
            if (!value->sofaFileName) { \
                VALIDATE_POINTER(value->sofaData); \
                VALIDATE(IPLint32, value->sofaDataSize, (value->sofaDataSize > 0)); \
            } \
        } \
        VALIDATE(IPLfloat32, value->volume, (value->volume > 0.0f)); \
        VALIDATE_IPLHRTFNormType(value->normType); \
    } \
}

#define VALIDATE_IPLSpeakerLayout(value) { \
    VALIDATE_IPLSpeakerLayoutType(value.type); \
    if (value.type == IPL_SPEAKERLAYOUTTYPE_CUSTOM) { \
        VALIDATE(IPLint32, value.numSpeakers, (value.numSpeakers > 0)); \
        VALIDATE_POINTER(value.speakers); \
        if (value.speakers) { \
            for (int iSpeaker = 0; iSpeaker < value.numSpeakers; ++iSpeaker) { \
                VALIDATE_IPLVector3(value.speakers[iSpeaker]); \
            } \
        } \
    } \
}

#define VALIDATE_IPLPanningEffectSettings(value) { \
    VALIDATE_POINTER(value); \
    if (value) { \
        VALIDATE_IPLSpeakerLayout(value->speakerLayout); \
    } \
}

#define VALIDATE_IPLPanningEffectParams(value) { \
    VALIDATE_POINTER(value); \
    if (value) { \
        VALIDATE_IPLVector3(value->direction); \
    } \
}

#define VALIDATE_IPLBinauralEffectSettings(value) { \
    VALIDATE_POINTER(value); \
    if (value) { \
        VALIDATE_POINTER(value->hrtf); \
    } \
}

#define VALIDATE_IPLBinauralEffectParams(value) { \
    VALIDATE_POINTER(value); \
    if (value) { \
        VALIDATE_IPLVector3(value->direction); \
        VALIDATE_IPLHRTFInterpolation(value->interpolation); \
        VALIDATE(IPLfloat32, value->spatialBlend, (0.0f <= value->spatialBlend && value->spatialBlend <= 1.0f)); \
        VALIDATE_POINTER(value->hrtf); \
    } \
}

#define VALIDATE_IPLVirtualSurroundEffectSettings(value) { \
    VALIDATE_POINTER(value); \
    if (value) { \
        VALIDATE_IPLSpeakerLayout(value->speakerLayout); \
        VALIDATE_POINTER(value->hrtf); \
    } \
}

#define VALIDATE_IPLVirtualSurroundEffectParams(value) { \
    VALIDATE_POINTER(value); \
    if (value) { \
        VALIDATE_POINTER(value->hrtf); \
    } \
}

#define VALIDATE_IPLAmbisonicsEncodeEffectSettings(value) { \
    VALIDATE_POINTER(value); \
    if (value) { \
        VALIDATE(IPLint32, value->maxOrder, (value->maxOrder >= 0)); \
    } \
}

#define VALIDATE_IPLAmbisonicsEncodeEffectParams(value) { \
    VALIDATE_POINTER(value); \
    if (value) { \
        VALIDATE_IPLVector3(value->direction); \
        VALIDATE(IPLint32, value->order, (value->order >= 0)); \
    } \
}

#define VALIDATE_IPLAmbisonicsPanningEffectSettings(value) { \
    VALIDATE_POINTER(value); \
    if (value) { \
        VALIDATE_IPLSpeakerLayout(value->speakerLayout); \
        VALIDATE(IPLint32, value->maxOrder, (value->maxOrder >= 0)); \
    } \
}

#define VALIDATE_IPLAmbisonicsPanningEffectParams(value) { \
    VALIDATE_POINTER(value); \
    if (value) { \
        VALIDATE(IPLint32, value->order, (value->order >= 0)); \
    } \
}

#define VALIDATE_IPLAmbisonicsBinauralEffectSettings(value) { \
    VALIDATE_POINTER(value); \
    if (value) { \
        VALIDATE_POINTER(value->hrtf); \
        VALIDATE(IPLint32, value->maxOrder, (value->maxOrder >= 0)); \
    } \
}

#define VALIDATE_IPLAmbisonicsBinauralEffectParams(value) { \
    VALIDATE_POINTER(value); \
    if (value) { \
        VALIDATE_POINTER(value->hrtf); \
        VALIDATE(IPLint32, value->order, (value->order >= 0)); \
    } \
}

#define VALIDATE_IPLAmbisonicsRotationEffectSettings(value) { \
    VALIDATE_POINTER(value); \
    if (value) { \
        VALIDATE(IPLint32, value->maxOrder, (value->maxOrder >= 0)); \
    } \
}

#define VALIDATE_IPLAmbisonicsRotationEffectParams(value) { \
    VALIDATE_POINTER(value); \
    if (value) { \
        VALIDATE_IPLCoordinateSpace3(value->orientation); \
        VALIDATE(IPLint32, value->order, (value->order >= 0)); \
    } \
}

#define VALIDATE_IPLAmbisonicsDecodeEffectSettings(value) { \
    VALIDATE_POINTER(value); \
    if (value) { \
        VALIDATE_IPLSpeakerLayout(value->speakerLayout); \
        VALIDATE(IPLint32, value->maxOrder, (value->maxOrder >= 0)); \
    } \
}

#define VALIDATE_IPLAmbisonicsDecodeEffectParams(value) { \
    VALIDATE_POINTER(value); \
    if (value) { \
        VALIDATE(IPLint32, value->order, (value->order >= 0)); \
        VALIDATE_POINTER(value->hrtf); \
        VALIDATE_IPLCoordinateSpace3(value->orientation); \
        VALIDATE_IPLbool(value->binaural); \
    } \
}

#define VALIDATE_IPLDirectEffectSettings(value) { \
    VALIDATE_POINTER(value); \
    if (value) { \
        VALIDATE(IPLint32, value->numChannels, (value->numChannels > 0)); \
    } \
}

#define VALIDATE_IPLDirectEffectParams(value) { \
    VALIDATE_POINTER(value); \
    if (value) { \
        VALIDATE_IPLDirectEffectFlags(value->flags); \
        VALIDATE_IPLTransmissionType(value->flags); \
        VALIDATE(IPLfloat32, value->distanceAttenuation, (0.0f <= value->distanceAttenuation && value->distanceAttenuation <= 1.0f)); \
        for (int iBand = 0; iBand < 3; ++iBand) { \
            VALIDATE(IPLfloat32, value->airAbsorption[iBand], (0.0f <= value->airAbsorption[iBand] && value->airAbsorption[iBand] <= 1.0f)); \
        } \
        VALIDATE(IPLfloat32, value->directivity, (0.0f <= value->directivity && value->directivity <= 1.0f)); \
        VALIDATE(IPLfloat32, value->occlusion, (0.0f <= value->occlusion && value->occlusion <= 1.0f)); \
        for (int iBand = 0; iBand < 3; ++iBand) { \
            VALIDATE(IPLfloat32, value->transmission[iBand], (0.0f <= value->transmission[iBand] && value->transmission[iBand] <= 1.0f)); \
        } \
    } \
}

#define VALIDATE_IPLReflectionEffectSettings(value) { \
    VALIDATE_POINTER(value); \
    if (value) { \
        VALIDATE_IPLReflectionEffectType(value->type); \
        VALIDATE(IPLint32, value->irSize, (value->irSize > 0)); \
        VALIDATE(IPLint32, value->numChannels, (value->numChannels > 0)); \
    } \
}

#define VALIDATE_IPLReflectionEffectParams(value) { \
    VALIDATE_POINTER(value); \
    if (value) { \
        VALIDATE_IPLReflectionEffectType(value->type); \
        if (value->type == IPL_REFLECTIONEFFECTTYPE_CONVOLUTION || value->type == IPL_REFLECTIONEFFECTTYPE_HYBRID) { \
            VALIDATE_POINTER(value->ir); \
            VALIDATE(IPLint32, value->numChannels, (value->numChannels > 0)); \
            VALIDATE(IPLint32, value->irSize, (value->irSize > 0)); \
        } \
        if (value->type == IPL_REFLECTIONEFFECTTYPE_PARAMETRIC || value->type == IPL_REFLECTIONEFFECTTYPE_HYBRID) { \
            for (int iBand = 0; iBand < 3; ++iBand) { \
                VALIDATE(IPLfloat32, value->reverbTimes[iBand], (value->reverbTimes[iBand] > 0.0f)); \
            } \
        } \
        if (value->type == IPL_REFLECTIONEFFECTTYPE_HYBRID) { \
            for (int iBand = 0; iBand < 3; ++iBand) { \
                VALIDATE(IPLfloat32, value->eq[iBand], (0.0f < value->eq[iBand] && value->eq[iBand] <= 1.0f)); \
            } \
            VALIDATE(IPLint32, value->delay, (value->delay > 0)); \
        } \
        if (value->type == IPL_REFLECTIONEFFECTTYPE_TAN) { \
            VALIDATE_POINTER(value->tanDevice); \
            VALIDATE(IPLint32, value->tanSlot, (value->tanSlot >= 0)); \
        } \
    } \
}

#define VALIDATE_IPLPathEffectSettings(value) { \
    VALIDATE_POINTER(value); \
    if (value) { \
        VALIDATE(IPLint32, value->maxOrder, (value->maxOrder >= 0)); \
        VALIDATE_IPLbool(value->spatialize); \
        if (value->spatialize) { \
            VALIDATE_IPLSpeakerLayout(value->speakerLayout); \
        } \
    } \
}

#define VALIDATE_IPLPathEffectParams(value) { \
    VALIDATE_POINTER(value); \
    if (value) { \
        for (int iBand = 0; iBand < 3; ++iBand) { \
            VALIDATE(IPLfloat32, value->eqCoeffs[iBand], (0.0f < value->eqCoeffs[iBand] && value->eqCoeffs[iBand] <= 1.0f)); \
        } \
        VALIDATE(IPLint32, value->order, (value->order >= 0)); \
        for (int iCoeff = 0; iCoeff < (value->order + 1) * (value->order + 1); ++iCoeff) { \
            VALIDATE_IPLfloat32(value->shCoeffs[iCoeff]); \
        } \
        VALIDATE_IPLbool(value->binaural); \
        if (value->binaural) { \
            VALIDATE_POINTER(value->hrtf); \
            VALIDATE_IPLCoordinateSpace3(value->listener); \
        } \
    } \
}

#define VALIDATE_IPLProbeGenerationParams(value) { \
    VALIDATE_POINTER(value); \
    if (value) { \
        VALIDATE_IPLProbeGenerationType(value->type); \
        if (value->type != IPL_PROBEGENERATIONTYPE_CENTROID) { \
            VALIDATE(IPLfloat32, value->spacing, (value->spacing > 0.0f)); \
        } \
        if (value->type == IPL_PROBEGENERATIONTYPE_UNIFORMFLOOR) { \
            VALIDATE(IPLfloat32, value->height, (value->height > 0.0f)); \
        } \
        VALIDATE_IPLMatrix4x4(value->transform); \
    } \
}

#define VALIDATE_IPLBakedDataIdentifier(value) { \
    VALIDATE_IPLBakedDataType(value.type); \
    VALIDATE_IPLBakedDataVariation(value.variation); \
    VALIDATE_IPLSphere(value.endpointInfluence); \
}

#define VALIDATE_IPLReflectionsBakeParams(value) { \
    VALIDATE_POINTER(value); \
    if (value) { \
        VALIDATE_POINTER(value->scene); \
        VALIDATE_POINTER(value->probeBatch); \
        VALIDATE_IPLSceneType(value->sceneType); \
        VALIDATE_IPLBakedDataIdentifier(value->identifier); \
        VALIDATE_IPLReflectionsBakeFlags(value->bakeFlags); \
        VALIDATE(IPLint32, value->numRays, (value->numRays > 0)); \
        VALIDATE(IPLint32, value->numDiffuseSamples, (value->numDiffuseSamples > 0)); \
        VALIDATE(IPLint32, value->numBounces, (value->numBounces > 0)); \
        VALIDATE(IPLfloat32, value->simulatedDuration, (value->simulatedDuration > 0.0f)); \
        VALIDATE(IPLfloat32, value->savedDuration, (0.0f < value->savedDuration && value->savedDuration <= value->simulatedDuration)); \
        VALIDATE(IPLint32, value->order, (value->order > 0)); \
        VALIDATE(IPLint32, value->numThreads, (value->numThreads > 0)); \
        if (value->sceneType == IPL_SCENETYPE_CUSTOM) { \
            VALIDATE(IPLint32, value->rayBatchSize, (value->rayBatchSize > 0)); \
        } \
        VALIDATE(IPLfloat32, value->irradianceMinDistance, (value->irradianceMinDistance > 0.0f)); \
        if (value->sceneType == IPL_SCENETYPE_RADEONRAYS) { \
            VALIDATE(IPLint32, value->bakeBatchSize, (value->bakeBatchSize > 0)); \
            VALIDATE_POINTER(value->openCLDevice); \
            VALIDATE_POINTER(value->radeonRaysDevice); \
        } \
    } \
}

#define VALIDATE_IPLPathBakeParams(value) { \
    VALIDATE_POINTER(value); \
    if (value) { \
        VALIDATE_POINTER(value->scene); \
        VALIDATE_POINTER(value->probeBatch); \
        VALIDATE_IPLBakedDataIdentifier(value->identifier); \
        VALIDATE(IPLint32, value->numSamples, (value->numSamples > 0)); \
        VALIDATE(IPLfloat32, value->radius, (value->radius > 0.0f)); \
        VALIDATE(IPLfloat32, value->threshold, (value->threshold > 0.0f)); \
        VALIDATE(IPLfloat32, value->visRange, (value->visRange > 0.0f)); \
        VALIDATE(IPLfloat32, value->pathRange, (value->pathRange > 0.0f)); \
        VALIDATE(IPLint32, value->numThreads, (value->numThreads > 0)); \
    } \
}

#define VALIDATE_IPLSimulationSettings(value) { \
    VALIDATE_POINTER(value); \
    if (value) { \
        VALIDATE_IPLSimulationFlags(value->flags); \
        VALIDATE_IPLSceneType(value->sceneType); \
        VALIDATE_IPLReflectionEffectType(value->reflectionType); \
        VALIDATE(IPLint32, value->maxNumOcclusionSamples, (value->maxNumOcclusionSamples > 0)); \
        VALIDATE(IPLint32, value->maxNumRays, (value->maxNumRays > 0)); \
        VALIDATE(IPLint32, value->numDiffuseSamples, (value->numDiffuseSamples > 0)); \
        VALIDATE(IPLfloat32, value->maxDuration, (value->maxDuration > 0.0f)); \
        VALIDATE(IPLint32, value->maxOrder, (value->maxOrder >= 0)); \
        VALIDATE(IPLint32, value->maxNumSources, (value->maxNumSources > 0)); \
        VALIDATE(IPLint32, value->numThreads, (value->numThreads > 0)); \
        if (value->sceneType == IPL_SCENETYPE_CUSTOM) { \
            VALIDATE(IPLint32, value->rayBatchSize, (value->rayBatchSize > 0)); \
        } \
        VALIDATE(IPLint32, value->numVisSamples, (value->numVisSamples > 0)); \
        VALIDATE(IPLint32, value->samplingRate, (value->samplingRate > 0)); \
        VALIDATE(IPLint32, value->frameSize, (value->frameSize > 0)); \
        if (value->sceneType == IPL_SCENETYPE_RADEONRAYS || value->reflectionType == IPL_REFLECTIONEFFECTTYPE_TAN) { \
            VALIDATE_POINTER(value->openCLDevice); \
            if (value->sceneType == IPL_SCENETYPE_RADEONRAYS) { \
                VALIDATE_POINTER(value->radeonRaysDevice); \
            } \
            if (value->reflectionType == IPL_REFLECTIONEFFECTTYPE_TAN) { \
                VALIDATE_POINTER(value->tanDevice); \
            } \
        } \
    } \
}

#define VALIDATE_IPLSourceSettings(value) { \
    VALIDATE_POINTER(value); \
    if (value) { \
        VALIDATE_IPLSimulationFlags(value->flags); \
    } \
}

#define VALIDATE_IPLSimulationSharedInputs(value, flags) { \
    VALIDATE_POINTER(value); \
    if (value) { \
        VALIDATE_IPLCoordinateSpace3(value->listener); \
        if (flags & IPL_SIMULATIONFLAGS_REFLECTIONS) { \
            VALIDATE(IPLint32, value->numRays, (value->numRays > 0)); \
            VALIDATE(IPLint32, value->numBounces, (value->numBounces > 0)); \
            VALIDATE(IPLfloat32, value->duration, (value->duration > 0.0f)); \
            VALIDATE(IPLint32, value->order, (value->order >= 0)); \
            VALIDATE(IPLfloat32, value->irradianceMinDistance, (value->irradianceMinDistance > 0.0f)); \
        } \
    } \
}

#define VALIDATE_IPLSimulationInputs(value) { \
    VALIDATE_POINTER(value); \
    if (value) { \
        VALIDATE_IPLSimulationFlags(value->flags); \
        VALIDATE_IPLCoordinateSpace3(value->source); \
        if (value->flags & IPL_SIMULATIONFLAGS_DIRECT) { \
            VALIDATE_IPLDirectSimulationFlags(value->directFlags); \
            VALIDATE_IPLDistanceAttenuationModel((&value->distanceAttenuationModel)); \
            VALIDATE_IPLAirAbsorptionModel((&value->airAbsorptionModel)); \
            VALIDATE_IPLDirectivity((&value->directivity)); \
            VALIDATE_IPLOcclusionType(value->occlusionType); \
            VALIDATE(IPLfloat32, value->occlusionRadius, (value->occlusionRadius > 0.0f)); \
            VALIDATE(IPLint32, value->numOcclusionSamples, (value->numOcclusionSamples > 0)); \
            VALIDATE(IPLint32, value->numTransmissionRays, (value->numTransmissionRays > 0)); \
        } \
        if (value->flags & (IPL_SIMULATIONFLAGS_REFLECTIONS | IPL_SIMULATIONFLAGS_PATHING)) { \
            VALIDATE_IPLbool(value->baked); \
            if (value->baked) { \
                VALIDATE_IPLBakedDataIdentifier(value->bakedDataIdentifier); \
            } \
            if (value->flags & IPL_SIMULATIONFLAGS_REFLECTIONS) { \
                for (int iBand = 0; iBand < 3; ++iBand) { \
                    VALIDATE(IPLfloat32, value->reverbScale[iBand], (value->reverbScale[iBand] > 0.0f)); \
                } \
                VALIDATE(IPLfloat32, value->hybridReverbTransitionTime, (value->hybridReverbTransitionTime > 0.0f)); \
                VALIDATE(IPLfloat32, value->hybridReverbOverlapPercent, (0.0f < value->hybridReverbOverlapPercent && value->hybridReverbOverlapPercent <= 1.0f)); \
            } \
            if (value->flags & IPL_SIMULATIONFLAGS_PATHING) { \
                VALIDATE_POINTER(value->pathingProbes); \
                VALIDATE(IPLfloat32, value->visRadius, (value->visRadius >= 0.0f)); \
                VALIDATE(IPLfloat32, value->visThreshold, (value->visThreshold >= 0.0f)); \
                VALIDATE(IPLfloat32, value->visRange, (value->visRange >= 0.0f)); \
                VALIDATE(IPLint32, value->pathingOrder, (value->pathingOrder >= 0)); \
                VALIDATE_IPLbool(value->enableValidation); \
                VALIDATE_IPLbool(value->findAlternatePaths); \
            } \
        } \
    } \
}

#define VALIDATE_IPLSimulationOutputs(value, flags) { \
    VALIDATE_POINTER(value); \
    if (value) { \
        if (flags & IPL_SIMULATIONFLAGS_DIRECT) { \
            VALIDATE_IPLDirectEffectParams((&value->direct)); \
        } \
        if (flags & IPL_SIMULATIONFLAGS_REFLECTIONS) { \
            VALIDATE_IPLReflectionEffectParams((&value->reflections)); \
        } \
        if (flags & IPL_SIMULATIONFLAGS_PATHING) { \
            VALIDATE_IPLPathEffectParams((&value->pathing)); \
        } \
    } \
}

#define VALIDATE_IPLDistanceAttenuationModel(value) { \
    VALIDATE_POINTER(value); \
    if (value) { \
        VALIDATE_IPLDistanceAttenuationModelType(value->type); \
        if (value->type == IPL_DISTANCEATTENUATIONTYPE_INVERSEDISTANCE) { \
            VALIDATE(IPLfloat32, value->minDistance, (value->minDistance > 0.0f)); \
        } \
        else if (value->type == IPL_DISTANCEATTENUATIONTYPE_CALLBACK) { \
            VALIDATE_POINTER(value->callback); \
        } \
        VALIDATE_IPLbool(value->dirty); \
    } \
}

#define VALIDATE_IPLAirAbsorptionModel(value) { \
    VALIDATE_POINTER(value); \
    if (value) { \
        VALIDATE_IPLAirAbsorptionModelType(value->type); \
        if (value->type == IPL_AIRABSORPTIONTYPE_EXPONENTIAL) { \
            VALIDATE_ARRAY_IPLfloat32(value->coefficients, 3); \
        } \
        else if (value->type == IPL_AIRABSORPTIONTYPE_CALLBACK) { \
            VALIDATE_POINTER(value->callback); \
        } \
        VALIDATE_IPLbool(value->dirty); \
    } \
}

#define VALIDATE_IPLDirectivity(value) { \
    VALIDATE_POINTER(value); \
    if (value) { \
        VALIDATE(IPLfloat32, value->dipoleWeight, (0.0f <= value->dipoleWeight && 1.0f <= value->dipoleWeight)); \
        VALIDATE(IPLfloat32, value->dipolePower, (value->dipolePower >= 0.0f)); \
    } \
}


// --------------------------------------------------------------------------------------------------------------------
// CValidatedContext
// --------------------------------------------------------------------------------------------------------------------

class CValidatedSerializedObject;
class CValidatedEmbreeDevice;
class CValidatedOpenCLDeviceList;
class CValidatedOpenCLDevice;
class CValidatedRadeonRaysDevice;
class CValidatedTrueAudioNextDevice;
class CValidatedScene;
class CValidatedStaticMesh;
class CValidatedInstancedMesh;
class CValidatedHRTF;
class CValidatedPanningEffect;
class CValidatedBinauralEffect;
class CValidatedVirtualSurroundEffect;
class CValidatedAmbisonicsEncodeEffect;
class CValidatedAmbisonicsPanningEffect;
class CValidatedAmbisonicsBinauralEffect;
class CValidatedAmbisonicsRotationEffect;
class CValidatedAmbisonicsDecodeEffect;
class CValidatedDirectEffect;
class CValidatedReflectionEffect;
class CValidatedReflectionMixer;
class CValidatedPathEffect;
class CValidatedProbeArray;
class CValidatedProbeBatch;
class CValidatedSimulator;
class CValidatedSource;

class CValidatedContext : public CContext
{
public:
    CValidatedContext(IPLContextSettings* settings)
        : CContext(settings)
    {}

    virtual void setProfilerContext(void* profilerContext) override
    {
        VALIDATE_POINTER(profilerContext);

        CContext::setProfilerContext(profilerContext);
    }

    virtual IPLVector3 calculateRelativeDirection(IPLVector3 sourcePosition, IPLVector3 listenerPosition, IPLVector3 listenerAhead, IPLVector3 listenerUp) override
    {
        VALIDATE_IPLVector3(sourcePosition);
        VALIDATE_IPLVector3(listenerPosition);
        VALIDATE_IPLVector3(listenerAhead);
        VALIDATE_IPLVector3(listenerUp);

        auto result = CContext::calculateRelativeDirection(sourcePosition, listenerPosition, listenerAhead, listenerUp);

        VALIDATE_IPLVector3(result);

        return result;
    }

    virtual IPLerror createSerializedObject(IPLSerializedObjectSettings* settings, ISerializedObject** serializedObject) override
    {
        VALIDATE_IPLSerializedObjectSettings(settings);
        VALIDATE_POINTER(serializedObject);

        return apiObjectAllocate<CValidatedSerializedObject, CContext, ISerializedObject>(serializedObject, this, settings);
    }

    virtual IPLerror createEmbreeDevice(IPLEmbreeDeviceSettings* settings, IEmbreeDevice** device) override
    {
        VALIDATE_IPLEmbreeDeviceSettings(settings);
        VALIDATE_POINTER(device);

        return apiObjectAllocate<CValidatedEmbreeDevice, CContext, IEmbreeDevice>(device, this, settings);
    }

    virtual IPLerror createOpenCLDeviceList(IPLOpenCLDeviceSettings* settings, IOpenCLDeviceList** deviceList) override
    {
        VALIDATE_IPLOpenCLDeviceSettings(settings);
        VALIDATE_POINTER(deviceList);

        return apiObjectAllocate<CValidatedOpenCLDeviceList, CContext, IOpenCLDeviceList>(deviceList, this, settings);
    }

    virtual IPLerror createOpenCLDevice(IOpenCLDeviceList* deviceList, IPLint32 index, IOpenCLDevice** device) override
    {
        VALIDATE_POINTER(deviceList);
        VALIDATE(IPLint32, index, (0 <= index && index < deviceList->getNumDevices()));
        VALIDATE_POINTER(device);

        return apiObjectAllocate<CValidatedOpenCLDevice, CContext, IOpenCLDevice>(device, this, deviceList, index);
    }

    virtual IPLerror createOpenCLDeviceFromExisting(void* convolutionQueue, void* irUpdateQueue, IOpenCLDevice** device) override
    {
        VALIDATE_POINTER(convolutionQueue);
        VALIDATE_POINTER(irUpdateQueue);
        VALIDATE_POINTER(device);

        return apiObjectAllocate<CValidatedOpenCLDevice, CContext, IOpenCLDevice>(device, this, convolutionQueue, irUpdateQueue);
    }

    virtual IPLerror createScene(IPLSceneSettings* settings, IScene** scene) override
    {
        VALIDATE_IPLSceneSettings(settings);
        VALIDATE_POINTER(scene);

        return apiObjectAllocate<CValidatedScene, CContext, IScene>(scene, this, settings);
    }

    virtual IPLerror loadScene(IPLSceneSettings* settings, ISerializedObject* serializedObject, IPLProgressCallback progressCallback, void* userData, IScene** scene) override
    {
        VALIDATE_IPLSceneSettings(settings);
        VALIDATE_POINTER(serializedObject);
        VALIDATE_POINTER(scene);

        return apiObjectAllocate<CValidatedScene, CContext, IScene>(scene, this, settings, serializedObject);
    }

    virtual IPLerror allocateAudioBuffer(IPLint32 numChannels, IPLint32 numSamples, IPLAudioBuffer* audioBuffer) override
    {
        VALIDATE(IPLint32, numChannels, (numChannels > 0));
        VALIDATE(IPLint32, numSamples, (numSamples > 0));
        VALIDATE_POINTER(audioBuffer);

        return CContext::allocateAudioBuffer(numChannels, numSamples, audioBuffer);
    }

    virtual void freeAudioBuffer(IPLAudioBuffer* audioBuffer) override
    {
        VALIDATE_POINTER(audioBuffer);

        CContext::freeAudioBuffer(audioBuffer);
    }

    virtual void interleaveAudioBuffer(IPLAudioBuffer* src, IPLfloat32* dst) override
    {
        VALIDATE_IPLAudioBuffer(src, true);
        VALIDATE_POINTER(dst);

        CContext::interleaveAudioBuffer(src, dst);

        VALIDATE_ARRAY_IPLfloat32(dst, src->numChannels * src->numSamples);
    }

    virtual void deinterleaveAudioBuffer(IPLfloat32* src, IPLAudioBuffer* dst) override
    {
        VALIDATE_POINTER(src);
        VALIDATE_IPLAudioBuffer(dst, false);
        VALIDATE_ARRAY_IPLfloat32(src, dst->numChannels * dst->numSamples);

        CContext::deinterleaveAudioBuffer(src, dst);

        VALIDATE_IPLAudioBuffer(dst, true);
    }

    virtual void mixAudioBuffer(IPLAudioBuffer* in, IPLAudioBuffer* mix) override
    {
        VALIDATE_IPLAudioBuffer(in, true);
        VALIDATE_IPLAudioBuffer(mix, false);
        VALIDATE(IPLint32, in->numChannels, (in->numChannels == mix->numChannels));
        VALIDATE(IPLint32, in->numSamples, (in->numSamples == mix->numSamples));

        CContext::mixAudioBuffer(in, mix);

        VALIDATE_IPLAudioBuffer(mix, true);
    }

    virtual void downmixAudioBuffer(IPLAudioBuffer* in, IPLAudioBuffer* out) override
    {
        VALIDATE_IPLAudioBuffer(in, true);
        VALIDATE_IPLAudioBuffer(out, false);
        VALIDATE(IPLint32, out->numChannels, (out->numChannels == 1));
        VALIDATE(IPLint32, in->numSamples, (in->numSamples == out->numSamples));

        CContext::downmixAudioBuffer(in, out);

        VALIDATE_IPLAudioBuffer(out, true);
    }

    virtual void convertAmbisonicAudioBuffer(IPLAmbisonicsType inType, IPLAmbisonicsType outType, IPLAudioBuffer* in, IPLAudioBuffer* out) override
    {
        VALIDATE_IPLAmbisonicsType(inType);
        VALIDATE_IPLAmbisonicsType(outType);
        VALIDATE_IPLAudioBuffer(in, true);
        VALIDATE_IPLAudioBuffer(out, false);
        VALIDATE(IPLint32, in->numChannels, (in->numChannels == out->numChannels));
        VALIDATE(IPLint32, in->numSamples, (in->numSamples == out->numSamples));

        CContext::convertAmbisonicAudioBuffer(inType, outType, in, out);

        VALIDATE_IPLAudioBuffer(out, true);
    }

    virtual IPLerror createHRTF(IPLAudioSettings* audioSettings, IPLHRTFSettings* hrtfSettings, IHRTF** hrtf) override
    {
        VALIDATE_IPLAudioSettings(audioSettings);
        VALIDATE_IPLHRTFSettings(hrtfSettings);
        VALIDATE_POINTER(hrtf);

        return apiObjectAllocate<CValidatedHRTF, CContext, IHRTF>(hrtf, this, audioSettings, hrtfSettings);
    }

    virtual IPLerror createPanningEffect(IPLAudioSettings* audioSettings, IPLPanningEffectSettings* effectSettings, IPanningEffect** effect) override
    {
        VALIDATE_IPLAudioSettings(audioSettings);
        VALIDATE_IPLPanningEffectSettings(effectSettings);
        VALIDATE_POINTER(effect);

        return apiObjectAllocate<CValidatedPanningEffect, CContext, IPanningEffect>(effect, this, audioSettings, effectSettings);
    }

    virtual IPLerror createBinauralEffect(IPLAudioSettings* audioSettings, IPLBinauralEffectSettings* effectSettings, IBinauralEffect** effect) override
    {
        VALIDATE_IPLAudioSettings(audioSettings);
        VALIDATE_IPLBinauralEffectSettings(effectSettings);
        VALIDATE_POINTER(effect);

        return apiObjectAllocate<CValidatedBinauralEffect, CContext, IBinauralEffect>(effect, this, audioSettings, effectSettings);
    }

    virtual IPLerror createVirtualSurroundEffect(IPLAudioSettings* audioSettings, IPLVirtualSurroundEffectSettings* effectSettings, IVirtualSurroundEffect** effect) override
    {
        VALIDATE_IPLAudioSettings(audioSettings);
        VALIDATE_IPLVirtualSurroundEffectSettings(effectSettings);
        VALIDATE_POINTER(effect);

        return apiObjectAllocate<CValidatedVirtualSurroundEffect, CContext, IVirtualSurroundEffect>(effect, this, audioSettings, effectSettings);
    }

    virtual IPLerror createAmbisonicsEncodeEffect(IPLAudioSettings* audioSettings, IPLAmbisonicsEncodeEffectSettings* effectSettings, IAmbisonicsEncodeEffect** effect) override
    {
        VALIDATE_IPLAudioSettings(audioSettings);
        VALIDATE_IPLAmbisonicsEncodeEffectSettings(effectSettings);
        VALIDATE_POINTER(effect);

        return apiObjectAllocate<CValidatedAmbisonicsEncodeEffect, CContext, IAmbisonicsEncodeEffect>(effect, this, audioSettings, effectSettings);
    }

    virtual IPLerror createAmbisonicsPanningEffect(IPLAudioSettings* audioSettings, IPLAmbisonicsPanningEffectSettings* effectSettings, IAmbisonicsPanningEffect** effect) override
    {
        VALIDATE_IPLAudioSettings(audioSettings);
        VALIDATE_IPLAmbisonicsPanningEffectSettings(effectSettings);
        VALIDATE_POINTER(effect);

        return apiObjectAllocate<CValidatedAmbisonicsPanningEffect, CContext, IAmbisonicsPanningEffect>(effect, this, audioSettings, effectSettings);
    }

    virtual IPLerror createAmbisonicsBinauralEffect(IPLAudioSettings* audioSettings, IPLAmbisonicsBinauralEffectSettings* effectSettings, IAmbisonicsBinauralEffect** effect) override
    {
        VALIDATE_IPLAudioSettings(audioSettings);
        VALIDATE_IPLAmbisonicsBinauralEffectSettings(effectSettings);
        VALIDATE_POINTER(effect);

        return apiObjectAllocate<CValidatedAmbisonicsBinauralEffect, CContext, IAmbisonicsBinauralEffect>(effect, this, audioSettings, effectSettings);
    }

    virtual IPLerror createAmbisonicsRotationEffect(IPLAudioSettings* audioSettings, IPLAmbisonicsRotationEffectSettings* effectSettings, IAmbisonicsRotationEffect** effect) override
    {
        VALIDATE_IPLAudioSettings(audioSettings);
        VALIDATE_IPLAmbisonicsRotationEffectSettings(effectSettings);
        VALIDATE_POINTER(effect);

        return apiObjectAllocate<CValidatedAmbisonicsRotationEffect, CContext, IAmbisonicsRotationEffect>(effect, this, audioSettings, effectSettings);
    }

    virtual IPLerror createAmbisonicsDecodeEffect(IPLAudioSettings* audioSettings, IPLAmbisonicsDecodeEffectSettings* effectSettings, IAmbisonicsDecodeEffect** effect) override
    {
        VALIDATE_IPLAudioSettings(audioSettings);
        VALIDATE_IPLAmbisonicsDecodeEffectSettings(effectSettings);
        VALIDATE_POINTER(effect);

        return apiObjectAllocate<CValidatedAmbisonicsDecodeEffect, CContext, IAmbisonicsDecodeEffect>(effect, this, audioSettings, effectSettings);
    }

    virtual IPLerror createDirectEffect(IPLAudioSettings* audioSettings, IPLDirectEffectSettings* effectSettings, IDirectEffect** effect) override
    {
        VALIDATE_IPLAudioSettings(audioSettings);
        VALIDATE_IPLDirectEffectSettings(effectSettings);
        VALIDATE_POINTER(effect);

        return apiObjectAllocate<CValidatedDirectEffect, CContext, IDirectEffect>(effect, this, audioSettings, effectSettings);
    }

    virtual IPLerror createReflectionEffect(IPLAudioSettings* audioSettings, IPLReflectionEffectSettings* effectSettings, IReflectionEffect** effect) override
    {
        VALIDATE_IPLAudioSettings(audioSettings);
        VALIDATE_IPLReflectionEffectSettings(effectSettings);
        VALIDATE_POINTER(effect);

        return apiObjectAllocate<CValidatedReflectionEffect, CContext, IReflectionEffect>(effect, this, audioSettings, effectSettings);
    }

    virtual IPLerror createReflectionMixer(IPLAudioSettings* audioSettings, IPLReflectionEffectSettings* effectSettings, IReflectionMixer** mixer) override
    {
        VALIDATE_IPLAudioSettings(audioSettings);
        VALIDATE_IPLReflectionEffectSettings(effectSettings);
        VALIDATE_POINTER(mixer);

        return apiObjectAllocate<CValidatedReflectionMixer, CContext, IReflectionMixer>(mixer, this, audioSettings, effectSettings);
    }

    virtual IPLerror createPathEffect(IPLAudioSettings* audioSettings, IPLPathEffectSettings* effectSettings, IPathEffect** effect) override
    {
        VALIDATE_IPLAudioSettings(audioSettings);
        VALIDATE_IPLPathEffectSettings(effectSettings);
        VALIDATE_POINTER(effect);

        return apiObjectAllocate<CValidatedPathEffect, CContext, IPathEffect>(effect, this, audioSettings, effectSettings);
    }

    virtual IPLerror createProbeArray(IProbeArray** probeArray) override
    {
        VALIDATE_POINTER(probeArray);

        return apiObjectAllocate<CValidatedProbeArray, CContext, IProbeArray>(probeArray, this);
    }

    virtual IPLerror createProbeBatch(IProbeBatch** probeBatch) override
    {
        VALIDATE_POINTER(probeBatch);

        return apiObjectAllocate<CValidatedProbeBatch, CContext, IProbeBatch>(probeBatch, this);
    }

    virtual IPLerror loadProbeBatch(ISerializedObject* serializedObject, IProbeBatch** probeBatch) override
    {
        VALIDATE_POINTER(serializedObject);
        VALIDATE_POINTER(probeBatch);

        return apiObjectAllocate<CValidatedProbeBatch, CContext, IProbeBatch>(probeBatch, this, serializedObject);
    }

    virtual void bakeReflections(IPLReflectionsBakeParams* params, IPLProgressCallback progressCallback, void* userData) override
    {
        VALIDATE_IPLReflectionsBakeParams(params);

        CContext::bakeReflections(params, progressCallback, userData);
    }

    virtual void bakePaths(IPLPathBakeParams* params, IPLProgressCallback progressCallback, void* userData) override
    {
        VALIDATE_IPLPathBakeParams(params);

        CContext::bakePaths(params, progressCallback, userData);
    }

    virtual IPLerror createSimulator(IPLSimulationSettings* settings, ISimulator** simulator) override
    {
        VALIDATE_IPLSimulationSettings(settings);
        VALIDATE_POINTER(simulator);

        return apiObjectAllocate<CValidatedSimulator, CContext, ISimulator>(simulator, this, settings);
    }

    virtual IPLfloat32 calculateDistanceAttenuation(IPLVector3 source, IPLVector3 listener, IPLDistanceAttenuationModel* model) override
    {
        VALIDATE_IPLVector3(source);
        VALIDATE_IPLVector3(listener);
        VALIDATE_IPLDistanceAttenuationModel(model);

        auto result = CContext::calculateDistanceAttenuation(source, listener, model);

        VALIDATE_IPLfloat32(result);

        return result;
    }

    virtual void calculateAirAbsorption(IPLVector3 source, IPLVector3 listener, IPLAirAbsorptionModel* model, IPLfloat32* airAbsorption) override
    {
        VALIDATE_IPLVector3(source);
        VALIDATE_IPLVector3(listener);
        VALIDATE_IPLAirAbsorptionModel(model);
        VALIDATE_POINTER(airAbsorption);

        CContext::calculateAirAbsorption(source, listener, model, airAbsorption);

        VALIDATE_ARRAY_IPLfloat32(airAbsorption, 3);
    }

    virtual IPLfloat32 calculateDirectivity(IPLCoordinateSpace3 source, IPLVector3 listener, IPLDirectivity* model) override
    {
        VALIDATE_IPLCoordinateSpace3(source);
        VALIDATE_IPLVector3(listener);
        VALIDATE_IPLDirectivity(model);

        auto result = CContext::calculateDirectivity(source, listener, model);

        VALIDATE_IPLfloat32(result);

        return result;
    }
};

IPLerror CContext::createContext(IPLContextSettings* settings,
                                 IContext** context)
{
    if (!settings || !context)
        return IPL_STATUS_FAILURE;

    if (!isVersionCompatible(settings->version))
        return IPL_STATUS_FAILURE;

    Context::sAPIVersion = settings->version;

    auto _allocateCallback = reinterpret_cast<AllocateCallback>(settings->allocateCallback);
    auto _freeCallback = reinterpret_cast<FreeCallback>(settings->freeCallback);
    Context::sMemory.init(_allocateCallback, _freeCallback);

    auto _enableValidation = false;
    if (Context::isCallerAPIVersionAtLeast(4, 5))
    {
        _enableValidation = (settings->flags & IPL_CONTEXTFLAGS_VALIDATION);
    }

    if (_enableValidation)
    {
        VALIDATE_IPLContextSettings(settings);

        try
        {
            auto _context = reinterpret_cast<CValidatedContext*>(gMemory().allocate(sizeof(CValidatedContext), Memory::kDefaultAlignment));
            new (_context) CValidatedContext(settings);
            *context = _context;
        }
        catch (Exception exception)
        {
            return static_cast<IPLerror>(exception.status());
        }
    }
    else
    {
        try
        {
            auto _context = reinterpret_cast<CContext*>(gMemory().allocate(sizeof(CContext), Memory::kDefaultAlignment));
            new (_context) CContext(settings);
            *context = _context;
        }
        catch (Exception exception)
        {
            return static_cast<IPLerror>(exception.status());
        }
    }

    return IPL_STATUS_SUCCESS;
}


// --------------------------------------------------------------------------------------------------------------------
// CValidatedSerializedObject
// --------------------------------------------------------------------------------------------------------------------

class CValidatedSerializedObject : public CSerializedObject
{
public:
    CValidatedSerializedObject(CContext* context, IPLSerializedObjectSettings* settings)
        : CSerializedObject(context, settings)
    {}
};


// --------------------------------------------------------------------------------------------------------------------
// CValidatedEmbreeDevice
// --------------------------------------------------------------------------------------------------------------------

class CValidatedEmbreeDevice : public CEmbreeDevice
{
public:
    CValidatedEmbreeDevice(CContext* context, IPLEmbreeDeviceSettings* settings)
        : CEmbreeDevice(context, settings)
    {}
};


// --------------------------------------------------------------------------------------------------------------------
// CValidatedOpenCLDeviceList
// --------------------------------------------------------------------------------------------------------------------

class CValidatedOpenCLDeviceList : public COpenCLDeviceList
{
public:
    CValidatedOpenCLDeviceList(CContext* context, IPLOpenCLDeviceSettings* settings)
        : COpenCLDeviceList(context, settings)
    {}

    virtual IPLint32 getNumDevices() override
    {
        auto result = COpenCLDeviceList::getNumDevices();

        VALIDATE(IPLint32, result, (result >= 0));

        return result;
    }

    virtual void getDeviceDesc(IPLint32 index, IPLOpenCLDeviceDesc* deviceDesc) override
    {
        VALIDATE(IPLint32, index, (0 <= index && index < getNumDevices()));
        VALIDATE_POINTER(deviceDesc);

        COpenCLDeviceList::getDeviceDesc(index, deviceDesc);
    }
};


// --------------------------------------------------------------------------------------------------------------------
// CValidatedOpenCLDevice
// --------------------------------------------------------------------------------------------------------------------

class CValidatedOpenCLDevice : public COpenCLDevice
{
public:
    CValidatedOpenCLDevice(CContext* context, IOpenCLDeviceList* deviceList, IPLint32 index)
        : COpenCLDevice(context, deviceList, index)
    {}

    CValidatedOpenCLDevice(CContext* context, void* convolutionQueue, void* irUpdateQueue)
        : COpenCLDevice(context, convolutionQueue, irUpdateQueue)
    {}

    virtual IPLerror createRadeonRaysDevice(IPLRadeonRaysDeviceSettings* settings, IRadeonRaysDevice** device) override
    {
        VALIDATE_IPLRadeonRaysDeviceSettings(settings);
        VALIDATE_POINTER(device);

        return apiObjectAllocate<CValidatedRadeonRaysDevice, COpenCLDevice, IRadeonRaysDevice>(device, this, settings);
    }

    virtual IPLerror createTrueAudioNextDevice(IPLTrueAudioNextDeviceSettings* settings, ITrueAudioNextDevice** device) override
    {
        VALIDATE_IPLTrueAudioNextDeviceSettings(settings);
        VALIDATE_POINTER(device);

        return apiObjectAllocate<CValidatedTrueAudioNextDevice, COpenCLDevice, ITrueAudioNextDevice>(device, this, settings);
    }
};


// --------------------------------------------------------------------------------------------------------------------
// CValidatedRadeonRaysDevice
// --------------------------------------------------------------------------------------------------------------------

class CValidatedRadeonRaysDevice : public CRadeonRaysDevice
{
public:
    CValidatedRadeonRaysDevice(COpenCLDevice* openCLDevice, IPLRadeonRaysDeviceSettings* settings)
        : CRadeonRaysDevice(openCLDevice, settings)
    {}
};


// --------------------------------------------------------------------------------------------------------------------
// CValidatedTrueAudioNextDevice
// --------------------------------------------------------------------------------------------------------------------

class CValidatedTrueAudioNextDevice : public CTrueAudioNextDevice
{
public:
    CValidatedTrueAudioNextDevice(COpenCLDevice* openCLDevice, IPLTrueAudioNextDeviceSettings* settings)
        : CTrueAudioNextDevice(openCLDevice, settings)
    {}
};


// --------------------------------------------------------------------------------------------------------------------
// CValidatedScene
// --------------------------------------------------------------------------------------------------------------------

class CValidatedScene : public CScene
{
public:
    CValidatedScene(CContext* context, IPLSceneSettings* settings)
        : CScene(context, settings)
    {}

    CValidatedScene(CContext* context, IPLSceneSettings* settings, ISerializedObject* serializedObject)
        : CScene(context, settings, serializedObject)
    {}

    virtual void save(ISerializedObject* serializedObject) override
    {
        VALIDATE_POINTER(serializedObject);

        CScene::save(serializedObject);
    }

    virtual void saveOBJ(IPLstring fileBaseName) override
    {
        VALIDATE_POINTER(fileBaseName);

        CScene::saveOBJ(fileBaseName);
    }

    virtual IPLerror createStaticMesh(IPLStaticMeshSettings* settings, IStaticMesh** staticMesh) override
    {
        VALIDATE_IPLStaticMeshSettings(settings);
        VALIDATE_POINTER(staticMesh);

        return apiObjectAllocate<CValidatedStaticMesh, CScene, IStaticMesh>(staticMesh, this, settings);
    }

    virtual IPLerror loadStaticMesh(ISerializedObject* serializedObject, IPLProgressCallback progressCallback, void* userData, IStaticMesh** staticMesh) override
    {
        VALIDATE_POINTER(serializedObject);
        VALIDATE_POINTER(staticMesh);

        return apiObjectAllocate<CValidatedStaticMesh, CScene, IStaticMesh>(staticMesh, this, serializedObject);
    }

    virtual IPLerror createInstancedMesh(IPLInstancedMeshSettings* settings, IInstancedMesh** instancedMesh) override
    {
        VALIDATE_IPLInstancedMeshSettings(settings);
        VALIDATE_POINTER(instancedMesh);

        return apiObjectAllocate<CValidatedInstancedMesh, CScene, IInstancedMesh>(instancedMesh, this, settings);
    }
};


// --------------------------------------------------------------------------------------------------------------------
// CValidatedStaticMesh
// --------------------------------------------------------------------------------------------------------------------

class CValidatedStaticMesh : public CStaticMesh
{
public:
    CValidatedStaticMesh(CScene* scene, IPLStaticMeshSettings* settings)
        : CStaticMesh(scene, settings)
    {}

    CValidatedStaticMesh(CScene* scene, ISerializedObject* serializedObject)
        : CStaticMesh(scene, serializedObject)
    {}

    virtual void save(ISerializedObject* serializedObject) override
    {
        VALIDATE_POINTER(serializedObject);

        CStaticMesh::save(serializedObject);
    }

    virtual void add(IScene* scene) override
    {
        VALIDATE_POINTER(scene);

        CStaticMesh::add(scene);
    }

    virtual void remove(IScene* scene) override
    {
        VALIDATE_POINTER(scene);

        CStaticMesh::remove(scene);
    }
};


// --------------------------------------------------------------------------------------------------------------------
// CValidatedInstancedMesh
// --------------------------------------------------------------------------------------------------------------------

class CValidatedInstancedMesh : public CInstancedMesh
{
public:
    CValidatedInstancedMesh(CScene* scene, IPLInstancedMeshSettings* settings)
        : CInstancedMesh(scene, settings)
    {}

    virtual void add(IScene* scene) override
    {
        VALIDATE_POINTER(scene);

        CInstancedMesh::add(scene);
    }

    virtual void remove(IScene* scene) override
    {
        VALIDATE_POINTER(scene);

        CInstancedMesh::remove(scene);
    }

    virtual void updateTransform(IScene* scene, IPLMatrix4x4 transform) override
    {
        VALIDATE_POINTER(scene);
        VALIDATE_IPLMatrix4x4(transform);

        CInstancedMesh::updateTransform(scene, transform);
    }
};


// --------------------------------------------------------------------------------------------------------------------
// CValidatedHRTF
// --------------------------------------------------------------------------------------------------------------------

class CValidatedHRTF : public CHRTF
{
public:
    CValidatedHRTF(CContext* context, IPLAudioSettings* audioSettings, IPLHRTFSettings* hrtfSettings)
        : CHRTF(context, audioSettings, hrtfSettings)
    {}
};


// --------------------------------------------------------------------------------------------------------------------
// CValidatedPanningEffect
// --------------------------------------------------------------------------------------------------------------------

class CValidatedPanningEffect : public CPanningEffect
{
public:
    CValidatedPanningEffect(CContext* context, IPLAudioSettings* audioSettings, IPLPanningEffectSettings* effectSettings)
        : CPanningEffect(context, audioSettings, effectSettings)
    {}

    virtual IPLAudioEffectState apply(IPLPanningEffectParams* params, IPLAudioBuffer* in, IPLAudioBuffer* out) override
    {
        VALIDATE_IPLPanningEffectParams(params);
        VALIDATE_IPLAudioBuffer(in, true);
        VALIDATE_IPLAudioBuffer(out, false);

        auto result = CPanningEffect::apply(params, in, out);

        VALIDATE_IPLAudioEffectState(result);
        VALIDATE_IPLAudioBuffer(out, true);

        return result;
    }
};


// --------------------------------------------------------------------------------------------------------------------
// CValidatedBinauralEffect
// --------------------------------------------------------------------------------------------------------------------

class CValidatedBinauralEffect : public CBinauralEffect
{
public:
    CValidatedBinauralEffect(CContext* context, IPLAudioSettings* audioSettings, IPLBinauralEffectSettings* effectSettings)
        : CBinauralEffect(context, audioSettings, effectSettings)
    {}

    virtual IPLAudioEffectState apply(IPLBinauralEffectParams* params, IPLAudioBuffer* in, IPLAudioBuffer* out) override
    {
        VALIDATE_IPLBinauralEffectParams(params);
        VALIDATE_IPLAudioBuffer(in, true);
        VALIDATE_IPLAudioBuffer(out, false);

        auto result = CBinauralEffect::apply(params, in, out);

        VALIDATE_IPLAudioEffectState(result);
        VALIDATE_IPLAudioBuffer(out, true);

        return result;
    }
};


// --------------------------------------------------------------------------------------------------------------------
// CValidatedVirtualSurroundEffect
// --------------------------------------------------------------------------------------------------------------------

class CValidatedVirtualSurroundEffect : public CVirtualSurroundEffect
{
public:
    CValidatedVirtualSurroundEffect(CContext* context, IPLAudioSettings* audioSettings, IPLVirtualSurroundEffectSettings* effectSettings)
        : CVirtualSurroundEffect(context, audioSettings, effectSettings)
    {}

    virtual IPLAudioEffectState apply(IPLVirtualSurroundEffectParams* params, IPLAudioBuffer* in, IPLAudioBuffer* out) override
    {
        VALIDATE_IPLVirtualSurroundEffectParams(params);
        VALIDATE_IPLAudioBuffer(in, true);
        VALIDATE_IPLAudioBuffer(out, false);

        auto result = CVirtualSurroundEffect::apply(params, in, out);

        VALIDATE_IPLAudioEffectState(result);
        VALIDATE_IPLAudioBuffer(out, true);

        return result;
    }
};


// --------------------------------------------------------------------------------------------------------------------
// CValidatedAmbisonicsEncodeEffect
// --------------------------------------------------------------------------------------------------------------------

class CValidatedAmbisonicsEncodeEffect : public CAmbisonicsEncodeEffect
{
public:
    CValidatedAmbisonicsEncodeEffect(CContext* context, IPLAudioSettings* audioSettings, IPLAmbisonicsEncodeEffectSettings* effectSettings)
        : CAmbisonicsEncodeEffect(context, audioSettings, effectSettings)
    {}

    virtual IPLAudioEffectState apply(IPLAmbisonicsEncodeEffectParams* params, IPLAudioBuffer* in, IPLAudioBuffer* out) override
    {
        VALIDATE_IPLAmbisonicsEncodeEffectParams(params);
        VALIDATE_IPLAudioBuffer(in, true);
        VALIDATE_IPLAudioBuffer(out, false);

        auto result = CAmbisonicsEncodeEffect::apply(params, in, out);

        VALIDATE_IPLAudioEffectState(result);
        VALIDATE_IPLAudioBuffer(out, true);

        return result;
    }
};


// --------------------------------------------------------------------------------------------------------------------
// CValidatedAmbisonicsPanningEffect
// --------------------------------------------------------------------------------------------------------------------

class CValidatedAmbisonicsPanningEffect : public CAmbisonicsPanningEffect
{
public:
    CValidatedAmbisonicsPanningEffect(CContext* context, IPLAudioSettings* audioSettings, IPLAmbisonicsPanningEffectSettings* effectSettings)
        : CAmbisonicsPanningEffect(context, audioSettings, effectSettings)
    {}

    virtual IPLAudioEffectState apply(IPLAmbisonicsPanningEffectParams* params, IPLAudioBuffer* in, IPLAudioBuffer* out) override
    {
        VALIDATE_IPLAmbisonicsPanningEffectParams(params);
        VALIDATE_IPLAudioBuffer(in, true);
        VALIDATE_IPLAudioBuffer(out, false);

        auto result = CAmbisonicsPanningEffect::apply(params, in, out);

        VALIDATE_IPLAudioEffectState(result);
        VALIDATE_IPLAudioBuffer(out, true);

        return result;
    }
};


// --------------------------------------------------------------------------------------------------------------------
// CValidatedAmbisonicsBinauralEffect
// --------------------------------------------------------------------------------------------------------------------

class CValidatedAmbisonicsBinauralEffect : public CAmbisonicsBinauralEffect
{
public:
    CValidatedAmbisonicsBinauralEffect(CContext* context, IPLAudioSettings* audioSettings, IPLAmbisonicsBinauralEffectSettings* effectSettings)
        : CAmbisonicsBinauralEffect(context, audioSettings, effectSettings)
    {}

    virtual IPLAudioEffectState apply(IPLAmbisonicsBinauralEffectParams* params, IPLAudioBuffer* in, IPLAudioBuffer* out) override
    {
        VALIDATE_IPLAmbisonicsBinauralEffectParams(params);
        VALIDATE_IPLAudioBuffer(in, true);
        VALIDATE_IPLAudioBuffer(out, false);

        auto result = CAmbisonicsBinauralEffect::apply(params, in, out);

        VALIDATE_IPLAudioEffectState(result);
        VALIDATE_IPLAudioBuffer(out, true);

        return result;
    }
};


// --------------------------------------------------------------------------------------------------------------------
// CValidatedAmbisonicsRotationEffect
// --------------------------------------------------------------------------------------------------------------------

class CValidatedAmbisonicsRotationEffect : public CAmbisonicsRotationEffect
{
public:
    CValidatedAmbisonicsRotationEffect(CContext* context, IPLAudioSettings* audioSettings, IPLAmbisonicsRotationEffectSettings* effectSettings)
        : CAmbisonicsRotationEffect(context, audioSettings, effectSettings)
    {}

    virtual IPLAudioEffectState apply(IPLAmbisonicsRotationEffectParams* params, IPLAudioBuffer* in, IPLAudioBuffer* out) override
    {
        VALIDATE_IPLAmbisonicsRotationEffectParams(params);
        VALIDATE_IPLAudioBuffer(in, true);
        VALIDATE_IPLAudioBuffer(out, false);

        auto result = CAmbisonicsRotationEffect::apply(params, in, out);

        VALIDATE_IPLAudioEffectState(result);
        VALIDATE_IPLAudioBuffer(out, true);

        return result;
    }
};


// --------------------------------------------------------------------------------------------------------------------
// CValidatedAmbisonicsDecodeEffect
// --------------------------------------------------------------------------------------------------------------------

class CValidatedAmbisonicsDecodeEffect : public CAmbisonicsDecodeEffect
{
public:
    CValidatedAmbisonicsDecodeEffect(CContext* context, IPLAudioSettings* audioSettings, IPLAmbisonicsDecodeEffectSettings* effectSettings)
        : CAmbisonicsDecodeEffect(context, audioSettings, effectSettings)
    {}

    virtual IPLAudioEffectState apply(IPLAmbisonicsDecodeEffectParams* params, IPLAudioBuffer* in, IPLAudioBuffer* out) override
    {
        VALIDATE_IPLAmbisonicsDecodeEffectParams(params);
        VALIDATE_IPLAudioBuffer(in, true);
        VALIDATE_IPLAudioBuffer(out, false);

        auto result = CAmbisonicsDecodeEffect::apply(params, in, out);

        VALIDATE_IPLAudioEffectState(result);
        VALIDATE_IPLAudioBuffer(out, true);

        return result;
    }
};


// --------------------------------------------------------------------------------------------------------------------
// CValidatedDirectEffect
// --------------------------------------------------------------------------------------------------------------------

class CValidatedDirectEffect : public CDirectEffect
{
public:
    CValidatedDirectEffect(CContext* context, IPLAudioSettings* audioSettings, IPLDirectEffectSettings* effectSettings)
        : CDirectEffect(context, audioSettings, effectSettings)
    {}

    virtual IPLAudioEffectState apply(IPLDirectEffectParams* params, IPLAudioBuffer* in, IPLAudioBuffer* out) override
    {
        VALIDATE_IPLDirectEffectParams(params);
        VALIDATE_IPLAudioBuffer(in, true);
        VALIDATE_IPLAudioBuffer(out, false);

        auto result = CDirectEffect::apply(params, in, out);

        VALIDATE_IPLAudioEffectState(result);
        VALIDATE_IPLAudioBuffer(out, true);

        return result;
    }
};


// --------------------------------------------------------------------------------------------------------------------
// CValidatedReflectionEffect
// --------------------------------------------------------------------------------------------------------------------

class CValidatedReflectionEffect : public CReflectionEffect
{
public:
    CValidatedReflectionEffect(CContext* context, IPLAudioSettings* audioSettings, IPLReflectionEffectSettings* effectSettings)
        : CReflectionEffect(context, audioSettings, effectSettings)
    {}

    virtual IPLAudioEffectState apply(IPLReflectionEffectParams* params, IPLAudioBuffer* in, IPLAudioBuffer* out, IReflectionMixer* mixer) override
    {
        VALIDATE_IPLReflectionEffectParams(params);
        VALIDATE_IPLAudioBuffer(in, true);
        if (out)
        {
            VALIDATE_IPLAudioBuffer(out, false);
        }
        else
        {
            VALIDATE_POINTER(mixer);
        }

        auto result = CReflectionEffect::apply(params, in, out, mixer);

        VALIDATE_IPLAudioEffectState(result);

        if (out)
        {
            VALIDATE_IPLAudioBuffer(out, true);
        }

        return result;
    }
};

class CValidatedReflectionMixer : public CReflectionMixer
{
public:
    CValidatedReflectionMixer(CContext* context, IPLAudioSettings* audioSettings, IPLReflectionEffectSettings* effectSettings)
        : CReflectionMixer(context, audioSettings, effectSettings)
    {}

    virtual IPLAudioEffectState apply(IPLReflectionEffectParams* params, IPLAudioBuffer* out) override
    {
        VALIDATE_IPLReflectionEffectParams(params);
        VALIDATE_IPLAudioBuffer(out, false);

        auto result = CReflectionMixer::apply(params, out);

        VALIDATE_IPLAudioEffectState(result);
        VALIDATE_IPLAudioBuffer(out, true);

        return result;
    }
};


// --------------------------------------------------------------------------------------------------------------------
// CValidatedPathEffect
// --------------------------------------------------------------------------------------------------------------------

class CValidatedPathEffect : public CPathEffect
{
public:
    CValidatedPathEffect(CContext* context, IPLAudioSettings* audioSettings, IPLPathEffectSettings* effectSettings)
        : CPathEffect(context, audioSettings, effectSettings)
    {}

    virtual IPLAudioEffectState apply(IPLPathEffectParams* params, IPLAudioBuffer* in, IPLAudioBuffer* out) override
    {
        VALIDATE_IPLPathEffectParams(params);
        VALIDATE_IPLAudioBuffer(in, true);
        VALIDATE_IPLAudioBuffer(out, false);

        auto result = CPathEffect::apply(params, in, out);

        VALIDATE_IPLAudioEffectState(result);
        VALIDATE_IPLAudioBuffer(out, true);

        return result;
    }
};


// --------------------------------------------------------------------------------------------------------------------
// CValidatedProbeArray
// --------------------------------------------------------------------------------------------------------------------

class CValidatedProbeArray : public CProbeArray
{
public:
    CValidatedProbeArray(CContext* context)
        : CProbeArray(context)
    {}

    virtual void generateProbes(IScene* scene, IPLProbeGenerationParams* params) override
    {
        VALIDATE_POINTER(scene);
        VALIDATE_IPLProbeGenerationParams(params);

        CProbeArray::generateProbes(scene, params);
    }

    virtual IPLint32 getNumProbes() override
    {
        auto result = CProbeArray::getNumProbes();

        VALIDATE(IPLint32, result, (result >= 0));

        return result;
    }

    virtual IPLSphere getProbe(IPLint32 index) override
    {
        VALIDATE(IPLint32, index, (0 <= index && index < getNumProbes()));

        auto result = CProbeArray::getProbe(index);

        VALIDATE_IPLSphere(result);

        return result;
    }
};


// --------------------------------------------------------------------------------------------------------------------
// CValidatedProbeBatch
// --------------------------------------------------------------------------------------------------------------------

class CValidatedProbeBatch : public CProbeBatch
{
public:
    CValidatedProbeBatch(CContext* context)
        : CProbeBatch(context)
    {}

    CValidatedProbeBatch(CContext* context, ISerializedObject* serializedObject)
        : CProbeBatch(context, serializedObject)
    {}

    virtual void save(ISerializedObject* serializedObject) override
    {
        VALIDATE_POINTER(serializedObject);

        CProbeBatch::save(serializedObject);
    }

    virtual IPLint32 getNumProbes() override
    {
        auto result = CProbeBatch::getNumProbes();

        VALIDATE(IPLint32, result, (result >= 0));

        return result;
    }

    virtual void addProbe(IPLSphere probe) override
    {
        VALIDATE_IPLSphere(probe);

        CProbeBatch::addProbe(probe);
    }

    virtual void addProbeArray(IProbeArray* probeArray) override
    {
        VALIDATE_POINTER(probeArray);

        CProbeBatch::addProbeArray(probeArray);
    }

    virtual void removeProbe(IPLint32 index) override
    {
        VALIDATE(IPLint32, index, (0 <= index && index < getNumProbes()));

        CProbeBatch::removeProbe(index);
    }

    virtual void removeData(IPLBakedDataIdentifier* identifier) override
    {
        VALIDATE_POINTER(identifier);
        if (identifier)
        {
            VALIDATE_IPLBakedDataIdentifier((*identifier));
        }

        CProbeBatch::removeData(identifier);
    }

    virtual IPLsize getDataSize(IPLBakedDataIdentifier* identifier) override
    {
        VALIDATE_POINTER(identifier);
        if (identifier)
        {
            VALIDATE_IPLBakedDataIdentifier((*identifier));
        }

        auto result = CProbeBatch::getDataSize(identifier);

        VALIDATE_IPLsize(result);

        return result;
    }
};


// --------------------------------------------------------------------------------------------------------------------
// CValidatedSimulator
// --------------------------------------------------------------------------------------------------------------------

class CValidatedSimulator : public CSimulator
{
public:
    CValidatedSimulator(CContext* context, IPLSimulationSettings* settings)
        : CSimulator(context, settings)
    {}

    virtual void setScene(IScene* scene) override
    {
        VALIDATE_POINTER(scene);

        CSimulator::setScene(scene);
    }

    virtual void addProbeBatch(IProbeBatch* probeBatch) override
    {
        VALIDATE_POINTER(probeBatch);

        CSimulator::addProbeBatch(probeBatch);
    }

    virtual void removeProbeBatch(IProbeBatch* probeBatch) override
    {
        VALIDATE_POINTER(probeBatch);

        CSimulator::removeProbeBatch(probeBatch);
    }

    virtual void setSharedInputs(IPLSimulationFlags flags, IPLSimulationSharedInputs* sharedInputs) override
    {
        VALIDATE_IPLSimulationFlags(flags);
        VALIDATE_IPLSimulationSharedInputs(sharedInputs, flags);

        CSimulator::setSharedInputs(flags, sharedInputs);
    }

    virtual IPLerror createSource(IPLSourceSettings* settings, ISource** source) override
    {
        VALIDATE_IPLSourceSettings(settings);
        VALIDATE_POINTER(source);

        return apiObjectAllocate<CValidatedSource, CSimulator, ISource>(source, this, settings);
    }
};


// --------------------------------------------------------------------------------------------------------------------
// CValidatedSource
// --------------------------------------------------------------------------------------------------------------------

class CValidatedSource : public CSource
{
public:
    CValidatedSource(CSimulator* simulator, IPLSourceSettings* settings)
        : CSource(simulator, settings)
    {}

    virtual void add(ISimulator* simulator) override
    {
        VALIDATE_POINTER(simulator);

        CSource::add(simulator);
    }

    virtual void remove(ISimulator* simulator) override
    {
        VALIDATE_POINTER(simulator);

        CSource::remove(simulator);
    }

    virtual void setInputs(IPLSimulationFlags flags, IPLSimulationInputs* inputs) override
    {
        VALIDATE_IPLSimulationFlags(flags);
        VALIDATE_IPLSimulationInputs(inputs);

        CSource::setInputs(flags, inputs);
    }

    virtual void getOutputs(IPLSimulationFlags flags, IPLSimulationOutputs* outputs) override
    {
        VALIDATE_IPLSimulationFlags(flags);
        VALIDATE_POINTER(outputs);

        CSource::getOutputs(flags, outputs);

        VALIDATE_IPLSimulationOutputs(outputs, flags);
    }
};

}
