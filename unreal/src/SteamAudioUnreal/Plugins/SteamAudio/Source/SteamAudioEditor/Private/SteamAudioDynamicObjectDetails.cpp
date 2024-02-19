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
        FString Name;
        bool bNameChosen = PromptForName(bExportOBJ, Name);

        if (bNameChosen)
        {
            FSteamAudioEditorModule::NotifyStarting(NSLOCTEXT("SteamAudio", "ExportDynamic", "Exporting dynamic object..."));

            Async(EAsyncExecution::Thread, [&]()
            {
                if (!ExportDynamicObject(DynamicObjectComponent.Get(), Name, bExportOBJ))
                {
                    FSteamAudioEditorModule::NotifyFailed(NSLOCTEXT("SteamAudio", "ExportDynamicFail", "Failed to export dynamic object."));
                }
                else
                {
                    FSteamAudioEditorModule::NotifySucceeded(NSLOCTEXT("SteamAudio", "ExportDynamicSuccess", "Dynamic object exported."));
                }
            });
        }
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

bool FSteamAudioDynamicObjectDetails::PromptForAssetName(FString& AssetName)
{
    // If the Steam Audio Dynamic Object component points to some asset, use that asset's asset path.
    if (DynamicObjectComponent != nullptr && DynamicObjectComponent->Asset.IsValid())
    {
        AssetName = DynamicObjectComponent->Asset.GetAssetPathString();
        return true;
    }

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

    return false;
}

bool FSteamAudioDynamicObjectDetails::PromptForName(bool bExportOBJ, FString& Name)
{
    return (bExportOBJ) ? PromptForFileName(Name) : PromptForAssetName(Name);
}

}
