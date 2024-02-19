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

#include "SteamAudioEditorModule.h"
#include "ContentBrowserModule.h"
#include "Editor.h"
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
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "SteamAudioBakedListenerComponent.h"
#include "SteamAudioBakedListenerComponentVisualizer.h"
#include "SteamAudioBakedListenerDetails.h"
#include "SteamAudioBakedSourceComponent.h"
#include "SteamAudioBakedSourceComponentVisualizer.h"
#include "SteamAudioBakedSourceDetails.h"
#include "SteamAudioBakeWindow.h"
#include "SteamAudioDynamicObjectComponent.h"
#include "SteamAudioDynamicObjectDetails.h"
#include "SteamAudioGeometryComponent.h"
#include "SteamAudioListenerDetails.h"
#include "SteamAudioMaterialFactory.h"
#include "SteamAudioOcclusionSettingsFactory.h"
#include "SteamAudioProbeComponent.h"
#include "SteamAudioProbeComponentVisualizer.h"
#include "SteamAudioProbeVolumeDetails.h"
#include "SteamAudioReverbSettingsFactory.h"
#include "SteamAudioScene.h"
#include "SteamAudioSettings.h"
#include "SteamAudioSourceComponent.h"
#include "SteamAudioSourceComponentVisualizer.h"
#include "SteamAudioSpatializationSettingsFactory.h"
#include "SteamAudioStaticMeshActor.h"
#include "TickableNotification.h"

DEFINE_LOG_CATEGORY(LogSteamAudioEditor);

IMPLEMENT_MODULE(SteamAudio::FSteamAudioEditorModule, SteamAudioEditor)

