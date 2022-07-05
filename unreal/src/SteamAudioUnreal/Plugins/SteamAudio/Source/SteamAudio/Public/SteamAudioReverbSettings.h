//
// Copyright (C) Valve Corporation. All rights reserved.
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
