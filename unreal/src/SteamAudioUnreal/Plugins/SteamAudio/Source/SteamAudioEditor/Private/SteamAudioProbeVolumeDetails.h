//
// Copyright 2017-2023 Valve Corporation.
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
