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

#include "SteamAudioReverbSettings.h"

// ---------------------------------------------------------------------------------------------------------------------
// USteamAudioReverbSettings
// ---------------------------------------------------------------------------------------------------------------------

USteamAudioReverbSettings::USteamAudioReverbSettings()
	: bApplyReflections(false)
	, bApplyHRTFToReflections(false)
	, ReflectionsMixLevel(1.0f)
{}

#if WITH_EDITOR

bool USteamAudioReverbSettings::CanEditChange(const FProperty* InProperty) const
{
    bool bParentVal = Super::CanEditChange(InProperty);

    if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioReverbSettings, bApplyHRTFToReflections))
        return bParentVal && bApplyReflections;
    if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioReverbSettings, ReflectionsMixLevel))
        return bParentVal && bApplyReflections;

    return bParentVal;
}

#endif
