//
// Copyright 2017-2023 Valve Corporation.
//

#include "SteamAudioSpatializationSettingsFactory.h"
#include "SteamAudioSpatializationSettings.h"

namespace SteamAudio {

// ---------------------------------------------------------------------------------------------------------------------
// FAssetTypeActions_SteamAudioSpatializationSettings
// ---------------------------------------------------------------------------------------------------------------------

FText FAssetTypeActions_SteamAudioSpatializationSettings::GetName() const
{
    return NSLOCTEXT("SteamAudio", "AssetTypeActions_SteamAudioSpatializationSettings", "Steam Audio Spatialization Settings");
}

FColor FAssetTypeActions_SteamAudioSpatializationSettings::GetTypeColor() const
{
    return FColor(245, 195, 101);
}

UClass* FAssetTypeActions_SteamAudioSpatializationSettings::GetSupportedClass() const
{
    return USteamAudioSpatializationSettings::StaticClass();
}

uint32 FAssetTypeActions_SteamAudioSpatializationSettings::GetCategories()
{
    return EAssetTypeCategories::Sounds;
}

const TArray<FText>& FAssetTypeActions_SteamAudioSpatializationSettings::GetSubMenus() const
{
    static const TArray<FText> SteamAudioSubMenus
    {
        NSLOCTEXT("SteamAudio", "AssetSteamAudioSubMenu", "Steam Audio")
    };

    return SteamAudioSubMenus;
}

}


// ---------------------------------------------------------------------------------------------------------------------
// USteamAudioSpatializationSettingsFactory
// ---------------------------------------------------------------------------------------------------------------------

USteamAudioSpatializationSettingsFactory::USteamAudioSpatializationSettingsFactory(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    SupportedClass = USteamAudioSpatializationSettings::StaticClass();

    bCreateNew = true;
    bEditorImport = false;
    bEditAfterNew = true;
}

UObject* USteamAudioSpatializationSettingsFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
    return NewObject<USteamAudioSpatializationSettings>(InParent, InName, Flags);
}

uint32 USteamAudioSpatializationSettingsFactory::GetMenuCategories() const
{
    return EAssetTypeCategories::Sounds;
}
