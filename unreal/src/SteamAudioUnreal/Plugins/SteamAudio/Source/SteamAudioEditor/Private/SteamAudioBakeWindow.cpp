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

#include "SteamAudioBakeWindow.h"
#include "ContentBrowserModule.h"
#include "DetailLayoutBuilder.h"
#include "Editor.h"
#include "EditorStyleSet.h"
#include "EngineUtils.h"
#include "IContentBrowserSingleton.h"
#include "ISettingsModule.h"
#include "LevelEditor.h"
#include "LevelEditorViewport.h"
#include "UnrealEdGlobals.h"
#include "Async/Async.h"
#include "Developer/DesktopPlatform/Public/DesktopPlatformModule.h"
#include "Editor/UnrealEdEngine.h"
#include "Interfaces/IPluginManager.h"
#include "Kismet/GameplayStatics.h"
#if ((ENGINE_MAJOR_VERSION > 5) || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1))
#include "Styling/AppStyle.h"
#endif
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "SteamAudioBakedListenerComponent.h"
#include "SteamAudioBakedSourceComponent.h"
#include "SteamAudioDynamicObjectComponent.h"
#include "SteamAudioProbeComponent.h"
#include "SteamAudioProbeVolume.h"
#include "SteamAudioScene.h"
#include "SteamAudioSettings.h"
#include "SteamAudioSourceComponent.h"
#include "SteamAudioStaticMeshActor.h"
#include "TickableNotification.h"


namespace SteamAudio {

// ---------------------------------------------------------------------------------------------------------------------
// FBakeWindow
// ---------------------------------------------------------------------------------------------------------------------

static const FName BakeWindowTabName("BakeTab");

FBakeWindow::FBakeWindow()
{
    FGlobalTabmanager::Get()->RegisterTabSpawner(BakeWindowTabName, FOnSpawnTab::CreateRaw(this, &FBakeWindow::SpawnTab))
        .SetDisplayName(FText::FromString(TEXT("Bake Indirect Sound")));
}

FBakeWindow::~FBakeWindow()
{
    FGlobalTabmanager::Get()->UnregisterTabSpawner(BakeWindowTabName);
}

void FBakeWindow::Invoke()
{
    FGlobalTabmanager::Get()->TryInvokeTab(BakeWindowTabName);
}

TSharedRef<SDockTab> FBakeWindow::SpawnTab(const FSpawnTabArgs& SpawnTabArgs)
{
    RefreshBakeTasks();

    ListView =
        SNew(SListView<TSharedPtr<FBakeWindowRow>>)
        .SelectionMode(ESelectionMode::Multi)
        .ListItemsSource(&BakeWindowRows)
        .OnGenerateRow(this, &FBakeWindow::OnGenerateRow)
        .HeaderRow(
        SNew(SHeaderRow)
        + SHeaderRow::Column("Actor")
        .DefaultLabel(FText::FromString(TEXT("Actor")))
        .FillWidth(0.35f)
        + SHeaderRow::Column("Level")
        .DefaultLabel(FText::FromString(TEXT("Level")))
        .FillWidth(0.3f)
        + SHeaderRow::Column("Type")
        .DefaultLabel(FText::FromString(TEXT("Type")))
        .FillWidth(0.2f)
        + SHeaderRow::Column("Data Size")
        .DefaultLabel(FText::FromString(TEXT("Data Size")))
        .FillWidth(0.15f)
        );

    return SNew(SDockTab)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
        [
            SNew(SBorder)
#if ((ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1) || (ENGINE_MAJOR_VERSION > 5))
            .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
#else
            .BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
#endif
        [
            ListView.ToSharedRef()
        ]
        ]
    + SVerticalBox::Slot()
        .AutoHeight()
        .HAlign(HAlign_Right)
        .Padding(2)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
        .AutoWidth()
        [
            SNew(SButton)
            .ContentPadding(3)
        .VAlign(VAlign_Center)
        .HAlign(HAlign_Center)
        .IsEnabled(this, &FBakeWindow::IsBakeEnabled)
        .OnClicked(this, &FBakeWindow::OnBakeSelected)
        [
            SNew(STextBlock)
            .Text(NSLOCTEXT("SteamAudio", "BakeSelected", "Bake Selected"))
        ]
        ]
        ]
        ];
}

TSharedRef<ITableRow> FBakeWindow::OnGenerateRow(TSharedPtr<FBakeWindowRow> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
    FString Type;
    switch (Item->Type)
    {
    case EBakeTaskType::STATIC_SOURCE_REFLECTIONS:
        Type = "Static Source";
        break;
    case EBakeTaskType::STATIC_LISTENER_REFLECTIONS:
        Type = "Static Listener";
        break;
    case EBakeTaskType::REVERB:
        Type = "Reverb";
        break;
    case EBakeTaskType::PATHING:
        Type = "Pathing";
        break;
    }

    return SNew(STableRow<TSharedPtr<FString>>, OwnerTable)
        .Padding(4)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
        .HAlign(HAlign_Left)
        .FillWidth(0.35f)
        [
            SNew(STextBlock)
            .Text(Item->Actor ? FText::FromString(Item->Actor->GetName()) : FText::FromString("N/A"))
        .Font(IDetailLayoutBuilder::GetDetailFont())
        ]
    + SHorizontalBox::Slot()
        .HAlign(HAlign_Left)
        .FillWidth(0.3f)
        [
            SNew(STextBlock)
            .Text(Item->Actor ? FText::FromString(Item->Actor->GetLevel()->GetName()) : FText::FromString("N/A"))
        .Font(IDetailLayoutBuilder::GetDetailFont())
        ]
    + SHorizontalBox::Slot()
        .HAlign(HAlign_Left)
        .FillWidth(0.2f)
        [
            SNew(STextBlock)
            .Text(FText::FromString(Type))
        .Font(IDetailLayoutBuilder::GetDetailFont())
        ]
    + SHorizontalBox::Slot()
        .HAlign(HAlign_Right)
        .FillWidth(0.15f)
        [
            SNew(STextBlock)
            .Text(FText::AsMemory(Item->Size))
        .Font(IDetailLayoutBuilder::GetDetailFont())
        ]
        ];
}

