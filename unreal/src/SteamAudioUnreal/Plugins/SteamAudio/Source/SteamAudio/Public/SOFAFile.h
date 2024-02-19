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
