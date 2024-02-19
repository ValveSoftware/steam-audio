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