namespace SteamAudio {

static TSharedPtr<FTickableNotification> GEdModeTickable = MakeShareable(new FTickableNotification());

// ---------------------------------------------------------------------------------------------------------------------
// Helper Functions
// ---------------------------------------------------------------------------------------------------------------------

template <typename T>
void AddAssetType(IAssetTools& AssetTools, TArray<TSharedPtr<FAssetTypeActions_Base>> AssetTypes)
{
    TSharedPtr<T> AssetType = MakeShared<T>();
    AssetTools.RegisterAssetTypeActions(AssetType.ToSharedRef());
    AssetTypes.Add(StaticCastSharedPtr<FAssetTypeActions_Base>(AssetType));
}


// ---------------------------------------------------------------------------------------------------------------------
// FSteamAudioEditorModule
// ---------------------------------------------------------------------------------------------------------------------

void FSteamAudioEditorModule::StartupModule()
{
    // Initialize settings data.
    ISettingsModule& SettingsModule = FModuleManager::LoadModuleChecked<ISettingsModule>(TEXT("Settings"));
    SettingsModule.RegisterSettings("Project", "Plugins", "Steam Audio",
        NSLOCTEXT("SteamAudio", "SteamAudio", "Steam Audio"),
        NSLOCTEXT("SteamAudio", "ConfigureSteamAudioSettings", "Configure Steam Audio settings"),
        GetMutableDefault<USteamAudioSettings>());

    // Create and register custom Slate style.
    FString SteamAudioContent = IPluginManager::Get().FindPlugin("SteamAudio")->GetBaseDir() + "/Content";
    FVector2D Vec16 = FVector2D(16.0f, 16.0f);
    FVector2D Vec40 = FVector2D(40.0f, 40.0f);
    FVector2D Vec64 = FVector2D(64.0f, 64.0f);
    SteamAudioStyleSet = MakeShareable(new FSlateStyleSet("SteamAudio"));
    SteamAudioStyleSet->SetContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate"));
    SteamAudioStyleSet->SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));
    SteamAudioStyleSet->Set("ClassIcon.SteamAudioBakedListenerComponent", new FSlateImageBrush(SteamAudioContent + "/S_SteamAudioSource_16.png", Vec16));
    SteamAudioStyleSet->Set("ClassIcon.SteamAudioBakedSourceComponent", new FSlateImageBrush(SteamAudioContent + "/S_SteamAudioSource_16.png", Vec16));
    SteamAudioStyleSet->Set("ClassIcon.SteamAudioDynamicObjectComponent", new FSlateImageBrush(SteamAudioContent + "/S_SteamAudioGeometry_16.png", Vec16));
    SteamAudioStyleSet->Set("ClassIcon.SteamAudioGeometryComponent", new FSlateImageBrush(SteamAudioContent + "/S_SteamAudioGeometry_16.png", Vec16));
    SteamAudioStyleSet->Set("ClassIcon.SteamAudioMaterialComponent", new FSlateImageBrush(SteamAudioContent + "/S_SteamAudioMaterial_16.png", Vec16));
    SteamAudioStyleSet->Set("ClassIcon.SteamAudioSourceComponent", new FSlateImageBrush(SteamAudioContent + "/S_SteamAudioSource_16.png", Vec16));
    SteamAudioStyleSet->Set("ClassIcon.SteamAudioSpatializationSettings", new FSlateImageBrush(SteamAudioContent + "/S_SteamAudioSpatializationSettings_16.png", Vec16));
    SteamAudioStyleSet->Set("ClassThumbnail.SteamAudioSpatializationSettings", new FSlateImageBrush(SteamAudioContent + "/S_SteamAudioSpatializationSettings_64.png", Vec64));
    SteamAudioStyleSet->Set("ClassIcon.SteamAudioOcclusionSettings", new FSlateImageBrush(SteamAudioContent + "/S_SteamAudioOcclusionSettings_16.png", Vec16));
    SteamAudioStyleSet->Set("ClassThumbnail.SteamAudioOcclusionSettings", new FSlateImageBrush(SteamAudioContent + "/S_SteamAudioOcclusionSettings_64.png", Vec64));
    SteamAudioStyleSet->Set("ClassIcon.SteamAudioReverbSettings", new FSlateImageBrush(SteamAudioContent + "/S_SteamAudioReverbSettings_16.png", Vec16));
    SteamAudioStyleSet->Set("ClassThumbnail.SteamAudioReverbSettings", new FSlateImageBrush(SteamAudioContent + "/S_SteamAudioReverbSettings_64.png", Vec64));
    SteamAudioStyleSet->Set("LevelEditor.SteamAudioMode", new FSlateImageBrush(SteamAudioContent + "/SteamAudio_EdMode_40.png", Vec40));
    SteamAudioStyleSet->Set("LevelEditor.SteamAudioMode.Small", new FSlateImageBrush(SteamAudioContent + "/SteamAudio_EdMode_16.png", Vec16));
    FSlateStyleRegistry::RegisterSlateStyle(*SteamAudioStyleSet.Get());

    // Initialize asset types.
    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
    AddAssetType<FAssetTypeActions_SteamAudioMaterial>(AssetTools, AssetTypeActions);
    AddAssetType<FAssetTypeActions_SteamAudioSpatializationSettings>(AssetTools, AssetTypeActions);
    AddAssetType<FAssetTypeActions_SteamAudioOcclusionSettings>(AssetTools, AssetTypeActions);
    AddAssetType<FAssetTypeActions_SteamAudioReverbSettings>(AssetTools, AssetTypeActions);

    // Initialize detail customizations (custom GUIs for various components).
    FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout("SteamAudioDynamicObjectComponent", FOnGetDetailCustomizationInstance::CreateStatic(&FSteamAudioDynamicObjectDetails::MakeInstance));
	PropertyModule.RegisterCustomClassLayout("SteamAudioBakedSourceComponent", FOnGetDetailCustomizationInstance::CreateStatic(&FSteamAudioBakedSourceDetails::MakeInstance));
	PropertyModule.RegisterCustomClassLayout("SteamAudioBakedListenerComponent", FOnGetDetailCustomizationInstance::CreateStatic(&FSteamAudioBakedListenerDetails::MakeInstance));
	PropertyModule.RegisterCustomClassLayout("SteamAudioListenerComponent", FOnGetDetailCustomizationInstance::CreateStatic(&FSteamAudioListenerDetails::MakeInstance));
    PropertyModule.RegisterCustomClassLayout("SteamAudioProbeVolume", FOnGetDetailCustomizationInstance::CreateStatic(&FSteamAudioProbeVolumeDetails::MakeInstance));

    // Initialize custom visualizers.
    RegisterComponentVisualizer(USteamAudioProbeComponent::StaticClass()->GetFName(), MakeShared<FSteamAudioProbeComponentVisualizer>());
    RegisterComponentVisualizer(USteamAudioBakedSourceComponent::StaticClass()->GetFName(), MakeShared<FSteamAudioBakedSourceComponentVisualizer>());
    RegisterComponentVisualizer(USteamAudioBakedListenerComponent::StaticClass()->GetFName(), MakeShared<FSteamAudioBakedListenerComponentVisualizer>());
    RegisterComponentVisualizer(USteamAudioSourceComponent::StaticClass()->GetFName(), MakeShared<FSteamAudioSourceComponentVisualizer>());

    // Create the bake window.
    BakeWindow = MakeShared<FBakeWindow>();

    // Extend the toolbar.
    if (!IsRunningCommandlet())
    {
        FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));

        TSharedPtr<FUICommandList> Actions = MakeShared<FUICommandList>();

        TSharedPtr<FExtender> Extender = MakeShared<FExtender>();
