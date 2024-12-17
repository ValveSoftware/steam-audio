/*******************************************************************************
The content of this file includes portions of the AUDIOKINETIC Wwise Technology
released in source code form as part of the SDK installer package.

Commercial License Usage

Licensees holding valid commercial licenses to the AUDIOKINETIC Wwise Technology
may use this file in accordance with the end user license agreement provided
with the software or, alternatively, in accordance with the terms contained in a
written agreement between you and Audiokinetic Inc.

Apache License Usage

Alternatively, this file may be used under the Apache License, Version 2.0 (the
"Apache License"); you may not use this file except in compliance with the
Apache License. You may obtain a copy of the Apache License at
http://www.apache.org/licenses/LICENSE-2.0.

Unless required by applicable law or agreed to in writing, software distributed
under the Apache License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES
OR CONDITIONS OF ANY KIND, either express or implied. See the Apache License for
the specific language governing permissions and limitations under the License.

  Copyright (c) 2024 Audiokinetic Inc.
*******************************************************************************/

#include "SteamAudioSpatializerFXParams.h"

#include <AK/Tools/Common/AkBankReadHelpers.h>


SteamAudioSpatializerFXParams::SteamAudioSpatializerFXParams() = default;

SteamAudioSpatializerFXParams::SteamAudioSpatializerFXParams(const SteamAudioSpatializerFXParams& other)
    : RTPC(other.RTPC)
    , NonRTPC(other.NonRTPC)
{
    m_paramChangeHandler.SetAllParamChanges();
}

SteamAudioSpatializerFXParams::~SteamAudioSpatializerFXParams() = default;

AK::IAkPluginParam* SteamAudioSpatializerFXParams::Clone(AK::IAkPluginMemAlloc* in_pAllocator)
{
    return AK_PLUGIN_NEW(in_pAllocator, SteamAudioSpatializerFXParams(*this));
}

AKRESULT SteamAudioSpatializerFXParams::Init(AK::IAkPluginMemAlloc* in_pAllocator, const void* in_pParamsBlock, AkUInt32 in_uBlockSize)
{
    if (in_uBlockSize == 0)
    {
        NonRTPC.directBinaural = true;
        NonRTPC.hrtfInterpolation = static_cast<AkInt16>(IPL_HRTFINTERPOLATION_NEAREST);
        NonRTPC.distanceAttenuation = false;
        NonRTPC.airAbsorption = false;
        NonRTPC.directivity = false;
        NonRTPC.occlusion = STEAMAUDIO_PARAM_OFF;
        NonRTPC.transmission = STEAMAUDIO_PARAM_OFF;
        NonRTPC.transmissionType = IPL_TRANSMISSIONTYPE_FREQINDEPENDENT;
        NonRTPC.reflections = false;
        NonRTPC.reflectionsBinaural = false;
        NonRTPC.pathing = false;
        NonRTPC.pathingBinaural = false;

        RTPC.pos[0] = 1.f;
        RTPC.pos[1] = 0.f;
        RTPC.pos[2] = 0.f;
        RTPC.occlusionValue = 1.0f;
        RTPC.transmissionValue[0] = 1.0f;
        RTPC.transmissionValue[1] = 1.0f;
        RTPC.transmissionValue[2] = 1.0f;
        RTPC.dipoleWeight = 0.0f;
        RTPC.dipolePower = 0.0f;
        RTPC.directMixLevel = 1.0f;
        RTPC.reflectionsMixLevel = 1.0f;
        RTPC.pathingMixLevel = 1.0f;

        m_paramChangeHandler.SetAllParamChanges();
        
        return AK_Success;
    }
    else
    {
        return SetParamsBlock(in_pParamsBlock, in_uBlockSize);
    }
}

AKRESULT SteamAudioSpatializerFXParams::Term(AK::IAkPluginMemAlloc* in_pAllocator)
{
    AK_PLUGIN_DELETE(in_pAllocator, this);
    return AK_Success;
}

AKRESULT SteamAudioSpatializerFXParams::SetParamsBlock(const void* in_pParamsBlock, AkUInt32 in_uBlockSize)
{
    auto result = AK_Success;

    auto* block = (AkUInt8*) in_pParamsBlock;

    NonRTPC.occlusion = READBANKDATA(AkInt16, block, in_uBlockSize);
    RTPC.occlusionValue = READBANKDATA(AkReal32, block, in_uBlockSize);
    NonRTPC.transmission = READBANKDATA(AkInt16, block, in_uBlockSize);
    NonRTPC.transmissionType = (IPLTransmissionType) READBANKDATA(AkInt16, block, in_uBlockSize);
    RTPC.transmissionValue[0] = READBANKDATA(AkReal32, block, in_uBlockSize);
    RTPC.transmissionValue[1] = READBANKDATA(AkReal32, block, in_uBlockSize);
    RTPC.transmissionValue[2] = READBANKDATA(AkReal32, block, in_uBlockSize);
    NonRTPC.directBinaural = READBANKDATA(bool, block, in_uBlockSize);
    RTPC.pos[0] = READBANKDATA(AkReal32, block, in_uBlockSize);
    RTPC.pos[1] = READBANKDATA(AkReal32, block, in_uBlockSize);
    RTPC.pos[2] = READBANKDATA(AkReal32, block, in_uBlockSize);
    NonRTPC.hrtfInterpolation = READBANKDATA(AkInt16, block, in_uBlockSize);
    NonRTPC.distanceAttenuation = READBANKDATA(bool, block, in_uBlockSize);
    NonRTPC.airAbsorption = READBANKDATA(bool, block, in_uBlockSize);
    NonRTPC.directivity = READBANKDATA(bool, block, in_uBlockSize);
    RTPC.dipoleWeight = READBANKDATA(AkReal32, block, in_uBlockSize);
    RTPC.dipolePower = READBANKDATA(AkReal32, block, in_uBlockSize);
    RTPC.directMixLevel = READBANKDATA(AkReal32, block, in_uBlockSize);
    NonRTPC.reflections = READBANKDATA(bool, block, in_uBlockSize);
    NonRTPC.reflectionsBinaural = READBANKDATA(bool, block, in_uBlockSize);
    RTPC.reflectionsMixLevel = READBANKDATA(AkReal32, block, in_uBlockSize);
    NonRTPC.pathing = READBANKDATA(bool, block, in_uBlockSize);
    NonRTPC.pathingBinaural = READBANKDATA(bool, block, in_uBlockSize);
    RTPC.pathingMixLevel = READBANKDATA(AkReal32, block, in_uBlockSize);

    CHECKBANKDATASIZE(in_uBlockSize, result);

    m_paramChangeHandler.SetAllParamChanges();

    return result;
}

