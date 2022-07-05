//
// Copyright (C) Valve Corporation. All rights reserved.
//

#include "SteamAudioOcclusionSettings.h"

// ---------------------------------------------------------------------------------------------------------------------
// USteamAudioOcclusionSettings
// ---------------------------------------------------------------------------------------------------------------------

USteamAudioOcclusionSettings::USteamAudioOcclusionSettings()
    : bApplyDistanceAttenuation(false)
    , bApplyAirAbsorption(false)
    , bApplyDirectivity(false)
    , DipoleWeight(0.0f)
    , DipolePower(0.0f)
    , bApplyOcclusion(false)
    , bApplyTransmission(false)
    , TransmissionType(ETransmissionType::FREQUENCY_DEPENDENT)
{}

#if WITH_EDITOR
bool USteamAudioOcclusionSettings::CanEditChange(const FProperty* InProperty) const
{
    const bool ParentVal = Super::CanEditChange(InProperty);

    if ((InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioOcclusionSettings, DipoleWeight)))
    {
        return ParentVal && bApplyDirectivity;
    }
    else if ((InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioOcclusionSettings, DipolePower)))
    {
        return ParentVal && bApplyDirectivity;
    }
    else if ((InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioOcclusionSettings, bApplyTransmission)))
    {
        return ParentVal && bApplyOcclusion;
    }
    else if ((InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USteamAudioOcclusionSettings, TransmissionType)))
    {
        return ParentVal && bApplyTransmission;
    }

    return ParentVal;
}
#endif