#if ((ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 0) || (ENGINE_MAJOR_VERSION > 5))
        Extender->AddToolBarExtension("Content", EExtensionHook::After, Actions, FToolBarExtensionDelegate::CreateLambda([&](FToolBarBuilder& Builder)
#else
        Extender->AddToolBarExtension("Misc", EExtensionHook::After, Actions, FToolBarExtensionDelegate::CreateLambda([&](FToolBarBuilder& Builder)
#endif
        {
            Builder.AddComboButton(
                FUIAction(),
                FOnGetContent::CreateLambda([this]()
                {
                    FMenuBuilder MenuBuilder(true, nullptr);

                    MenuBuilder.BeginSection("GeometryTagging", NSLOCTEXT("SteamAudio", "MenuGeometryTagging", "Geometry Tagging"));
                    MenuBuilder.AddMenuEntry(
                        NSLOCTEXT("SteamAudio", "MenuAddAllActors", "Add Geometry Component to all Actors"),
                        NSLOCTEXT("SteamAudio", "MenuAddAllActorsTooltip", "Add the Steam Audio Geometry component to all actors with static geometry."),
                        FSlateIcon(),
                        FUIAction(FExecuteAction::CreateRaw(this, &FSteamAudioEditorModule::OnAddGeometryComponentToStaticMeshes))
                    );
                    MenuBuilder.AddMenuEntry(
                        NSLOCTEXT("SteamAudio", "MenuRemoveAllActors", "Remove Geometry Component from all Actors"),
                        NSLOCTEXT("SteamAudio", "MenuRemoveAllActorsTooltip", "Remove the Steam Audio Geometry component from all actors with static geometry."),
                        FSlateIcon(),
                        FUIAction(FExecuteAction::CreateRaw(this, &FSteamAudioEditorModule::OnRemoveGeometryComponentFromStaticMeshes))
                    );
                    MenuBuilder.EndSection();

                    MenuBuilder.BeginSection("StaticGeometry", NSLOCTEXT("SteamAudio", "MenuStaticGeometry", "Static Geometry"));
                    MenuBuilder.AddMenuEntry(
                        NSLOCTEXT("SteamAudio", "MenuExportStatic", "Export Static Geometry"),
                        NSLOCTEXT("SteamAudio", "MenuExportStaticTooltip", "Export the static geometry for all sublevels."),
                        FSlateIcon(),
                        FUIAction(FExecuteAction::CreateRaw(this, &FSteamAudioEditorModule::OnExportStaticGeometry))
                    );
                    MenuBuilder.AddMenuEntry(
                        NSLOCTEXT("SteamAudio", "MenuExportStaticSingle", "Export Static Geometry (Current Level)"),
                        NSLOCTEXT("SteamAudio", "MenuExportStaticSingleTooltip", "Export the static geometry for the current sublevel."),
                        FSlateIcon(),
                        FUIAction(FExecuteAction::CreateRaw(this, &FSteamAudioEditorModule::OnExportStaticGeometryCurrentLevel))
                    );
                    MenuBuilder.AddMenuEntry(
                        NSLOCTEXT("SteamAudio", "MenuExportStaticOBJ", "Export Static Geometry to .obj"),
                        NSLOCTEXT("SteamAudio", "MenuExportStaticOBJTooltip", "Export the static geometry for all sublevels to a .obj file."),
                        FSlateIcon(),
                        FUIAction(FExecuteAction::CreateRaw(this, &FSteamAudioEditorModule::OnExportStaticGeometryToOBJ))
                    );
                    MenuBuilder.AddMenuEntry(
                        NSLOCTEXT("SteamAudio", "MenuExportStaticSingleOBJ", "Export Static Geometry to .obj (Current Level)"),
                        NSLOCTEXT("SteamAudio", "MenuExportStaticSingleOBJTooltip", "Export the static geometry for the current sublevel to a .obj file."),
                        FSlateIcon(),
                        FUIAction(FExecuteAction::CreateRaw(this, &FSteamAudioEditorModule::OnExportStaticGeometryToOBJCurrentLevel))
                    );
                    MenuBuilder.EndSection();

                    MenuBuilder.BeginSection("DynamicGeometry", NSLOCTEXT("SteamAudio", "MenuDynamicGeometry", "Dynamic Objects"));
                    MenuBuilder.AddMenuEntry(
                        NSLOCTEXT("SteamAudio", "MenuExportDynamic", "Export All Dynamic Objects"),
                        NSLOCTEXT("SteamAudio", "MenuExportDynamicTooltip", "Export all dynamic objects in all sublevels."),
                        FSlateIcon(),
                        FUIAction(FExecuteAction::CreateRaw(this, &FSteamAudioEditorModule::OnExportDynamicObjects))
                    );
                    MenuBuilder.AddMenuEntry(
                        NSLOCTEXT("SteamAudio", "MenuExportDynamicSingle", "Export All Dynamic Objects (Current Level)"),
                        NSLOCTEXT("SteamAudio", "MenuExportDynamicSingleTooltip", "Export all dynamic objects in the current sublevel."),
                        FSlateIcon(),
                        FUIAction(FExecuteAction::CreateRaw(this, &FSteamAudioEditorModule::OnExportDynamicObjectsCurrentLevel))
                    );
                    MenuBuilder.EndSection();

                    MenuBuilder.BeginSection("Baking", NSLOCTEXT("SteamAudio", "MenuBaking", "Baking"));
                    MenuBuilder.AddMenuEntry(
                        NSLOCTEXT("SteamAudio", "MenuBake", "Bake Indirect Sound..."),
                        NSLOCTEXT("SteamAudio", "MenuBakeTooltip", "Bake reflections, reverb, and pathing."),
                        FSlateIcon(),
                        FUIAction(FExecuteAction::CreateRaw(this, &FSteamAudioEditorModule::OnBake))
                    );
                    MenuBuilder.EndSection();

                    return MenuBuilder.MakeWidget();
                }),
                TAttribute<FText>::Create([this]() { return NSLOCTEXT("SteamAudio", "SteamAudio", "Steam Audio"); }),
                TAttribute<FText>::Create([this]() { return NSLOCTEXT("SteamAudio", "SteamAudioTooltip", "Commands related to Steam Audio geometry export and baking."); }),
                FSlateIcon(SteamAudioStyleSet->GetStyleSetName(), "LevelEditor.SteamAudioMode", "LevelEditor.SteamAudioMode.Small")
            );
        }));

        LevelEditorModule.GetToolBarExtensibilityManager()->AddExtender(Extender);
    }
}