AKRESULT SteamAudioSpatializerFXParams::SetParam(AkPluginParamID in_paramID, const void* in_pValue, AkUInt32 in_uParamSize)
{
    // NOTE: RTPC parameters are always sent as AkReal32.

    switch (in_paramID)
    {
    case SPATIALIZER_PARAM_OCCLUSION:
        NonRTPC.occlusion = (AkInt16) *((AkInt16*) in_pValue);
        break;
    case SPATIALIZER_PARAM_OCCLUSIONVALUE:
        RTPC.occlusionValue = *((AkReal32*) in_pValue);
        break;
    case SPATIALIZER_PARAM_TRANSMISSION:
        NonRTPC.transmission = (AkInt16) * ((AkInt16*) in_pValue);
        break;
    case SPATIALIZER_PARAM_TRANSMISSIONTYPE:
        NonRTPC.transmissionType = (IPLTransmissionType) * ((AkInt16*) in_pValue);
        break;
    case SPATIALIZER_PARAM_TRANSMISSIONLOW:
        RTPC.transmissionValue[0] = *((AkReal32*) in_pValue);
        break;
    case SPATIALIZER_PARAM_TRANSMISSIONMID:
        RTPC.transmissionValue[1] = *((AkReal32*) in_pValue);
        break;
    case SPATIALIZER_PARAM_TRANSMISSIONHIGH:
        RTPC.transmissionValue[2] = *((AkReal32*) in_pValue);
        break;
    case SPATIALIZER_PARAM_DIRECTBINAURAL:
        NonRTPC.directBinaural = *((bool*)in_pValue);
        break;
    case SPATIALIZER_PARAM_POSITION_X:
        RTPC.pos[0] = *((AkReal32*)in_pValue);
        break;
	case SPATIALIZER_PARAM_POSITION_Y:
		RTPC.pos[1] = *((AkReal32*)in_pValue);
		break;
	case SPATIALIZER_PARAM_POSITION_Z:
		RTPC.pos[2] = *((AkReal32*)in_pValue);
		break;
    case SPATIALIZER_PARAM_HRTFINTERPOLATION:
        NonRTPC.hrtfInterpolation = (AkInt16) *((AkInt16*) in_pValue);
        break;
    case SPATIALIZER_PARAM_DISTANCEATTENUATION:
        NonRTPC.distanceAttenuation = (bool) *((bool*) in_pValue);
        break;
    case SPATIALIZER_PARAM_AIRABSORPTION:
        NonRTPC.airAbsorption = (bool) *((bool*) in_pValue);
        break;
    case SPATIALIZER_PARAM_DIRECTIVITY:
        NonRTPC.directivity = (bool) *((bool*) in_pValue);
        break;
    case SPATIALIZER_PARAM_DIPOLEWEIGHT:
        RTPC.dipoleWeight = *((AkReal32*) in_pValue);
        break;
    case SPATIALIZER_PARAM_DIPOLEPOWER:
        RTPC.dipolePower = *((AkReal32*) in_pValue);
        break;
    case SPATIALIZER_PARAM_DIRECTMIXLEVEL:
        RTPC.directMixLevel = *((AkReal32*) in_pValue);
        break;
    case SPATIALIZER_PARAM_REFLECTIONS:
        NonRTPC.reflections = *((bool*) in_pValue);
        break;
    case SPATIALIZER_PARAM_REFLECTIONSBINAURAL:
        NonRTPC.reflectionsBinaural = *((bool*) in_pValue);
        break;
    case SPATIALIZER_PARAM_REFLECTIONSMIXLEVEL:
        RTPC.directMixLevel = *((AkReal32*) in_pValue);
        break;
    case SPATIALIZER_PARAM_PATHING:
        NonRTPC.pathing = *((bool*) in_pValue);
        break;
    case SPATIALIZER_PARAM_PATHINGBINAURAL:
        NonRTPC.pathingBinaural = *((bool*) in_pValue);
        break;
    case SPATIALIZER_PARAM_PATHINGMIXLEVEL:
        RTPC.directMixLevel = *((AkReal32*) in_pValue);
        break;
    default:
        return AK_InvalidParameter;
    }

    m_paramChangeHandler.SetParamChange(in_paramID);
    return AK_Success;
}
