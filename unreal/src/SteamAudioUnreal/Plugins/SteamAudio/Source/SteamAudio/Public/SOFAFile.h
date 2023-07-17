//
// Copyright (C) Valve Corporation. All rights reserved.
//

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "SteamAudioSettings.h"
#include "SOFAFile.generated.h"


// ---------------------------------------------------------------------------------------------------------------------
// USOFAFile
// ---------------------------------------------------------------------------------------------------------------------

/**
 * Serializable object that is created when a .sofa file is imported as an asset into the project.
 */
UCLASS()
class STEAMAUDIO_API USOFAFile : public UObject
{
	GENERATED_BODY()

public:
	/** Name of the .sofa file from which this asset was imported. */
	UPROPERTY()
	FString Name;

	/** Imported data. */
	UPROPERTY()
	TArray<uint8> Data;

	/** Volume correction factor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HRTFSettings, meta = (DisplayName = "Volume Gain (dB)", UIMin = "-12.0", UIMax = "12.0"))
	float Volume;
    
    /** HRTF normalization algorithm. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HRTFSettings)
    EHRTFNormType NormalizationType;
};
