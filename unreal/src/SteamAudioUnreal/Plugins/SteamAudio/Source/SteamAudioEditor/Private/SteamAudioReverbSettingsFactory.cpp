//
// Copyright (C) Valve Corporation. All rights reserved.
//

#include "SteamAudioReverbSettingsFactory.h"
#include "SteamAudioReverbSettings.h"

namespace SteamAudio {

// ---------------------------------------------------------------------------------------------------------------------
// FAssetTypeActions_SteamAudioReverbSettings
// ---------------------------------------------------------------------------------------------------------------------

FText FAssetTypeActions_SteamAudioReverbSettings::GetName() const
{
    return NSLOCTEXT("SteamAudio", "AssetTypeActions_SteamAudioReverbSettings", "Steam Audio Reverb Settings");
}

FColor FAssetTypeActions_SteamAudioReverbSettings::GetTypeColor() const
{
    return FColor(245, 195, 101);
}

UClass* FAssetTypeActions_SteamAudioReverbSettings::GetSupportedClass() const
{
    return USteamAudioReverbSettings::StaticClass();
}

uint32 FAssetTypeActions_SteamAudioReverbSettings::GetCategories()
{
    return EAssetTypeCategories::Sounds;
}

const TArray<FText>& FAssetTypeActions_SteamAudioReverbSettings::GetSubMenus() const
{
    static const TArray<FText> SteamAudioSubMenus
    {
        NSLOCTEXT("SteamAudio", "AssetSteamAudioSubMenu", "Steam Audio")
    };

    return SteamAudioSubMenus;
}

}


// ---------------------------------------------------------------------------------------------------------------------
// USteamAudioReverbSettingsFactory
// ---------------------------------------------------------------------------------------------------------------------

USteamAudioReverbSettingsFactory::USteamAudioReverbSettingsFactory(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    SupportedClass = USteamAudioReverbSettings::StaticClass();

    bCreateNew = true;
    bEditorImport = false;
    bEditAfterNew = true;
}

UObject* USteamAudioReverbSettingsFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
    return NewObject<USteamAudioReverbSettings>(InParent, InName, Flags);
}

uint32 USteamAudioReverbSettingsFactory::GetMenuCategories() const
{
    return EAssetTypeCategories::Sounds;
}
