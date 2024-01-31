//
// Copyright 2017-2023 Valve Corporation.
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
