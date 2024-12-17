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

#include <AK/Wwise/Plugin.h>

// --------------------------------------------------------------------------------------------------------------------
// SteamAudioSpatializerPlugin
// --------------------------------------------------------------------------------------------------------------------

class SteamAudioSpatializerPlugin final : public AK::Wwise::Plugin::AudioPlugin
{
public:
    SteamAudioSpatializerPlugin();
    
    ~SteamAudioSpatializerPlugin();

    /// This function is called by Wwise to obtain parameters that will be written to a bank.
    /// Because these can be changed at run-time, the parameter block should stay relatively small.
    // Larger data should be put in the Data Block.
    virtual bool GetBankParameters(const GUID& platform, AK::Wwise::Plugin::DataWriter& dataWriter) const override;
};


// --------------------------------------------------------------------------------------------------------------------
// SteamAudioMixReturnPlugin
// --------------------------------------------------------------------------------------------------------------------

class SteamAudioMixReturnPlugin final : public AK::Wwise::Plugin::AudioPlugin
{
public:
    SteamAudioMixReturnPlugin();

    ~SteamAudioMixReturnPlugin();

    virtual bool GetBankParameters(const GUID& in_guidPlatform, AK::Wwise::Plugin::DataWriter& in_dataWriter) const override;
};


// --------------------------------------------------------------------------------------------------------------------
// SteamAudioReverbPlugin
// --------------------------------------------------------------------------------------------------------------------

class SteamAudioReverbPlugin final : public AK::Wwise::Plugin::AudioPlugin
{
public:
    SteamAudioReverbPlugin();

    ~SteamAudioReverbPlugin();

    virtual bool GetBankParameters(const GUID& in_guidPlatform, AK::Wwise::Plugin::DataWriter& in_dataWriter) const override;
};


DECLARE_AUDIOPLUGIN_CONTAINER(SteamAudioWwise);	// Exposes our PluginContainer structure that contains the info for our plugin
