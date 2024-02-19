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