void FSteamAudioEditorModule::ShutdownModule()
{
	// Unregister component visualizers
    if (GUnrealEd)
    {
        for (FName& ClassName : RegisteredComponentClassNames)
        {
            GUnrealEd->UnregisterComponentVisualizer(ClassName);
        }
    }
}

void FSteamAudioEditorModule::RegisterComponentVisualizer(FName ComponentClassName, TSharedPtr<FComponentVisualizer> Visualizer)
{
    if (GUnrealEd)
    {
        GUnrealEd->RegisterComponentVisualizer(ComponentClassName, Visualizer);
    }

    RegisteredComponentClassNames.Add(ComponentClassName);

    if (Visualizer.IsValid())
    {
        Visualizer->OnRegister();
    }
}

void FSteamAudioEditorModule::OnAddGeometryComponentToStaticMeshes()
{
    UWorld* World = GEditor->GetLevelViewportClients()[0]->GetWorld();

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        TArray<UStaticMeshComponent*> StaticMeshComponents;
        It->GetComponents<UStaticMeshComponent>(StaticMeshComponents);

        bool bCanAffectAudio = false;
        for (UStaticMeshComponent* StaticMeshComponent : StaticMeshComponents)
        {
            // Check if this static mesh can affect audio.
            if (StaticMeshComponent && StaticMeshComponent->IsValidLowLevel() && !StaticMeshComponent->IsVisualizationComponent())
            {
                bCanAffectAudio = true;
                break;
            }
        }

        if (bCanAffectAudio)
        {
            USteamAudioGeometryComponent* GeometryComponent = It->FindComponentByClass<USteamAudioGeometryComponent>();
            if (!GeometryComponent)
            {
                GeometryComponent = NewObject<USteamAudioGeometryComponent>(*It);
                GeometryComponent->RegisterComponent();
                It->AddInstanceComponent(GeometryComponent);
            }
        }
    }
}

