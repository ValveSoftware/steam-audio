//
// Copyright 2017-2023 Valve Corporation.
//

#include "SteamAudioBakedListenerDetails.h"
#include "ContentBrowserModule.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "EngineUtils.h"
#include "IContentBrowserSingleton.h"
#include "LevelEditorViewport.h"
#include "Async/Async.h"
#include "Widgets/Input/SButton.h"
#include "SteamAudioBakedListenerComponent.h"
#include "SteamAudioBaking.h"
#include "TickableNotification.h"


namespace SteamAudio {

// ---------------------------------------------------------------------------------------------------------------------
// FSteamAudioBakedListenerDetails
// ---------------------------------------------------------------------------------------------------------------------

TSharedRef<IDetailCustomization> FSteamAudioBakedListenerDetails::MakeInstance()
{
    return MakeShareable(new FSteamAudioBakedListenerDetails());
}

void FSteamAudioBakedListenerDetails::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
    const TArray<TWeakObjectPtr<UObject>>& SelectedObjects = DetailLayout.GetSelectedObjects();
    for (const TWeakObjectPtr<UObject>& Object : SelectedObjects)
    {
        if (Object.IsValid())
        {
            if (Cast<USteamAudioBakedListenerComponent>(Object.Get()))
            {
                BakedListenerComponent = Cast<USteamAudioBakedListenerComponent>(Object.Get());
                break;
            }
        }
    }

	DetailLayout.EditCategory("BakedListenerSettings").AddProperty(GET_MEMBER_NAME_CHECKED(USteamAudioBakedListenerComponent, InfluenceRadius));
    DetailLayout.EditCategory("BakedListenerSettings").AddCustomRow(NSLOCTEXT("SteamAudio", "BakeReflections", "Bake Reflections"))
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
                    .OnClicked(this, &FSteamAudioBakedListenerDetails::OnBakeReflections)
                    [
                        SNew(STextBlock)
                        .Text(NSLOCTEXT("SteamAudio", "BakeReflections", "Bake Reflections"))
                        .Font(IDetailLayoutBuilder::GetDetailFont())
                    ]
                ]
        ];
}

FReply FSteamAudioBakedListenerDetails::OnBakeReflections()
{
    UWorld* World = GEditor->GetLevelViewportClients()[0]->GetWorld();
    ULevel* Level = World->GetCurrentLevel();

    FBakeTask Task{};
    Task.Type = EBakeTaskType::STATIC_LISTENER_REFLECTIONS;
    Task.BakedListener = BakedListenerComponent.Get();

    TArray<FBakeTask> Tasks;
    Tasks.Add(Task);

    SteamAudio::Bake(World, Level, Tasks);

    return FReply::Handled();
}

}
