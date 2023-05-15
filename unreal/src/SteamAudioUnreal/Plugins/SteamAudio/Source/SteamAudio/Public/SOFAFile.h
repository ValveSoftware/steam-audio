//
// Copyright (C) Valve Corporation. All rights reserved.
//

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HRTFSettings, meta = (UIMin = "0.0", UIMax = "2.0"))
	float Volume;
};
