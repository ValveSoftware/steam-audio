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
#include "SteamAudioSpatializationSettings.generated.h"

// ---------------------------------------------------------------------------------------------------------------------
// Enumerations
// ---------------------------------------------------------------------------------------------------------------------

/**
 * Equivalent to IPLHRTFInterpolation.
 */
UENUM(BlueprintType)
enum class EHRTFInterpolation : uint8
{
    NEAREST     UMETA(DisplayName = "Nearest-Neighbor"),
    BILINEAR    UMETA(DisplayName = "Bilinear"),
};


// ---------------------------------------------------------------------------------------------------------------------
// USteamAudioSpatializationSettings
// ---------------------------------------------------------------------------------------------------------------------

/**
 * Settings that can be serialized to an asset and re-used with multiple Audio components to configure how the
 * spatialization plugin renders them.
 */
UCLASS()
class STEAMAUDIO_API USteamAudioSpatializationSettings : public USpatializationPluginSourceSettingsBase
{
    GENERATED_BODY()

public:
    /** If true, use the HRTF to spatialize. If false, use Steam Audio's panning algorithm. */
    UPROPERTY(GlobalConfig, EditAnywhere, Category = SpatializationSettings)
    bool bBinaural;

    /** How to interpolate between HRTF samples. */
    UPROPERTY(GlobalConfig, EditAnywhere, Category = SpatializationSettings)
    EHRTFInterpolation Interpolation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PathingSettings)
	bool bApplyPathing;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PathingSettings, meta = (DisplayName = "Apply HRTF To Pathing"))
	bool bApplyHRTFToPathing;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PathingSettings)
	float PathingMixLevel;

    USteamAudioSpatializationSettings();

#if WITH_EDITOR
    /** Used to disable UI controls based on the values of other UI controls. */
    virtual bool CanEditChange(const FProperty* InProperty) const override;
#endif
};
