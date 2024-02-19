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

#include "SteamAudioEditorModule.h"
#include "AssetTypeActions_Base.h"
#include "Factories/Factory.h"
#include "SteamAudioSpatializationSettingsFactory.generated.h"

namespace SteamAudio {

// ---------------------------------------------------------------------------------------------------------------------
// FAssetTypeActions_SteamAudioSpatializationSettings
// ---------------------------------------------------------------------------------------------------------------------

// Returns metadata about the Steam Audio Spatialization Settings asset type.
class FAssetTypeActions_SteamAudioSpatializationSettings : public FAssetTypeActions_Base
{
public:
    //
    // Inherited from IAssetTypeActions
    //

    // Returns the user-friendly name of this asset type.
    virtual FText GetName() const override;

    // Returns the color with which to tint icons for this asset type.
    virtual FColor GetTypeColor() const override;

    // Returns the class object for the class corresponding to this asset type.
    virtual UClass* GetSupportedClass() const override;

    // Returns the asset category to which this asset type belongs.
    virtual uint32 GetCategories() override;

    // Returns the sub-menu under the asset category in which to show this asset type, when creating assets in the
    // content browser.
    virtual const TArray<FText>& GetSubMenus() const override;
};

}


// ---------------------------------------------------------------------------------------------------------------------
// USteamAudioSpatializationSettingsFactory
// ---------------------------------------------------------------------------------------------------------------------

// Instantiates a Spatialization Settings asset.
UCLASS(MinimalAPI, HideCategories = Object)
class USteamAudioSpatializationSettingsFactory : public UFactory
{
    GENERATED_UCLASS_BODY()

    // Called to create a new asset.
    virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;

    // Returns the asset category to which this asset type belongs.
    virtual uint32 GetMenuCategories() const override;
};
