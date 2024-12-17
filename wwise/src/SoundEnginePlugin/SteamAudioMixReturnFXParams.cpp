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

#include "SteamAudioMixReturnFXParams.h"

#include <AK/Tools/Common/AkBankReadHelpers.h>


SteamAudioMixReturnFXParams::SteamAudioMixReturnFXParams() = default;

SteamAudioMixReturnFXParams::SteamAudioMixReturnFXParams(const SteamAudioMixReturnFXParams& other)
    : RTPC(other.RTPC)
    , NonRTPC(other.NonRTPC)
{
    m_paramChangeHandler.SetAllParamChanges();
}

SteamAudioMixReturnFXParams::~SteamAudioMixReturnFXParams() = default;

AK::IAkPluginParam* SteamAudioMixReturnFXParams::Clone(AK::IAkPluginMemAlloc* in_pAllocator)
{
    return AK_PLUGIN_NEW(in_pAllocator, SteamAudioMixReturnFXParams(*this));
}

AKRESULT SteamAudioMixReturnFXParams::Init(AK::IAkPluginMemAlloc* in_pAllocator, const void* in_pParamsBlock, AkUInt32 in_uBlockSize)
{
    if (in_uBlockSize == 0)
    {
        NonRTPC.binaural = true;

        m_paramChangeHandler.SetAllParamChanges();
        
        return AK_Success;
    }
    else
    {
        return SetParamsBlock(in_pParamsBlock, in_uBlockSize);
    }
}

AKRESULT SteamAudioMixReturnFXParams::Term(AK::IAkPluginMemAlloc* in_pAllocator)
{
    AK_PLUGIN_DELETE(in_pAllocator, this);
    return AK_Success;
}

AKRESULT SteamAudioMixReturnFXParams::SetParamsBlock(const void* in_pParamsBlock, AkUInt32 in_uBlockSize)
{
    auto result = AK_Success;

    auto* block = (AkUInt8*) in_pParamsBlock;

    NonRTPC.binaural = READBANKDATA(bool, block, in_uBlockSize);

    CHECKBANKDATASIZE(in_uBlockSize, result);

    m_paramChangeHandler.SetAllParamChanges();

    return result;
}

AKRESULT SteamAudioMixReturnFXParams::SetParam(AkPluginParamID in_paramID, const void* in_pValue, AkUInt32 in_uParamSize)
{
    // NOTE: RTPC parameters are always sent as AkReal32.

    switch (in_paramID)
    {
    case MIXRETURN_PARAM_BINAURAL:
        NonRTPC.binaural = (bool) *((bool*) in_pValue);
        break;
    default:
        return AK_InvalidParameter;
    }

    m_paramChangeHandler.SetParamChange(in_paramID);
    return AK_Success;
}
