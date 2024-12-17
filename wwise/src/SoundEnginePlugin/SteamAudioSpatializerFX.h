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

#include "SteamAudioCommon.h"
#include "SteamAudioSpatializerFXParams.h"

class SteamAudioSpatializerFX : public AK::IAkOutOfPlaceEffectPlugin
{
public:
    SteamAudioSpatializerFX();

    ~SteamAudioSpatializerFX();

    virtual AKRESULT Init(AK::IAkPluginMemAlloc* in_pAllocator, AK::IAkEffectPluginContext* in_pEffectPluginContext, AK::IAkPluginParam* in_pParams, AkAudioFormat& io_rFormat) override;

    virtual AKRESULT Term(AK::IAkPluginMemAlloc* in_pAllocator) override;

    virtual AKRESULT Reset() override;

    virtual AKRESULT GetPluginInfo(AkPluginInfo& out_rPluginInfo) override;

    virtual void Execute(AkAudioBuffer* in_pBuffer, AkUInt32 in_uInOffset, AkAudioBuffer* out_pBuffer) override;

    virtual AKRESULT TimeSkip(AkUInt32& io_uFrames) override;

private:
    SteamAudioSpatializerFXParams* m_params;
    AK::IAkPluginMemAlloc* m_allocator;
    AK::IAkEffectPluginContext* m_context;
    AkAudioFormat m_format;
    IPLint16 m_inputChannelCount;
    IPLDirectEffect m_directEffect;
	IPLPanningEffect m_panningEffect;
	IPLBinauralEffect m_binauralEffect;
    IPLReflectionEffect m_reflectionEffect;
    IPLPathEffect m_pathingEffect;
    IPLAmbisonicsDecodeEffect m_ambisonicsDecodeEffect;
    IPLAudioBuffer m_inBuffer;
    IPLAudioBuffer m_outBuffer;
	IPLAudioBuffer m_directBuffer;
	IPLAudioBuffer m_monoBuffer;
	IPLAudioBuffer m_ambisonicsBuffer;
	IPLAudioBuffer m_ambisonicsOutBuffer;
    AkGameObjectID m_prevGameObjectID;
    std::shared_ptr<SteamAudioWwise::DoubleBufferedSource> m_source;
    IPLfloat32 m_prevDirectMixLevel;
    IPLfloat32 m_prevReflectionsMixLevel;
    IPLfloat32 m_prevPathingMixLevel;

    AKRESULT LazyInit();

    static bool IsAudioFormatSupported(const AkAudioFormat& in_rFormat);
};