void FSteamAudioEditorModule::OnRemoveGeometryComponentFromStaticMeshes()
{
    UWorld* World = GEditor->GetLevelViewportClients()[0]->GetWorld();

    for (TObjectIterator<USteamAudioGeometryComponent> It; It; ++It)
    {
        if (It && It->IsValidLowLevel() && It->GetWorld() == World)
        {
            It->DestroyComponent();
        }
    }
}

void FSteamAudioEditorModule::OnExportStaticGeometry()
{
    UWorld* World = GEditor->GetLevelViewportClients()[0]->GetWorld();

    ExportAllLevels(World, false);
}

void FSteamAudioEditorModule::OnExportStaticGeometryCurrentLevel()
{
    UWorld* World = GEditor->GetLevelViewportClients()[0]->GetWorld();
    ULevel* Level = World->GetCurrentLevel();

    ExportSingleLevel(World, Level, false);
}

void FSteamAudioEditorModule::OnExportStaticGeometryToOBJ()
{
    UWorld* World = GEditor->GetLevelViewportClients()[0]->GetWorld();

    ExportAllLevels(World, true);
}

void FSteamAudioEditorModule::OnExportStaticGeometryToOBJCurrentLevel()
{
    UWorld* World = GEditor->GetLevelViewportClients()[0]->GetWorld();
    ULevel* Level = World->GetCurrentLevel();

    ExportSingleLevel(World, Level, true);
}

