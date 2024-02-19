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

#pragma once

#include "SteamAudioModule.h"
#include "SteamAudioOcclusionSettings.generated.h"

// ---------------------------------------------------------------------------------------------------------------------
// Enumerations
// ---------------------------------------------------------------------------------------------------------------------

/**
 * Equivalent to IPLTransmissionType.
 */
UENUM(BlueprintType)
enum class ETransmissionType : uint8
{
    FREQUENCY_INDEPENDENT   UMETA(DisplayName = "Frequency-Independent"),
    FREQUENCY_DEPENDENT     UMETA(DisplayName = "Frequency-Dependent"),
};


// ---------------------------------------------------------------------------------------------------------------------
// USteamAudioOcclusionSettings
// ---------------------------------------------------------------------------------------------------------------------

// Settings that can be serialized to an asset and re-used with multiple Audio Components to configure how the
// occlusion plugin renders them.
UCLASS()
class STEAMAUDIO_API USteamAudioOcclusionSettings : public UOcclusionPluginSourceSettingsBase
{
    GENERATED_BODY()

public:
    /** If true, physics-based distance attenuation will be calculated and applied. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = DistanceAttenuationSettings)
    bool bApplyDistanceAttenuation;

    /** If true, frequency-dependent air absorption will be calculated and applied. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AirAbsorptionSettings)
    bool bApplyAirAbsorption;

    /** If true, a dipole directivity pattern will be modeled and applied. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = DirectivitySettings)
    bool bApplyDirectivity;

    /** Blends between monopole (omnidirectional) and dipole directivity patterns. Only if directivity is applied. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = DirectivitySettings, meta = (UIMin = "0.0", UIMax = "1.0"))
    float DipoleWeight;

    /** Controls how focused the dipole directivity is. Only if directivity is applied. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = DirectivitySettings, meta = (UIMin = "0.0", UIMax = "4.0"))
    float DipolePower;

    /** If true, attenuation due to occlusion will be applied. The occlusion attenuation value is provided by the
        Steam Audio Source component. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = OcclusionSettings)
    bool bApplyOcclusion;

    /** If true, attenuation due to transmission will be applied. The transmission attenuation value is provided by the
        Steam Audio Source component. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = OcclusionSettings)
    bool bApplyTransmission;

    /** Controls whether the transmission attenuation is frequency-dependent or not. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = OcclusionSettings)
    ETransmissionType TransmissionType;

    USteamAudioOcclusionSettings();

#if WITH_EDITOR
    /** Used to disable UI controls based on the values of other UI controls. */
    virtual bool CanEditChange(const FProperty* InProperty) const override;
#endif
};
