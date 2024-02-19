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
