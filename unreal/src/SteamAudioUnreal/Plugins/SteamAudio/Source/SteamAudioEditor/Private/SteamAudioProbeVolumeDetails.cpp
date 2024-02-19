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

#include "SteamAudioProbeVolumeDetails.h"
#include "ContentBrowserModule.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "EngineUtils.h"
#include "IContentBrowserSingleton.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailPropertyRow.h"
#include "LevelEditorViewport.h"
#include "PropertyCustomizationHelpers.h"
#include "Async/Async.h"
#include "Widgets/Input/SButton.h"
#include "SteamAudioBaking.h"
#include "SteamAudioCommon.h"
#include "SteamAudioManager.h"
#include "SteamAudioProbeComponent.h"
#include "SteamAudioProbeVolume.h"
#include "SteamAudioScene.h"
#include "SteamAudioSerializedObject.h"
#include "SteamAudioStaticMeshActor.h"
#include "TickableNotification.h"


namespace SteamAudio {

// ---------------------------------------------------------------------------------------------------------------------
// FSteamAudioProbeVolumeDetails
// ---------------------------------------------------------------------------------------------------------------------

TSharedRef<IDetailCustomization> FSteamAudioProbeVolumeDetails::MakeInstance()
{
    return MakeShareable(new FSteamAudioProbeVolumeDetails());
}

void FSteamAudioProbeVolumeDetails::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
    const TArray<TWeakObjectPtr<UObject>>& SelectedObjects = DetailLayout.GetSelectedObjects();
    for (const TWeakObjectPtr<UObject>& Object : SelectedObjects)
    {
        if (Object.IsValid())
        {
            if (Cast<ASteamAudioProbeVolume>(Object.Get()))
            {
                ProbeVolume = Cast<ASteamAudioProbeVolume>(Object.Get());
                break;
            }
        }
    }

	DetailLayout.HideCategory("BrushSettings");

	DetailLayout.EditCategory("ProbeBatchSettings").AddProperty(GET_MEMBER_NAME_CHECKED(ASteamAudioProbeVolume, Asset));
	DetailLayout.EditCategory("ProbeBatchSettings").AddProperty(GET_MEMBER_NAME_CHECKED(ASteamAudioProbeVolume, GenerationType));
	DetailLayout.EditCategory("ProbeBatchSettings").AddProperty(GET_MEMBER_NAME_CHECKED(ASteamAudioProbeVolume, HorizontalSpacing));
	DetailLayout.EditCategory("ProbeBatchSettings").AddProperty(GET_MEMBER_NAME_CHECKED(ASteamAudioProbeVolume, HeightAboveFloor));

    DetailLayout.EditCategory("ProbeBatchSettings").AddCustomRow(NSLOCTEXT("SteamAudio", "GenerateProbes", "Generate Probes"))
        .NameContent()
        [
            SNullWidget::NullWidget
        ]
        .ValueContent()
        [
            SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SButton)
                    .ContentPadding(2)
                    .VAlign(VAlign_Center)
                    .HAlign(HAlign_Center)
                    .OnClicked(this, &FSteamAudioProbeVolumeDetails::OnGenerateProbes)
                    [
                        SNew(STextBlock)
                        .Text(NSLOCTEXT("SteamAudio", "GenerateProbes", "Generate Probes"))
                        .Font(IDetailLayoutBuilder::GetDetailFont())
                    ]
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SButton)
                    .ContentPadding(2)
                    .VAlign(VAlign_Center)
                    .HAlign(HAlign_Center)
                    .OnClicked(this, &FSteamAudioProbeVolumeDetails::OnClearBakedData)
                    [
                        SNew(STextBlock)
                        .Text(NSLOCTEXT("SteamAudio", "ClearBakedData", "Clear Baked Data"))
                    .Font(IDetailLayoutBuilder::GetDetailFont())
                    ]
                ]
        ];

	DetailLayout.EditCategory("ProbeBatchSettings").AddProperty(GET_MEMBER_NAME_CHECKED(ASteamAudioProbeVolume, NumProbes));
	DetailLayout.EditCategory("ProbeBatchSettings").AddProperty(GET_MEMBER_NAME_CHECKED(ASteamAudioProbeVolume, DataSize));

    TSharedPtr<IPropertyHandle> DetailedStatsProperty = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(ASteamAudioProbeVolume, DetailedStats));
    TSharedRef<FDetailArrayBuilder> DetailedStatsArrayBuilder = MakeShareable(new FDetailArrayBuilder(DetailedStatsProperty.ToSharedRef()));
    DetailedStatsArrayBuilder->OnGenerateArrayElementWidget(FOnGenerateArrayElementWidget::CreateSP(this, &FSteamAudioProbeVolumeDetails::OnGenerateDetailedStats));
    DetailLayout.EditCategory("ProbeBatchSettings").AddCustomBuilder(DetailedStatsArrayBuilder);

    DetailLayout.EditCategory("BakedPathingSettings").AddCustomRow(NSLOCTEXT("SteamAudio", "BakePathing", "Bake Pathing"))
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
                    .OnClicked(this, &FSteamAudioProbeVolumeDetails::OnBakePathing)
                    [
                        SNew(STextBlock)
                        .Text(NSLOCTEXT("SteamAudio", "BakePathing", "Bake Pathing"))
                        .Font(IDetailLayoutBuilder::GetDetailFont())
                    ]
                ]
        ];
}

