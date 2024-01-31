//
// Copyright 2017-2023 Valve Corporation.
//

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSteamAudioEditor, Log, All);

class FComponentVisualizer;
class FSlateStyleSet;

namespace SteamAudio {

class FBakeWindow;

// ---------------------------------------------------------------------------------------------------------------------
// FSteamAudioEditorModule
// ---------------------------------------------------------------------------------------------------------------------

// Singleton class that contains all the global state related to the Steam Audio editor module.
class FSteamAudioEditorModule : public IModuleInterface
{
public:
    //
    // Inherited from IModuleInterface
    //

    // Called when the module is being loaded.
    virtual void StartupModule() override;

    // Called when the module is being unloaded.
    virtual void ShutdownModule() override;

    static void NotifyStarting(const FText& Message);
    static void NotifyStartingWithCancel(const FText& Message, TDelegate<void()> OnCancel);
    static void NotifyUpdate(const FText& Message);
    static void NotifyFailed(const FText& Message);
    static void NotifySucceeded(const FText& Message);

private:
    TSharedPtr<FSlateStyleSet> SteamAudioStyleSet;
    TArray<TSharedPtr<FAssetTypeActions_Base>> AssetTypeActions; // Metadata objects for each type of Steam Audio asset that can be created.
	TArray<FName> RegisteredComponentClassNames;
    TSharedPtr<FBakeWindow> BakeWindow;

    void RegisterComponentVisualizer(FName ComponentClassName, TSharedPtr<FComponentVisualizer> Visualizer);

    void OnAddGeometryComponentToStaticMeshes();
    void OnRemoveGeometryComponentFromStaticMeshes();
    void OnExportStaticGeometry();
    void OnExportStaticGeometryCurrentLevel();
    void OnExportStaticGeometryToOBJ();
    void OnExportStaticGeometryToOBJCurrentLevel();
    void OnExportDynamicObjects();
    void OnExportDynamicObjectsCurrentLevel();
    void OnBake();

    void ExportSingleLevel(UWorld* World, ULevel* Level, bool bExportOBJ);
    void ExportAllLevels(UWorld* World, bool bExportOBJ);

    bool PromptForFileName(UWorld* World, ULevel* Level, FString& FileName);
    bool PromptForAssetName(UWorld* World, ULevel* Level, FString& PackageName);
    bool PromptForSingleLevelName(UWorld* World, ULevel* Level, bool bExportOBJ, FString& Name);
    void PromptForAllLevelNames(UWorld* World, bool bExportOBJ, TMap<ULevel*, FString>& Names);
};

}
