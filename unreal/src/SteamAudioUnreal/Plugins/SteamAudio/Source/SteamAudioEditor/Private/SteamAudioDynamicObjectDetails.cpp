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

#include "SteamAudioDynamicObjectDetails.h"
#include "Engine/StaticMeshActor.h"
#include "ContentBrowserModule.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IContentBrowserSingleton.h"
#include "Async/Async.h"
#include "Developer/DesktopPlatform/Public/DesktopPlatformModule.h"
#include "Engine/SimpleConstructionScript.h"
#include "Widgets/Input/SButton.h"
#include "SteamAudioDynamicObjectComponent.h"
#include "SteamAudioScene.h"

namespace SteamAudio {

// ---------------------------------------------------------------------------------------------------------------------
// FSteamAudioDynamicObjectDetails
// ---------------------------------------------------------------------------------------------------------------------

TSharedRef<IDetailCustomization> FSteamAudioDynamicObjectDetails::MakeInstance()
{
    return MakeShareable(new FSteamAudioDynamicObjectDetails());
}

void FSteamAudioDynamicObjectDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
    const TArray<TWeakObjectPtr<UObject>>& SelectedObjects = DetailBuilder.GetSelectedObjects();
    for (int i = 0; i < SelectedObjects.Num(); ++i)
    {
        const TWeakObjectPtr<UObject>& CurrentObject = SelectedObjects[i];
        if (CurrentObject.IsValid())
        {
            USteamAudioDynamicObjectComponent* SelectedComponent = Cast<USteamAudioDynamicObjectComponent>(CurrentObject.Get());
            if (SelectedComponent)
            {
                DynamicObjectComponent = SelectedComponent;
                break;
            }
        }
    }

    DetailBuilder.EditCategory("Export Settings").AddProperty(GET_MEMBER_NAME_CHECKED(USteamAudioDynamicObjectComponent, Asset));
    DetailBuilder.EditCategory("Export Settings").AddCustomRow(NSLOCTEXT("SteamAudio", "ExportDynamicObject", "Export Dynamic Object"))
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
                    .OnClicked(this, &FSteamAudioDynamicObjectDetails::OnExportDynamicObject)
                    [
                        SNew(STextBlock)
                        .Text(NSLOCTEXT("SteamAudio", "ExportDynamicObject", "Export Dynamic Object"))
                        .Font(IDetailLayoutBuilder::GetDetailFont())
                    ]
                ]
        ];
    DetailBuilder.EditCategory("Export Settings").AddCustomRow(NSLOCTEXT("SteamAudio", "ExportDynamicObjectToOBJ", "Export Dynamic Object to OBJ"))
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
                    .OnClicked(this, &FSteamAudioDynamicObjectDetails::OnExportDynamicObjectToOBJ)
                    [
                        SNew(STextBlock)
                        .Text(NSLOCTEXT("SteamAudio", "ExportDynamicObjectToOBJ", "Export Dynamic Object to OBJ"))
                        .Font(IDetailLayoutBuilder::GetDetailFont())
                    ]
                ]
        ];
}

FReply FSteamAudioDynamicObjectDetails::OnExportDynamicObject()
{
    Export(false);
    return FReply::Handled();
}

FReply FSteamAudioDynamicObjectDetails::OnExportDynamicObjectToOBJ()
{
    Export(true);
    return FReply::Handled();
}

