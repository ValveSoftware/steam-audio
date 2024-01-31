//
// Copyright 2017-2023 Valve Corporation.
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
