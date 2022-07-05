//
// Copyright (C) Valve Corporation. All rights reserved.
//

#include "SteamAudioListenerDetails.h"
#include "ContentBrowserModule.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "EngineUtils.h"
#include "IContentBrowserSingleton.h"
#include "LevelEditorViewport.h"
#include "Async/Async.h"
#include "Widgets/Input/SButton.h"
#include "SteamAudioBaking.h"
#include "SteamAudioListenerComponent.h"
#include "TickableNotification.h"


namespace SteamAudio {

// ---------------------------------------------------------------------------------------------------------------------
// FSteamAudioListenerDetails
// ---------------------------------------------------------------------------------------------------------------------

TSharedRef<IDetailCustomization> FSteamAudioListenerDetails::MakeInstance()
{
    return MakeShareable(new FSteamAudioListenerDetails());
}

void FSteamAudioListenerDetails::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
    const TArray<TWeakObjectPtr<UObject>>& SelectedObjects = DetailLayout.GetSelectedObjects();
    for (const TWeakObjectPtr<UObject>& Object : SelectedObjects)
    {
        if (Object.IsValid())
        {
            if (Cast<USteamAudioListenerComponent>(Object.Get()))
            {
                ListenerComponent = Cast<USteamAudioListenerComponent>(Object.Get());
                break;
            }
        }
    }

	DetailLayout.EditCategory("BakedListenerSettings").AddProperty(GET_MEMBER_NAME_CHECKED(USteamAudioListenerComponent, CurrentBakedListener));
	DetailLayout.EditCategory("ReverbSettings").AddProperty(GET_MEMBER_NAME_CHECKED(USteamAudioListenerComponent, bSimulateReverb));
	DetailLayout.EditCategory("ReverbSettings").AddProperty(GET_MEMBER_NAME_CHECKED(USteamAudioListenerComponent, ReverbType));
    DetailLayout.EditCategory("ReverbSettings").AddCustomRow(NSLOCTEXT("SteamAudio", "BakeReverb", "Bake Reverb"))
        .NameContent()
        [
            SNullWidget::NullWidget
        ]
        .ValueContent()
        [
            SNew(SHorizontalBox) + 
                SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SButton)
                    .ContentPadding(2)
                    .VAlign(VAlign_Center)
                    .HAlign(HAlign_Center)
                    .OnClicked(this, &FSteamAudioListenerDetails::OnBakeReverb)
                    [
                        SNew(STextBlock)
                        .Text(NSLOCTEXT("SteamAudio", "BakeReverb", "Bake Reverb"))
                        .Font(IDetailLayoutBuilder::GetDetailFont())
                    ]
                ]
        ];
}

FReply FSteamAudioListenerDetails::OnBakeReverb() 
{
    UWorld* World = GEditor->GetLevelViewportClients()[0]->GetWorld();
    ULevel* Level = World->GetCurrentLevel();

    FBakeTask Task{};
    Task.Type = EBakeTaskType::REVERB;

    TArray<FBakeTask> Tasks;
    Tasks.Add(Task);

    SteamAudio::Bake(World, Level, Tasks);

    return FReply::Handled();
}

}
