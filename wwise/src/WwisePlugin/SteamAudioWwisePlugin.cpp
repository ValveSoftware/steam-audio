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

#include "SteamAudioWwisePlugin.h"
#include "../SoundEnginePlugin/SteamAudioWwiseFXFactory.h"

// --------------------------------------------------------------------------------------------------------------------
// SteamAudioSpatializerPlugin
// --------------------------------------------------------------------------------------------------------------------

SteamAudioSpatializerPlugin::SteamAudioSpatializerPlugin() = default;

SteamAudioSpatializerPlugin::~SteamAudioSpatializerPlugin() = default;

bool SteamAudioSpatializerPlugin::GetBankParameters(const GUID& platform, AK::Wwise::Plugin::DataWriter& dataWriter) const
{
    // Write bank data here
    dataWriter.WriteInt16(m_propertySet.GetInt16(platform, "Occlusion"));
    dataWriter.WriteReal32(m_propertySet.GetReal32(platform, "OcclusionValue"));
    dataWriter.WriteInt16(m_propertySet.GetInt16(platform, "Transmission"));
    dataWriter.WriteInt16(m_propertySet.GetInt16(platform, "TransmissionType"));
    dataWriter.WriteReal32(m_propertySet.GetReal32(platform, "TransmissionLow"));
    dataWriter.WriteReal32(m_propertySet.GetReal32(platform, "TransmissionMid"));
    dataWriter.WriteReal32(m_propertySet.GetReal32(platform, "TransmissionHigh"));
    dataWriter.WriteBool(m_propertySet.GetBool(platform, "DirectBinaural"));
    dataWriter.WriteReal32(m_propertySet.GetReal32(platform, "PositionX"));
    dataWriter.WriteReal32(m_propertySet.GetReal32(platform, "PositionY"));
    dataWriter.WriteReal32(m_propertySet.GetReal32(platform, "PositionZ"));
    dataWriter.WriteInt16(m_propertySet.GetInt16(platform, "HRTFInterpolation"));
    dataWriter.WriteBool(m_propertySet.GetBool(platform, "DistanceAttenuation"));
    dataWriter.WriteBool(m_propertySet.GetBool(platform, "AirAbsorption"));
    dataWriter.WriteBool(m_propertySet.GetBool(platform, "Directivity"));
    dataWriter.WriteReal32(m_propertySet.GetReal32(platform, "DipoleWeight"));
    dataWriter.WriteReal32(m_propertySet.GetReal32(platform, "DipolePower"));
    dataWriter.WriteReal32(m_propertySet.GetReal32(platform, "DirectMixLevel"));
    dataWriter.WriteBool(m_propertySet.GetBool(platform, "Reflections"));
    dataWriter.WriteBool(m_propertySet.GetBool(platform, "ReflectionsBinaural"));
    dataWriter.WriteReal32(m_propertySet.GetReal32(platform, "ReflectionsMixLevel"));
    dataWriter.WriteBool(m_propertySet.GetBool(platform, "Pathing"));
    dataWriter.WriteBool(m_propertySet.GetBool(platform, "PathingBinaural"));
    dataWriter.WriteReal32(m_propertySet.GetReal32(platform, "PathingMixLevel"));

    return true;
}


// --------------------------------------------------------------------------------------------------------------------
// SteamAudioMixReturnPlugin
// --------------------------------------------------------------------------------------------------------------------

SteamAudioMixReturnPlugin::SteamAudioMixReturnPlugin() = default;

SteamAudioMixReturnPlugin::~SteamAudioMixReturnPlugin() = default;

bool SteamAudioMixReturnPlugin::GetBankParameters(const GUID& in_guidPlatform, AK::Wwise::Plugin::DataWriter& in_dataWriter) const
{
    in_dataWriter.WriteBool(m_propertySet.GetBool(in_guidPlatform, "Binaural"));

    return true;
}


// --------------------------------------------------------------------------------------------------------------------
// SteamAudioReverbPlugin
// --------------------------------------------------------------------------------------------------------------------

SteamAudioReverbPlugin::SteamAudioReverbPlugin() = default;

SteamAudioReverbPlugin::~SteamAudioReverbPlugin() = default;

bool SteamAudioReverbPlugin::GetBankParameters(const GUID& in_guidPlatform, AK::Wwise::Plugin::DataWriter& in_dataWriter) const
{
    in_dataWriter.WriteBool(m_propertySet.GetBool(in_guidPlatform, "Binaural"));

    return true;
}


// Create a PluginContainer structure that contains the info for our plugin
DEFINE_AUDIOPLUGIN_CONTAINER(SteamAudioWwise);
EXPORT_AUDIOPLUGIN_CONTAINER(SteamAudioWwise);

ADD_AUDIOPLUGIN_CLASS_TO_CONTAINER(SteamAudioWwise, SteamAudioSpatializerPlugin, SteamAudioSpatializerFX);
ADD_AUDIOPLUGIN_CLASS_TO_CONTAINER(SteamAudioWwise, SteamAudioMixReturnPlugin, SteamAudioMixReturnFX);
ADD_AUDIOPLUGIN_CLASS_TO_CONTAINER(SteamAudioWwise, SteamAudioReverbPlugin, SteamAudioReverbFX);

DEFINE_PLUGIN_REGISTER_HOOK
DEFINEDUMMYASSERTHOOK;							// Placeholder assert hook for Wwise plug-ins using AKASSERT (cassert used by default)
