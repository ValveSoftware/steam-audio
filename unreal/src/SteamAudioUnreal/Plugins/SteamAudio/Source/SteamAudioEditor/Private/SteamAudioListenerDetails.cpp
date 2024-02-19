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
