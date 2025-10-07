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

#include "../SteamAudioWwisePlugin.h"

#if (AK_WWISESDK_VERSION_MAJOR > 2024) || (AK_WWISESDK_VERSION_MAJOR == 2024 && AK_WWISESDK_VERSION_MINOR >= 1)
    #include <AK/Wwise/Plugin/GUIWindows.h>
    using SteamAudioPluginGUI = AK::Wwise::Plugin::GUIWindows;
#else
    #include <AK/Wwise/Plugin/PluginMFCWindows.h>
    using SteamAudioPluginGUI = AK::Wwise::Plugin::PluginMFCWindows<>;
#endif

class SteamAudioSpatializerPluginGUI final : public SteamAudioPluginGUI
{
public:
    SteamAudioSpatializerPluginGUI();
};

class SteamAudioMixReturnPluginGUI final : public SteamAudioPluginGUI
{
public:
    SteamAudioMixReturnPluginGUI();
};

class SteamAudioReverbPluginGUI final : public SteamAudioPluginGUI
{
public:
    SteamAudioReverbPluginGUI();
};