void FSteamAudioEditorModule::OnExportDynamicObjects()
{
    UWorld* World = GEditor->GetLevelViewportClients()[0]->GetWorld();

    NotifyStarting(NSLOCTEXT("SteamAudio", "ExportDynamicMulti", "Exporting dynamic objects..."));

    TArray<USteamAudioDynamicObjectComponent*> DynamicObjects;

    for (TObjectIterator<USteamAudioDynamicObjectComponent> It; It; ++It)
    {
        if (It->GetWorld() != World)
            continue;

        if (!It->Asset.IsAsset())
            continue;

        if (!It->GetOwner()->GetInstanceComponents().Contains(Cast<UActorComponent>(*It)))
            continue;

        DynamicObjects.Add(*It);
    }

    if (DynamicObjects.Num() > 0)
    {
        Async(EAsyncExecution::Thread, [&, DynamicObjects]()
        {
            int NumFailed = 0;

            for (USteamAudioDynamicObjectComponent* DynamicObject : DynamicObjects)
            {
                NotifyUpdate(FText::FormatOrdered(NSLOCTEXT("SteamAudio", "ExportDynamicMultiAllLevelsUpdate", "Level: {0}\nDynamic Object: {1}\nExporting..."),
                    FText::FromString(DynamicObject->GetComponentLevel()->GetOutermostObject()->GetName()),
                    FText::FromString(DynamicObject->GetOwner()->GetName())));

                if (!ExportDynamicObject(DynamicObject, DynamicObject->Asset.GetAssetPathString()))
                {
                    NumFailed++;
                    UE_LOG(LogSteamAudioEditor, Error, TEXT("Failed to export dynamic object %s in level %s."),
                        *DynamicObject->GetOwner()->GetName(),
                        *DynamicObject->GetComponentLevel()->GetOutermostObject()->GetName());
                }
            }

            if (NumFailed > 0)
            {
                NotifyFailed(FText::FormatOrdered(NSLOCTEXT("SteamAudio", "ExportDynamicMultiFail", "Failed to export {0} dynamic object(s)."),
                    FText::AsNumber(NumFailed)));
            }
            else
            {
                NotifySucceeded(FText::FormatOrdered(NSLOCTEXT("SteamAudio", "ExportDynamicMultiSuccess", "Exported {0} dynamic object(s)."),
                    FText::AsNumber(DynamicObjects.Num())));
            }
        });
    }
    else
    {
        NotifyFailed(NSLOCTEXT("SteamAudio", "ExportDynamicMultiNoObjects", "No dynamic objects found."));
    }
}

void FSteamAudioEditorModule::OnExportDynamicObjectsCurrentLevel()
{
    UWorld* World = GEditor->GetLevelViewportClients()[0]->GetWorld();
    ULevel* Level = World->GetCurrentLevel();

    NotifyStarting(NSLOCTEXT("SteamAudio", "ExportDynamicMulti", "Exporting dynamic objects..."));

    TArray<USteamAudioDynamicObjectComponent*> DynamicObjects;

    for (TObjectIterator<USteamAudioDynamicObjectComponent> It; It; ++It)
    {
        if (It->GetWorld() != World || It->GetComponentLevel() != Level)
            continue;

        if (!It->Asset.IsAsset())
            continue;

        if (!It->GetOwner()->GetInstanceComponents().Contains(Cast<UActorComponent>(*It)))
            continue;

        DynamicObjects.Add(*It);
    }

    if (DynamicObjects.Num() > 0)
    {
        Async(EAsyncExecution::Thread, [&, DynamicObjects]()
        {
            int NumFailed = 0;

            for (USteamAudioDynamicObjectComponent* DynamicObject : DynamicObjects)
            {
                NotifyUpdate(FText::FormatOrdered(NSLOCTEXT("SteamAudio", "ExportDynamicMultiUpdate", "Dynamic Object: {0}\nExporting..."),
                    FText::FromString(DynamicObject->GetOwner()->GetName())));

                if (!ExportDynamicObject(DynamicObject, DynamicObject->Asset.GetAssetPathString()))
                {
                    NumFailed++;
                    UE_LOG(LogSteamAudioEditor, Error, TEXT("Failed to export dynamic object %s."),
                        *DynamicObject->GetOwner()->GetName());
                }
            }

            if (NumFailed > 0)
            {
                NotifyFailed(FText::FormatOrdered(NSLOCTEXT("SteamAudio", "ExportDynamicMultiFail", "Failed to export {0} dynamic object(s)."),
                    FText::AsNumber(NumFailed)));
            }
            else
            {
                NotifySucceeded(FText::FormatOrdered(NSLOCTEXT("SteamAudio", "ExportDynamicMultiSuccess", "Exported {0} dynamic object(s)."),
                    FText::AsNumber(DynamicObjects.Num())));
            }
        });
    }
    else
    {
        NotifyFailed(NSLOCTEXT("SteamAudio", "ExportDynamicMultiNoObjects", "No dynamic objects found."));
    }
}