void FSteamAudioProbeVolumeDetails::OnGenerateDetailedStats(TSharedRef<IPropertyHandle> PropertyHandle, int32 ArrayIndex, IDetailChildrenBuilder& ChildrenBuilder)
{
		auto& Row = ChildrenBuilder.AddProperty(PropertyHandle);
		auto& Info = ProbeVolume->DetailedStats[ArrayIndex];

		Row.ShowPropertyButtons(false);
		Row.CustomWidget(false)
			.NameContent()
			[
				SNew(STextBlock)
				.Text(FText::FromString(Info.Name))
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
			.ValueContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				[
					SNew(SBox)
					.MinDesiredWidth(200)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::AsMemory(Info.Size))
						.Font(IDetailLayoutBuilder::GetDetailFont())
					]
				]
				+ SHorizontalBox::Slot()
					.AutoWidth()
				[
					PropertyCustomizationHelpers::MakeDeleteButton(FSimpleDelegate::CreateSP(this, &FSteamAudioProbeVolumeDetails::OnClearBakedDataLayer, ArrayIndex))
				]
			];
}

void FSteamAudioProbeVolumeDetails::OnClearBakedDataLayer(const int32 ArrayIndex)
{
    IPLContext Context = SteamAudio::FSteamAudioModule::GetManager().GetContext();
    IPLProbeBatch ProbeBatch = SteamAudio::LoadProbeBatchFromAsset(ProbeVolume->Asset, Context);
    if (ProbeBatch)
    {
        FSteamAudioBakedDataInfo& Info = ProbeVolume->DetailedStats[ArrayIndex];

        IPLBakedDataIdentifier Identifier;
        Identifier.type = static_cast<IPLBakedDataType>(Info.Type);
        Identifier.variation = static_cast<IPLBakedDataVariation>(Info.Variation);
        Identifier.endpointInfluence.center = SteamAudio::ConvertVector(Info.EndpointCenter);
        Identifier.endpointInfluence.radius = Info.EndpointRadius;

        iplProbeBatchRemoveData(ProbeBatch, &Identifier);

        IPLSerializedObjectSettings SerializedObjectSettings{};

        IPLSerializedObject SerializedObject = nullptr;
        IPLerror Status = iplSerializedObjectCreate(Context, &SerializedObjectSettings, &SerializedObject);
        if (Status == IPL_STATUS_SUCCESS)
        {
            iplProbeBatchSave(ProbeBatch, SerializedObject);

            ProbeVolume->Asset = USteamAudioSerializedObject::SerializeObjectToPackage(SerializedObject, ProbeVolume->Asset.GetAssetPathString());
            ProbeVolume->UpdateTotalSize(iplSerializedObjectGetSize(SerializedObject));
            ProbeVolume->RemoveLayer(Info.Name);
            ProbeVolume->MarkPackageDirty();

            iplSerializedObjectRelease(&SerializedObject);
        }
        else
        {
            UE_LOG(LogSteamAudioEditor, Error, TEXT("Unable to create serialized object. [%d]"), Status);
        }

        iplProbeBatchRelease(&ProbeBatch);
    }
    else
    {
        UE_LOG(LogSteamAudioEditor, Error, TEXT("Unable to load probe batch from asset: %s"), *ProbeVolume->Asset.GetAssetPathString());
    }
}

