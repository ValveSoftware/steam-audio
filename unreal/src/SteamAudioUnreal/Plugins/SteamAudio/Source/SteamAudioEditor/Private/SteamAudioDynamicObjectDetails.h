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