void FSteamAudioEditorModule::OnBake()
{
    BakeWindow->Invoke();
}

void FSteamAudioEditorModule::ExportSingleLevel(UWorld* World, ULevel* Level, bool bExportOBJ)
{
    FString Name;
    bool bNameChosen = PromptForSingleLevelName(World, Level, bExportOBJ, Name);

    if (bNameChosen)
    {
        NotifyStarting(NSLOCTEXT("SteamAudio", "ExportStatic", "Exporting static geometry..."));

        Async(EAsyncExecution::Thread, [&, World, Level, Name, bExportOBJ]()
        {
            if (!ExportStaticGeometryForLevel(World, Level, Name, bExportOBJ))
            {
                NotifyFailed(NSLOCTEXT("SteamAudio", "ExportStaticFail", "Failed to export static geometry."));
            }
            else
            {
                NotifySucceeded(NSLOCTEXT("SteamAudio", "ExportStaticSuccess", "Exported static geometry."));
            }
        });
    }
}

void FSteamAudioEditorModule::ExportAllLevels(UWorld* World, bool bExportOBJ)
{
    TMap<ULevel*, FString> Names;
    PromptForAllLevelNames(World, bExportOBJ, Names);

    if (Names.Num() > 0)
    {
        NotifyStarting(NSLOCTEXT("SteamAudio", "ExportStatic", "Exporting static geometry..."));

        Async(EAsyncExecution::Thread, [&, World, bExportOBJ, Names]()
        {
            int NumFailed = 0;

            for (ULevel* Level : World->GetLevels())
            {
                FString LevelName;
                Level->GetOutermostObject()->GetName(LevelName);

                if (Names.Contains(Level))
                {
                    if (!ExportStaticGeometryForLevel(World, Level, Names[Level], bExportOBJ))
                    {
                        NumFailed++;
                        UE_LOG(LogSteamAudioEditor, Error, TEXT("Failed to export static geometry for level %s."), *LevelName);
                    }
                }
                else
                {
                    UE_LOG(LogSteamAudioEditor, Warning, TEXT("No file name specified for level %s, skipping export."), *LevelName);
                }
            }

            if (NumFailed > 0)
            {
                NotifyFailed(FText::FormatOrdered(NSLOCTEXT("SteamAudio", "ExportStaticFailAllLevels", "Failed to export static geometry for {0} levels."),
                    FText::AsNumber(NumFailed)));
            }
            else
            {
                NotifySucceeded(NSLOCTEXT("SteamAudio", "ExportStaticSuccess", "Exported static geometry."));
            }
        });
    }
}