FReply FSteamAudioProbeVolumeDetails::OnGenerateProbes()
{
	// Grab a copy of the volume ptr as it will be destroyed if user clicks off of volume in GUI
	ASteamAudioProbeVolume* ProbeVolumeHandle = ProbeVolume.Get();

    UWorld* World = GEditor->GetLevelViewportClients()[0]->GetWorld();
    ULevel* Level = World->GetCurrentLevel();

	ASteamAudioStaticMeshActor* StaticMeshActor = ASteamAudioStaticMeshActor::FindInLevel(World, Level);
    if (StaticMeshActor && StaticMeshActor->Asset.IsAsset())
    {
        FString AssetName;
        if (PromptForAssetName(Level, AssetName))
        {
            FSteamAudioEditorModule::NotifyStarting(NSLOCTEXT("SteamAudio", "GenerateProbes", "Generating probes..."));

            Async(EAsyncExecution::Thread, [ProbeVolumeHandle, StaticMeshActor, AssetName]()
            {
                if (ProbeVolumeHandle->GenerateProbes(StaticMeshActor, AssetName))
                {
                    FSteamAudioEditorModule::NotifySucceeded(NSLOCTEXT("SteamAudio", "GenerateProbesSuccess", "Generated probes."));
                }
                else
                {
                    FSteamAudioEditorModule::NotifyFailed(NSLOCTEXT("SteamAudio", "GenerateProbesFail", "Failed to generate probes."));
                }
            });
        }
    }

    return FReply::Handled();
}

FReply FSteamAudioProbeVolumeDetails::OnClearBakedData()
{
    IPLContext Context = SteamAudio::FSteamAudioModule::GetManager().GetContext();
    IPLProbeBatch ProbeBatch = SteamAudio::LoadProbeBatchFromAsset(ProbeVolume->Asset, Context);
    if (ProbeBatch)
    {
        for (int i = 0; i < ProbeVolume->DetailedStats.Num(); ++i)
        {
            FSteamAudioBakedDataInfo& Info = ProbeVolume->DetailedStats[i];

            IPLBakedDataIdentifier Identifier;
            Identifier.type = static_cast<IPLBakedDataType>(Info.Type);
            Identifier.variation = static_cast<IPLBakedDataVariation>(Info.Variation);
            Identifier.endpointInfluence.center = SteamAudio::ConvertVector(Info.EndpointCenter);
            Identifier.endpointInfluence.radius = Info.EndpointRadius;

            iplProbeBatchRemoveData(ProbeBatch, &Identifier);
        }

        IPLSerializedObjectSettings SerializedObjectSettings{};

        IPLSerializedObject SerializedObject = nullptr;
        IPLerror Status = iplSerializedObjectCreate(Context, &SerializedObjectSettings, &SerializedObject);
        if (Status == IPL_STATUS_SUCCESS)
        {
            iplProbeBatchSave(ProbeBatch, SerializedObject);

            ProbeVolume->Asset = USteamAudioSerializedObject::SerializeObjectToPackage(SerializedObject, ProbeVolume->Asset.GetAssetPathString());
            ProbeVolume->UpdateTotalSize(iplSerializedObjectGetSize(SerializedObject));
            ProbeVolume->ResetLayers();
            ProbeVolume->MarkPackageDirty();

            iplSerializedObjectRelease(&SerializedObject);
        }
        else
        {
            UE_LOG(LogSteamAudioEditor, Error, TEXT("Unable to create serialized object. [%d]"), Status);
        }

        iplProbeBatchRelease(&ProbeBatch);
    }
    else
    {
        UE_LOG(LogSteamAudioEditor, Error, TEXT("Unable to load probe batch from asset: %s"), *ProbeVolume->Asset.GetAssetPathString());
    }

    return FReply::Handled();
}

FReply FSteamAudioProbeVolumeDetails::OnBakePathing()
{
    UWorld* World = GEditor->GetLevelViewportClients()[0]->GetWorld();
    ULevel* Level = World->GetCurrentLevel();

    FBakeTask Task{};
    Task.Type = EBakeTaskType::PATHING;
    Task.PathingProbeVolume = ProbeVolume.Get();

    TArray<FBakeTask> Tasks;
    Tasks.Add(Task);

    SteamAudio::Bake(World, Level, Tasks);

    return FReply::Handled();
}

bool FSteamAudioProbeVolumeDetails::PromptForAssetName(ULevel* Level, FString& AssetName)
{
    if (ProbeVolume->Asset.IsValid())
    {
        AssetName = ProbeVolume->Asset.GetAssetPathString();
    }
    else
    {
        IContentBrowserSingleton& ContentBrowser = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get();

        FSaveAssetDialogConfig DialogConfig{};
        DialogConfig.DialogTitleOverride = NSLOCTEXT("SteamAudio", "SaveProbeBatch", "Save probe batch as...");
        DialogConfig.DefaultPath = TEXT("/Game");
        DialogConfig.DefaultAssetName = Level->GetOutermostObject()->GetName() + "_" + ProbeVolume->GetName();
        DialogConfig.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::AllowButWarn;

        AssetName = ContentBrowser.CreateModalSaveAssetDialog(DialogConfig);
    }

    return (!AssetName.IsEmpty());
}

}
