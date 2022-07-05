//
// Copyright (C) Valve Corporation. All rights reserved.
//

#include "SteamAudioOcclusionSettingsFactory.h"
#include "SteamAudioOcclusionSettings.h"

namespace SteamAudio {

// ---------------------------------------------------------------------------------------------------------------------
// FAssetTypeActions_SteamAudioOcclusionSettings
// ---------------------------------------------------------------------------------------------------------------------

FText FAssetTypeActions_SteamAudioOcclusionSettings::GetName() const
{
    return NSLOCTEXT("SteamAudio", "AssetTypeActions_SteamAudioOcclusionSettings", "Steam Audio Occlusion Settings");
}

FColor FAssetTypeActions_SteamAudioOcclusionSettings::GetTypeColor() const
{
    return FColor(245, 195, 101);
}

UClass* FAssetTypeActions_SteamAudioOcclusionSettings::GetSupportedClass() const
{
    return USteamAudioOcclusionSettings::StaticClass();
}

uint32 FAssetTypeActions_SteamAudioOcclusionSettings::GetCategories()
{
    return EAssetTypeCategories::Sounds;
}

const TArray<FText>& FAssetTypeActions_SteamAudioOcclusionSettings::GetSubMenus() const
{
    static const TArray<FText> SteamAudioSubMenus
    {
        NSLOCTEXT("SteamAudio", "AssetSteamAudioSubMenu", "Steam Audio")
    };

    return SteamAudioSubMenus;
}

}


// ---------------------------------------------------------------------------------------------------------------------
// USteamAudioOcclusionSettingsFactory
// ---------------------------------------------------------------------------------------------------------------------

USteamAudioOcclusionSettingsFactory::USteamAudioOcclusionSettingsFactory(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    SupportedClass = USteamAudioOcclusionSettings::StaticClass();

    bCreateNew = true;
    bEditorImport = false;
    bEditAfterNew = true;
}

UObject* USteamAudioOcclusionSettingsFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
    return NewObject<USteamAudioOcclusionSettings>(InParent, InName, Flags);
}

uint32 USteamAudioOcclusionSettingsFactory::GetMenuCategories() const
{
    return EAssetTypeCategories::Sounds;
}