void FSteamAudioDynamicObjectDetails::Export(bool bExportOBJ)
{
    if (DynamicObjectComponent != nullptr)
    {
        int32 AssetsCounter = 0;
        TArray<AActor*> Children;
        DynamicObjectComponent->GetOwner()->GetAllChildActors(Children);

        FSteamAudioEditorModule::NotifyStarting(NSLOCTEXT("SteamAudio", "ExportDynamic", "Exporting dynamic object..."));
        for (int32 i = 0; i < Children.Num() + 1; ++i)
        {
            if (i > 0 && !Children[i - 1]->IsA<AStaticMeshActor>())
                continue;

            auto ResultDynamicObjectComponent = (i == 0 ? DynamicObjectComponent.Get() : Children[i - 1]->FindComponentByClass<USteamAudioDynamicObjectComponent>());
            if (!ResultDynamicObjectComponent)
            {
                ResultDynamicObjectComponent = NewObject<USteamAudioDynamicObjectComponent>(Children[i - 1]);
                ResultDynamicObjectComponent->RegisterComponent();
                Children[i - 1]->AddInstanceComponent(ResultDynamicObjectComponent);
            }

            FString Name;
            bool bNameChosen = PromptForName(ResultDynamicObjectComponent, bExportOBJ, Name, AssetsCounter++);
            if (bNameChosen)
            {
                Async(EAsyncExecution::Thread, [ResultDynamicObjectComponent, Name, bExportOBJ]()
                    {
                        if (!ExportDynamicObject(ResultDynamicObjectComponent, Name, bExportOBJ))
                        {
                            FSteamAudioEditorModule::NotifyFailed(NSLOCTEXT("SteamAudio", "ExportDynamicFail", "Failed to export dynamic object."));
                        }
                    });
            }
        }
        FSteamAudioEditorModule::NotifySucceeded(NSLOCTEXT("SteamAudio", "ExportDynamicSuccess", "Dynamic object exported."));
    }
}

FString FSteamAudioDynamicObjectDetails::GetDefaultAssetOrFileName()
{
    check(DynamicObjectComponent != nullptr);

    if (DynamicObjectComponent->IsInBlueprint())
    {
        return DynamicObjectComponent->GetOutermostObject()->GetName() + "_DynamicGeometry";
    }
    else
    {
        return DynamicObjectComponent->GetOutermostObject()->GetName() + "_" + DynamicObjectComponent->GetOwner()->GetName() + "_DynamicGeometry";
    }
}

bool FSteamAudioDynamicObjectDetails::PromptForFileName(FString& FileName)
{
    IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
    if (DesktopPlatform)
    {
        TArray<FString> FileNames;
        bool bFileChosen = DesktopPlatform->SaveFileDialog(
            FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
            "Save as OBJ...",
            FPaths::ProjectContentDir(),
            GetDefaultAssetOrFileName() + ".obj",
            "OBJ File|*.obj",
            EFileDialogFlags::None,
            FileNames);

        if (bFileChosen)
        {
            FileName = FileNames[0];
            return true;
        }
    }

    return false;
}

bool FSteamAudioDynamicObjectDetails::PromptForAssetName(USteamAudioDynamicObjectComponent* DynamicComponent, FString& AssetName, int32 AssetIndex)
{
    // If the Steam Audio Dynamic Object component points to some asset, use that asset's asset path.
    if (DynamicComponent != nullptr && DynamicComponent->Asset.IsValid())
    {
        AssetName = DynamicComponent->Asset.GetAssetPathString();
        return true;
    }

    if (DynamicComponent == DynamicObjectComponent)
    {
        // Otherwise, prompt the user to create a new .uasset.
        IContentBrowserSingleton& ContentBrowser = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get();

        FSaveAssetDialogConfig DialogConfig{};
        DialogConfig.DialogTitleOverride = NSLOCTEXT("SteamAudio", "SaveStaticMesh", "Save static mesh as...");
        DialogConfig.DefaultPath = TEXT("/Game");
        DialogConfig.DefaultAssetName = GetDefaultAssetOrFileName();
        DialogConfig.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::AllowButWarn;
        FString AssetPath = ContentBrowser.CreateModalSaveAssetDialog(DialogConfig);
        if (!AssetPath.IsEmpty())
        {
            AssetName = AssetPath;
            return true;
        }
    }
    else
    {
        FString PackageName, ObjectName;
        DynamicObjectComponent->Asset.GetAssetPathString().Split(".", &PackageName, &ObjectName);
        FString AssetIndexExtension = "_" + FString::FromInt(AssetIndex);
        AssetName = PackageName.Append(AssetIndexExtension) + "." + ObjectName.Append(AssetIndexExtension);
        return true;
    }

    return false;
}

bool FSteamAudioDynamicObjectDetails::PromptForName(USteamAudioDynamicObjectComponent* DynamicComponent, bool bExportOBJ, FString& Name, int32 AssetIndex)
{
    return (bExportOBJ) ? PromptForFileName(Name) : PromptForAssetName(DynamicComponent, Name, AssetIndex);
}

}
