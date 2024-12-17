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

static const AkPluginParamID    MIXRETURN_PARAM_BINAURAL    = 0;
static const AkUInt32           MIXRETURN_NUM_PARAMS        = 1;

struct SteamAudioMixReturnRTPCParams
{
};

struct SteamAudioMixReturnNonRTPCParams
{
    bool    binaural;
};

struct SteamAudioMixReturnFXParams : public AK::IAkPluginParam
{
    SteamAudioMixReturnFXParams();

    SteamAudioMixReturnFXParams(const SteamAudioMixReturnFXParams& other);

    ~SteamAudioMixReturnFXParams();

    virtual IAkPluginParam* Clone(AK::IAkPluginMemAlloc* in_pAllocator) override;

    virtual AKRESULT Init(AK::IAkPluginMemAlloc* in_pAllocator, const void* in_pParamsBlock, AkUInt32 in_uBlockSize) override;

    virtual AKRESULT Term(AK::IAkPluginMemAlloc* in_pAllocator) override;

    virtual AKRESULT SetParamsBlock(const void* in_pParamsBlock, AkUInt32 in_uBlockSize) override;

    virtual AKRESULT SetParam(AkPluginParamID in_paramID, const void* in_pValue, AkUInt32 in_uParamSize) override;

    AK::AkFXParameterChangeHandler<MIXRETURN_NUM_PARAMS> m_paramChangeHandler;

    SteamAudioMixReturnRTPCParams      RTPC;
    SteamAudioMixReturnNonRTPCParams   NonRTPC;
};
