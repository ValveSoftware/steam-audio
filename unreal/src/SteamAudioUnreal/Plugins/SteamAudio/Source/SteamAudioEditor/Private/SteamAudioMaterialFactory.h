//
// Copyright 2017-2023 Valve Corporation.
//

#pragma once

#include "SteamAudioEditorModule.h"
#include "AssetTypeActions_Base.h"
#include "Factories/Factory.h"
#include "SteamAudioMaterialFactory.generated.h"

namespace SteamAudio {

// ---------------------------------------------------------------------------------------------------------------------
// FAssetTypeActions_SteamAudioMaterial
// ---------------------------------------------------------------------------------------------------------------------

// Returns metadata about the Steam Audio Material asset type.
class FAssetTypeActions_SteamAudioMaterial : public FAssetTypeActions_Base
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
// USteamAudioMaterialFactory
// ---------------------------------------------------------------------------------------------------------------------

// Instantiates a Material asset.
UCLASS(MinimalAPI, hideCategories = Object)
class USteamAudioMaterialFactory : public UFactory
{
    GENERATED_UCLASS_BODY()

    // Called to create a new asset.
    virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;

    // Returns the asset category to which this asset type belongs.
    virtual uint32 GetMenuCategories() const override;
};