bool FSteamAudioEditorModule::PromptForFileName(UWorld* World, ULevel* Level, FString& FileName)
{
    IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
    if (DesktopPlatform)
    {
        TArray<FString> FileNames;
        bool bFileChosen = DesktopPlatform->SaveFileDialog(
            FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
            "Save as OBJ...",
            FPaths::ProjectContentDir(),
            Level->GetOutermostObject()->GetName() + ".obj",
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

bool FSteamAudioEditorModule::PromptForAssetName(UWorld* World, ULevel* Level, FString& PackageName)
{
    // See if there already is a Steam Audio Static Mesh actor in the level.
    ASteamAudioStaticMeshActor* SteamAudioStaticMeshActor = nullptr;
    for (TActorIterator<ASteamAudioStaticMeshActor> It(World); It; ++It)
    {
        if (It->GetLevel() == Level)
        {
            SteamAudioStaticMeshActor = *It;
            break;
        }
    }

    // If we found a Steam Audio Static Mesh actor, and it points to some asset, use that asset's asset path.
    if (SteamAudioStaticMeshActor && SteamAudioStaticMeshActor->Asset.IsValid())
    {
        PackageName = SteamAudioStaticMeshActor->Asset.GetAssetPathString();
        return true;
    }

    // We didn't find a Steam Audio Static Mesh actor, so prompt the user to create a new .uasset.
    IContentBrowserSingleton& ContentBrowser = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get();

    FSaveAssetDialogConfig DialogConfig{};
    DialogConfig.DialogTitleOverride = NSLOCTEXT("SteamAudio", "SaveStaticMesh", "Save static mesh as...");
    DialogConfig.DefaultPath = TEXT("/Game");
    DialogConfig.DefaultAssetName = Level->GetOutermostObject()->GetName() + "_StaticGeometry";
    DialogConfig.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::AllowButWarn;
    FString AssetPath = ContentBrowser.CreateModalSaveAssetDialog(DialogConfig);
    if (!AssetPath.IsEmpty())
    {
        PackageName = AssetPath;
        return true;
    }

    return false;
}

bool FSteamAudioEditorModule::PromptForSingleLevelName(UWorld* World, ULevel* Level, bool bExportOBJ, FString& Name)
{
    return (bExportOBJ) ? PromptForFileName(World, Level, Name) : PromptForAssetName(World, Level, Name);
}

void FSteamAudioEditorModule::PromptForAllLevelNames(UWorld* World, bool bExportOBJ, TMap<ULevel*, FString>& Names)
{
    for (ULevel* Level : World->GetLevels())
    {
        if (DoesLevelHaveStaticGeometryForExport(World, Level))
        {
            FString Name;
            if (PromptForSingleLevelName(World, Level, bExportOBJ, Name))
            {
                Names.Add(Level, Name);
            }
        }
        else
        {
            FString LevelName;
            Level->GetOutermostObject()->GetName(LevelName);

            UE_LOG(LogSteamAudioEditor, Warning, TEXT("No static geometry present in level %s, skipping export."), *LevelName);
        }
    }
}

void FSteamAudioEditorModule::NotifyStarting(const FText& Message)
{
    GEdModeTickable->SetDisplayText(Message);
    GEdModeTickable->CreateNotification();
}

void FSteamAudioEditorModule::NotifyStartingWithCancel(const FText& Message, TDelegate<void()> OnCancel)
{
    GEdModeTickable->SetDisplayText(Message);
    GEdModeTickable->CreateNotificationWithCancel(OnCancel);
}

void FSteamAudioEditorModule::NotifyUpdate(const FText& Message)
{
    GEdModeTickable->SetDisplayText(Message);
}

void FSteamAudioEditorModule::NotifyFailed(const FText& Message)
{
    GEdModeTickable->QueueWorkItem(FWorkItem([Message](FText& DisplayText)
    {
        DisplayText = Message;
    }, SNotificationItem::CS_Fail, true));
}

void FSteamAudioEditorModule::NotifySucceeded(const FText& Message)
{
    GEdModeTickable->QueueWorkItem(FWorkItem([Message](FText& DisplayText)
    {
        DisplayText = Message;
    }, SNotificationItem::CS_Success, true));
}

}
