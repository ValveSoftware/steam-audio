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
#include "SteamAudioMaterial.generated.h"

// ---------------------------------------------------------------------------------------------------------------------
// USteamAudioMaterial
// ---------------------------------------------------------------------------------------------------------------------

/**
 * Acoustic material properties that can be serialized to an asset.
 */
UCLASS()
class STEAMAUDIO_API USteamAudioMaterial : public UObject
{
    GENERATED_BODY()

public:
    /** Low frequency (0 - 800Hz) absorption. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MaterialSettings, meta = (UIMin = "0.0", UIMax = "1.0"))
    float AbsorptionLow;

    /** Mid frequency (800Hz - 8kHz) absorption. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MaterialSettings, meta = (UIMin = "0.0", UIMax = "1.0"))
    float AbsorptionMid;

    /** High frequency (8kHz - 22kHz) absorption. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MaterialSettings, meta = (UIMin = "0.0", UIMax = "1.0"))
    float AbsorptionHigh;

    /** Scattering coefficient. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MaterialSettings, meta = (UIMin = "0.0", UIMax = "1.0"))
    float Scattering;

    /** Low frequency (0 - 800Hz) transmission. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MaterialSettings, meta = (UIMin = "0.0", UIMax = "1.0"))
    float TransmissionLow;

    /** Mid frequency (800Hz - 8kHz) transmission. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MaterialSettings, meta = (UIMin = "0.0", UIMax = "1.0"))
    float TransmissionMid;

    /** High frequency (8kHz - 22kHz) transmission. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MaterialSettings, meta = (UIMin = "0.0", UIMax = "1.0"))
    float TransmissionHigh;

    USteamAudioMaterial();
};