bool FBakeWindow::IsBakeEnabled() const
{
    return !GIsBaking.load();
}

FReply FBakeWindow::OnBakeSelected()
{
    TArray<TSharedPtr<FBakeWindowRow>> SelectedRows;
    ListView->GetSelectedItems(SelectedRows);

    TArray<FBakeTask> Tasks;
    for (TSharedPtr<FBakeWindowRow> Row : SelectedRows)
    {
        FBakeTask Task;
        Task.Type = Row->Type;

        switch (Task.Type)
        {
        case EBakeTaskType::STATIC_SOURCE_REFLECTIONS:
            Task.BakedSource = Row->Actor->FindComponentByClass<USteamAudioBakedSourceComponent>();
            break;
        case EBakeTaskType::STATIC_LISTENER_REFLECTIONS:
            Task.BakedListener = Row->Actor->FindComponentByClass<USteamAudioBakedListenerComponent>();
            break;
        case EBakeTaskType::PATHING:
            Task.PathingProbeVolume = Cast<ASteamAudioProbeVolume>(Row->Actor);
            break;
        }

        Tasks.Add(Task);
    }

    UWorld* World = GEditor->GetLevelViewportClients()[0]->GetWorld();
    ULevel* Level = World->GetCurrentLevel();

    Bake(World, Level, Tasks, FSteamAudioBakeComplete::CreateRaw(this, &FBakeWindow::OnBakeComplete));

    return FReply::Handled();
}

void FBakeWindow::OnBakeComplete()
{
    AsyncTask(ENamedThreads::GameThread, [this]()
    {
        RefreshBakeTasks();
        ListView->RequestListRefresh();
    });
}

// todo: update window when scene changes
void FBakeWindow::RefreshBakeTasks()
{
    UWorld* World = GEditor->GetLevelViewportClients()[0]->GetWorld();

    BakeWindowRows.Empty();

    TArray<AActor*> ProbeVolumes;
    UGameplayStatics::GetAllActorsOfClass(World, ASteamAudioProbeVolume::StaticClass(), ProbeVolumes);

    {
        TSharedPtr<FBakeWindowRow> Row = MakeShared<FBakeWindowRow>();
        Row->Type = EBakeTaskType::REVERB;
        Row->Actor = nullptr;
        Row->Size = 0;

        for (AActor* Actor : ProbeVolumes)
        {
            ASteamAudioProbeVolume* ProbeVolume = Cast<ASteamAudioProbeVolume>(Actor);
            if (ProbeVolume)
            {
                int LayerIndex = ProbeVolume->FindLayer("Reverb");
                if (LayerIndex >= 0)
                {
                    Row->Size += ProbeVolume->DetailedStats[LayerIndex].Size;
                }
            }
        }

        BakeWindowRows.Add(Row);
    }

    for (TObjectIterator<USteamAudioBakedSourceComponent> It; It; ++It)
    {
        if (It->GetWorld() != World)
            continue;

        TSharedPtr<FBakeWindowRow> Row = MakeShared<FBakeWindowRow>();
        Row->Type = EBakeTaskType::STATIC_SOURCE_REFLECTIONS;
        Row->Actor = It->GetOwner();
        Row->Size = 0;

        for (AActor* Actor : ProbeVolumes)
        {
            ASteamAudioProbeVolume* ProbeVolume = Cast<ASteamAudioProbeVolume>(Actor);
            if (ProbeVolume)
            {
                int LayerIndex = ProbeVolume->FindLayer(It->GetOwner()->GetName());
                if (LayerIndex >= 0)
                {
                    Row->Size += ProbeVolume->DetailedStats[LayerIndex].Size;
                }
            }
        }

        BakeWindowRows.Add(Row);
    }

    for (TObjectIterator<USteamAudioBakedListenerComponent> It; It; ++It)
    {
        if (It->GetWorld() != World)
            continue;

        TSharedPtr<FBakeWindowRow> Row = MakeShared<FBakeWindowRow>();
        Row->Type = EBakeTaskType::STATIC_LISTENER_REFLECTIONS;
        Row->Actor = It->GetOwner();
        Row->Size = 0;

        for (AActor* Actor : ProbeVolumes)
        {
            ASteamAudioProbeVolume* ProbeVolume = Cast<ASteamAudioProbeVolume>(Actor);
            if (ProbeVolume)
            {
                int LayerIndex = ProbeVolume->FindLayer(It->GetOwner()->GetName());
                if (LayerIndex >= 0)
                {
                    Row->Size += ProbeVolume->DetailedStats[LayerIndex].Size;
                }
            }
        }

        BakeWindowRows.Add(Row);
    }

    for (AActor* Actor : ProbeVolumes)
    {
        ASteamAudioProbeVolume* ProbeVolume = Cast<ASteamAudioProbeVolume>(Actor);
        if (ProbeVolume)
        {
            int LayerIndex = ProbeVolume->FindLayer(ProbeVolume->GetName());
            if (LayerIndex >= 0)
            {
                TSharedPtr<FBakeWindowRow> Row = MakeShared<FBakeWindowRow>();
                Row->Type = EBakeTaskType::PATHING;
                Row->Actor = Actor;
                Row->Size = ProbeVolume->DetailedStats[LayerIndex].Size;

                BakeWindowRows.Add(Row);
            }
        }
    }
}

}
