//
// Copyright 2017-2023 Valve Corporation.
//

#include "SteamAudioBakedSourceDetails.h"
#include "ContentBrowserModule.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "EngineUtils.h"
#include "IContentBrowserSingleton.h"
#include "LevelEditorViewport.h"
#include "Async/Async.h"
#include "Widgets/Input/SButton.h"
#include "SteamAudioBakedSourceComponent.h"
#include "SteamAudioBaking.h"
#include "TickableNotification.h"


namespace SteamAudio {

// ---------------------------------------------------------------------------------------------------------------------
// FSteamAudioBakedSourceDetails
// ---------------------------------------------------------------------------------------------------------------------

TSharedRef<IDetailCustomization> FSteamAudioBakedSourceDetails::MakeInstance()
{
    return MakeShareable(new FSteamAudioBakedSourceDetails());
}

void FSteamAudioBakedSourceDetails::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
    const TArray<TWeakObjectPtr<UObject>>& SelectedObjects = DetailLayout.GetSelectedObjects();
    for (const TWeakObjectPtr<UObject>& Object : SelectedObjects)
    {
        if (Object.IsValid())
        {
            if (Cast<USteamAudioBakedSourceComponent>(Object.Get()))
            {
                BakedSourceComponent = Cast<USteamAudioBakedSourceComponent>(Object.Get());
                break;
            }
        }
    }

	DetailLayout.EditCategory("BakedSourceSettings").AddProperty(GET_MEMBER_NAME_CHECKED(USteamAudioBakedSourceComponent, InfluenceRadius));
    DetailLayout.EditCategory("BakedSourceSettings").AddCustomRow(NSLOCTEXT("SteamAudio", "BakeReflections", "Bake Reflections"))
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
                    .OnClicked(this, &FSteamAudioBakedSourceDetails::OnBakeReflections)
                    [
                        SNew(STextBlock)
                        .Text(NSLOCTEXT("SteamAudio", "BakeReflections", "Bake Reflections"))
                        .Font(IDetailLayoutBuilder::GetDetailFont())
                    ]
                ]
        ];
}

FReply FSteamAudioBakedSourceDetails::OnBakeReflections()
{
    UWorld* World = GEditor->GetLevelViewportClients()[0]->GetWorld();
    ULevel* Level = World->GetCurrentLevel();

    FBakeTask Task{};
    Task.Type = EBakeTaskType::STATIC_SOURCE_REFLECTIONS;
    Task.BakedSource = BakedSourceComponent.Get();

    TArray<FBakeTask> Tasks;
    Tasks.Add(Task);

    SteamAudio::Bake(World, Level, Tasks);

    return FReply::Handled();
}

}
