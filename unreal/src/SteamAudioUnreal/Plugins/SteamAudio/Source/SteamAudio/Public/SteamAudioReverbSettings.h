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
#include "SteamAudioReverbSettings.generated.h"

// ---------------------------------------------------------------------------------------------------------------------
// USteamAudioReverbSettings
// ---------------------------------------------------------------------------------------------------------------------

// Settings that can be serialized to an asset and re-used with multiple Audio Components to configure how the
// reverb plugin renders them.
UCLASS()
class STEAMAUDIO_API USteamAudioReverbSettings : public UReverbPluginSourceSettingsBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ReflectionsSettings)
	bool bApplyReflections;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ReflectionsSettings, meta = (DisplayName = "Apply HRTF To Reflections"))
	bool bApplyHRTFToReflections;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ReflectionsSettings)
	float ReflectionsMixLevel;

	USteamAudioReverbSettings();

#if WITH_EDITOR
    /** Used to disable UI controls based on the values of other UI controls. */
    virtual bool CanEditChange(const FProperty* InProperty) const override;
#endif
};
