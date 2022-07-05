//
// Copyright (C) Valve Corporation. All rights reserved.
//

#include "SteamAudioSpatializationSettings.h"

// ---------------------------------------------------------------------------------------------------------------------
// USteamAudioSpatializationSettings
// ---------------------------------------------------------------------------------------------------------------------

USteamAudioSpatializationSettings::USteamAudioSpatializationSettings()
    : bBinaural(true)
    , Interpolation(EHRTFInterpolation::NEAREST)
    , bApplyPathing(false)
    , bApplyHRTFToPathing(false)
    , PathingMixLevel(1.0f)
{}

#if WITH_EDITOR
bool USteamAudioSpatializationSettings::CanEditChange(const FProperty* InProperty) const
{
    bool bParentVal = Super::CanEditChange(InProperty);

    if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSpatializationSettings, Interpolation))
        return bParentVal && bBinaural;
    if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSpatializationSettings, bApplyHRTFToPathing))
        return bParentVal && bApplyPathing;
    if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioSpatializationSettings, PathingMixLevel))
        return bParentVal && bApplyPathing;

    return bParentVal;
}
#endif
