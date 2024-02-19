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

class ASteamAudioProbeVolume;


namespace SteamAudio {

// ---------------------------------------------------------------------------------------------------------------------
// FSteamAudioProbeVolumeDetails
// ---------------------------------------------------------------------------------------------------------------------

class FSteamAudioProbeVolumeDetails : public IDetailCustomization
{
public:
    static TSharedRef<IDetailCustomization> MakeInstance();

private:
    TWeakObjectPtr<ASteamAudioProbeVolume> ProbeVolume;

    virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;

    void OnGenerateDetailedStats(TSharedRef<IPropertyHandle> PropertyHandle, int32 ArrayIndex, IDetailChildrenBuilder& ChildrenBuilder);
    void OnClearBakedDataLayer(const int32 ArrayIndex);

    FReply OnGenerateProbes();
    FReply OnClearBakedData();
    FReply OnBakePathing();

    bool PromptForAssetName(ULevel* Level, FString& AssetName);
};

}
