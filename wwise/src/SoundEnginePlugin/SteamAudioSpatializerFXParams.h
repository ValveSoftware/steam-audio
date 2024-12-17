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

#pragma once

#include <AK/SoundEngine/Common/IAkPlugin.h>
#include <AK/Plugin/PluginServices/AkFXParameterChangeHandler.h>

#include <phonon.h>

static const AkInt16    STEAMAUDIO_PARAM_OFF                = 0;
static const AkInt16    STEAMAUDIO_PARAM_USERDEFINED        = 1;
static const AkInt16    STEAMAUDIO_PARAM_SIMULATIONDEFINED  = 2;

static const AkPluginParamID    SPATIALIZER_PARAM_OCCLUSION         = 0;
static const AkPluginParamID    SPATIALIZER_PARAM_OCCLUSIONVALUE    = 1;
static const AkPluginParamID    SPATIALIZER_PARAM_TRANSMISSION      = 2;
static const AkPluginParamID    SPATIALIZER_PARAM_TRANSMISSIONTYPE  = 3;
static const AkPluginParamID    SPATIALIZER_PARAM_TRANSMISSIONLOW   = 4;
static const AkPluginParamID    SPATIALIZER_PARAM_TRANSMISSIONMID   = 5;
static const AkPluginParamID    SPATIALIZER_PARAM_TRANSMISSIONHIGH  = 6;
static const AkPluginParamID    SPATIALIZER_PARAM_DIRECTBINAURAL = 7;
static const AkPluginParamID    SPATIALIZER_PARAM_POSITION_X = 8;
static const AkPluginParamID    SPATIALIZER_PARAM_POSITION_Y = 9;
static const AkPluginParamID    SPATIALIZER_PARAM_POSITION_Z = 10;
static const AkPluginParamID    SPATIALIZER_PARAM_HRTFINTERPOLATION = 11;
static const AkPluginParamID    SPATIALIZER_PARAM_DISTANCEATTENUATION = 12;
static const AkPluginParamID    SPATIALIZER_PARAM_AIRABSORPTION = 13;
static const AkPluginParamID    SPATIALIZER_PARAM_DIRECTIVITY = 14;
static const AkPluginParamID    SPATIALIZER_PARAM_DIPOLEWEIGHT = 15;
static const AkPluginParamID    SPATIALIZER_PARAM_DIPOLEPOWER = 16;
static const AkPluginParamID    SPATIALIZER_PARAM_DIRECTMIXLEVEL = 17;
static const AkPluginParamID    SPATIALIZER_PARAM_REFLECTIONS = 18;
static const AkPluginParamID    SPATIALIZER_PARAM_REFLECTIONSBINAURAL = 19;
static const AkPluginParamID    SPATIALIZER_PARAM_REFLECTIONSMIXLEVEL = 20;
static const AkPluginParamID    SPATIALIZER_PARAM_PATHING = 21;
static const AkPluginParamID    SPATIALIZER_PARAM_PATHINGBINAURAL = 22;
static const AkPluginParamID    SPATIALIZER_PARAM_PATHINGMIXLEVEL = 23;
static const AkUInt32           SPATIALIZER_NUM_PARAMS = 24;

struct SteamAudioSpatializerRTPCParams
{
    AkReal32            occlusionValue;
    AkReal32            transmissionValue[3];
    AkReal32            pos[3];
    AkReal32            dipoleWeight;
    AkReal32            dipolePower;
    AkReal32            directMixLevel;
    AkReal32            reflectionsMixLevel;
    AkReal32            pathingMixLevel;
};

struct SteamAudioSpatializerNonRTPCParams
{
	bool                directBinaural;
    AkInt16             occlusion;
    AkInt16             transmission;
    IPLTransmissionType transmissionType;
    AkInt16             hrtfInterpolation;
    bool                distanceAttenuation;
    bool                airAbsorption;
    bool                directivity;
    bool                reflections;
    bool                reflectionsBinaural;
    bool                pathing;
    bool                pathingBinaural;
};

struct SteamAudioSpatializerFXParams : public AK::IAkPluginParam
{
    SteamAudioSpatializerFXParams();

    SteamAudioSpatializerFXParams(const SteamAudioSpatializerFXParams& other);

    ~SteamAudioSpatializerFXParams();

    virtual IAkPluginParam* Clone(AK::IAkPluginMemAlloc* in_pAllocator) override;

    virtual AKRESULT Init(AK::IAkPluginMemAlloc* in_pAllocator, const void* in_pParamsBlock, AkUInt32 in_uBlockSize) override;

    virtual AKRESULT Term(AK::IAkPluginMemAlloc* in_pAllocator) override;

    virtual AKRESULT SetParamsBlock(const void* in_pParamsBlock, AkUInt32 in_uBlockSize) override;

    virtual AKRESULT SetParam(AkPluginParamID in_paramID, const void* in_pValue, AkUInt32 in_uParamSize) override;

    AK::AkFXParameterChangeHandler<SPATIALIZER_NUM_PARAMS> m_paramChangeHandler;

    SteamAudioSpatializerRTPCParams      RTPC;
    SteamAudioSpatializerNonRTPCParams   NonRTPC;
};
