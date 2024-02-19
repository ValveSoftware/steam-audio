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
