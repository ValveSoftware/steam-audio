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
