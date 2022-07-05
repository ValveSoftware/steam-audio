//
// Copyright (C) Valve Corporation. All rights reserved.
//

#pragma once

#include "SteamAudioEditorModule.h"
#include "IDetailCustomization.h"
#include "Input/Reply.h"

class IDetailLayoutBuilder;
class USteamAudioDynamicObjectComponent;

namespace SteamAudio {

// ---------------------------------------------------------------------------------------------------------------------
// FSteamAudioDynamicObjectDetails
// ---------------------------------------------------------------------------------------------------------------------

class FSteamAudioDynamicObjectDetails : public IDetailCustomization
{
public:
    static TSharedRef<IDetailCustomization> MakeInstance();

private:
    TWeakObjectPtr<USteamAudioDynamicObjectComponent> DynamicObjectComponent;

    virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

    FReply OnExportDynamicObject();
    FReply OnExportDynamicObjectToOBJ();

    FString GetDefaultAssetOrFileName();
    bool PromptForFileName(FString& FileName);
    bool PromptForAssetName(FString& AssetName);
    bool PromptForName(bool bExportOBJ, FString& Name);
    void Export(bool bExportOBJ);
};

}
