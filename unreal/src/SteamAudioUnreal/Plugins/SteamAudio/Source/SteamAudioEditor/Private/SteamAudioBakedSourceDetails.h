//
// Copyright 2017-2023 Valve Corporation.
//

#pragma once

#include "SteamAudioEditorModule.h"
#include "IDetailCustomization.h"
#include "Input/Reply.h"

class USteamAudioBakedSourceComponent;


namespace SteamAudio {

// ---------------------------------------------------------------------------------------------------------------------
// FSteamAudioBakedSourceDetails
// ---------------------------------------------------------------------------------------------------------------------

class FSteamAudioBakedSourceDetails : public IDetailCustomization
{
public:
    static TSharedRef<IDetailCustomization> MakeInstance();

private:
    TWeakObjectPtr<USteamAudioBakedSourceComponent> BakedSourceComponent;

    virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;

    FReply OnBakeReflections();
};

}
