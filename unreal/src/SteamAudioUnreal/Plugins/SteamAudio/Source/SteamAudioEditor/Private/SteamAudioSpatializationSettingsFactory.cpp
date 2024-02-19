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
