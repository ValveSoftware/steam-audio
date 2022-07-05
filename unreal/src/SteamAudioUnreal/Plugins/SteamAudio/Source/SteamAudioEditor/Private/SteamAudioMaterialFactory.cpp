//
// Copyright (C) Valve Corporation. All rights reserved.
//

#include "SteamAudioMaterialFactory.h"
#include "SteamAudioMaterial.h"

namespace SteamAudio {

// ---------------------------------------------------------------------------------------------------------------------
// FAssetTypeActions_SteamAudioMaterial
// ---------------------------------------------------------------------------------------------------------------------

FText FAssetTypeActions_SteamAudioMaterial::GetName() const
{
    return NSLOCTEXT("SteamAudio", "AssetTypeActions_SteamAudioMaterial", "Steam Audio Material");
}

FColor FAssetTypeActions_SteamAudioMaterial::GetTypeColor() const
{
    return FColor(245, 195, 101);
}

UClass* FAssetTypeActions_SteamAudioMaterial::GetSupportedClass() const
{
    return USteamAudioMaterial::StaticClass();
}

uint32 FAssetTypeActions_SteamAudioMaterial::GetCategories()
{
    return EAssetTypeCategories::Sounds;
}

const TArray<FText>& FAssetTypeActions_SteamAudioMaterial::GetSubMenus() const
{
    static const TArray<FText> SteamAudioSubMenus
    {
        NSLOCTEXT("SteamAudio", "AssetSteamAudioSubMenu", "Steam Audio")
    };
    return SteamAudioSubMenus;
}

}


// ---------------------------------------------------------------------------------------------------------------------
// USteamAudioMaterialFactory
// ---------------------------------------------------------------------------------------------------------------------

USteamAudioMaterialFactory::USteamAudioMaterialFactory(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    SupportedClass = USteamAudioMaterial::StaticClass();

    bCreateNew = true;
    bEditorImport = false;
    bEditAfterNew = true;
}

UObject* USteamAudioMaterialFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
    return NewObject<USteamAudioMaterial>(InParent, Name, Flags);
}

uint32 USteamAudioMaterialFactory::GetMenuCategories() const
{
    return EAssetTypeCategories::Sounds;
}
